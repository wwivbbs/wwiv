/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5                            */
/*             Copyright (C)1998-2022, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include "wwivd/wwivd.h"

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "sdk/config.h"
#include "wwivd/connection_data.h"
#include "wwivd/nets.h"
#include "wwivd/node_manager.h"
#include "wwivd/wwivd_http.h"
#include "wwivd/wwivd_non_http.h"
#include <atomic>
#include <csignal>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <WS2tcpip.h>
#else // _WIN32
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#endif // __linux__

#include "httplib.h"

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

#ifdef DELETE
#undef DELETE
#endif // DELETE

namespace wwiv::wwivd {

extern std::atomic<bool> need_to_exit;
extern std::atomic<bool> need_to_reload_config;

static bool DeleteAllSemaphores(const Config& config, int start_node, int end_node) {
  // Delete telnet/SSH node semaphore files.
  for (auto i = start_node; i <= end_node; i++) {
    if (const auto fn = node_file(config, ConnectionType::TELNET, i); File::Exists(fn)) {
      File::Remove(fn);
    }
  }

  if (const auto binkp_file = node_file(config, ConnectionType::BINKP, 0); File::Exists(binkp_file)) {
    File::Remove(binkp_file);
  }

  return true;
}

/**
 *  This program is the manager of the nodes for the WWIV BBS software
 *  on UNIX platforms.
 */
int Main(CommandLine& cmdline) {
  const auto wwiv_dir = cmdline.bbsdir();
  VLOG(2) << "Using WWIV_DIR: " << wwiv_dir;

  const auto wwiv_user = cmdline.arg("wwiv_user").as_string();
  VLOG(2) << "Using WWIV_USER: " << wwiv_user;

  const Config config{wwiv_dir};
  if (!config.IsInitialized()) {
    LOG(ERROR) << "Unable to load CONFIG.DAT";
    return EXIT_FAILURE;
  }

  wwivd_config_t c{};
  c.Load(config);
  File::set_current_directory(config.root_directory());
  LOG(INFO) << "Loaded BBSES:\r\n" << to_string(c.bbses);
  BeforeStartServer();

  std::map<const std::string, std::shared_ptr<NodeManager>> nodes;

  for (const auto& b : c.bbses) {
    nodes[b.name] = std::make_shared<NodeManager>(config, b);
  }
  // Add node manager for binkp.
  nodes["BINKP"] = std::make_shared<NodeManager>(config, ConnectionType::BINKP);

  // Add node for each BBS.
  for (const auto& n : nodes) {
    if (!DeleteAllSemaphores(config, n.second->start_node(), n.second->end_node())) {
      LOG(ERROR) << "Unable to clear semaphores.";
    }
  }

  const auto concurrent_connections =
      std::make_shared<ConcurrentConnections>(c.blocking.max_concurrent_sessions);

  ConnectionData data(&config, &c, &nodes, concurrent_connections);
  if (c.blocking.use_goodip_txt) {
    data.good_ips_ = std::make_shared<GoodIp>(FilePath(config.datadir(), "goodip.txt"));
  }
  // This needs to stay in scople for the lifetime of the AutoBlocker
  SystemClock clock;
  if (c.blocking.use_badip_txt) {
    data.bad_ips_ = std::make_shared<BadIp>(FilePath(config.datadir(), "badip.txt"), clock);
  }
  if (c.blocking.auto_blocklist) {
    if (!data.bad_ips_) {
      data.bad_ips_ = std::make_shared<BadIp>(FilePath(config.datadir(), "badip.txt"), clock);
    }
    data.auto_blocker_ = std::make_shared<AutoBlocker>(data.bad_ips_, c.blocking, config.datadir(), clock);
  }

  auto telnet_or_ssh_fn = [&](accepted_socket_t r) {
    std::thread client(HandleConnection, std::make_unique<ConnectionHandler>(data, r));
    client.detach();
  };
  auto binkp_fn = [&](accepted_socket_t r) {
    std::thread client(HandleBinkPConnection, std::make_unique<ConnectionHandler>(data, r));
    client.detach();
  };

  SocketSet sockets(10);
  if (c.telnet_port > 0) {
    sockets.add(c.telnet_port, telnet_or_ssh_fn, "TELNET");
  }
  if (c.ssh_port > 0) {
    sockets.add(c.ssh_port, telnet_or_ssh_fn, "SSH");
  }
  if (c.binkp_port > 0) {
    sockets.add(c.binkp_port, binkp_fn, "BINKP");
  }

  SwitchToNonRootUser(wwiv_user);
  need_to_exit.store(false);
  need_to_reload_config.store(false);

  // Do network callouts if enabled.
  auto& nodemgr = data.nodes->at("BINKP");
  do_wwivd_callouts(config, c, nodemgr);

  std::unique_ptr<httplib::Server> svr;
  std::thread srv_thread;
  if (c.http_port > 0) {
    using namespace std::placeholders;
    svr = std::make_unique<httplib::Server>();    
    svr->Get("/status", std::bind(StatusHandler, data.nodes, _1, _2));
    svr->set_logger(
        [](const httplib::Request& req, const httplib::Response& res) { VLOG(1) << res.body; });
    srv_thread = std::thread([&](const std::string http_address, int p) { 
      svr->listen(http_address, p); 
      }, c.http_address, c.http_port);
  }

  int result = EXIT_SUCCESS;
  if (!sockets.Run(need_to_exit)) {
    LOG(INFO) << "Error accepting client socket. " << errno;
    result = 2;
  }

  if (svr) {
    svr->stop();
    srv_thread.join();
  }

  return result;
}

} // namespace wwiv

int main(int argc, char* argv[]) {
  LoggerConfig config(LogDirFromConfig);
  Logger::Init(argc, argv, config);

  auto at_exit = finally(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");
  cmdline.AddStandardArgs();
  cmdline.add_argument({"wwiv_user", "WWIV User to use.", "wwiv", "WWIV_USER"});
  cmdline.add_argument(BooleanCommandLineArgument{"version", 'V', "Display version.", false});
  cmdline.set_no_args_allowed(true);

  if (!cmdline.Parse()) {
    std::cout << cmdline.GetHelp() << std::endl;
    return EXIT_FAILURE;
  }
  if (cmdline.help_requested()) {
    std::cout << cmdline.GetHelp() << std::endl;
    return EXIT_SUCCESS;
  }
  if (cmdline.barg("version")) {
    std::cout << full_version() << std::endl;
    return 0;
  }

  LOG(INFO) << "wwivd - WWIV Daemon.";

  try {
    return wwiv::wwivd::Main(cmdline);
  } catch (const std::exception& e) {
    LOG(ERROR) << "Caught top level exception: " << e.what();
    return EXIT_FAILURE;
  }
}
