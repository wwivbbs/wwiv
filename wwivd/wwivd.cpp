/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5                            */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include "core/http_server.h"
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
#include <iostream>
#include <map>
#include <memory>
#include <signal.h>
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

using std::cerr;
using std::clog;
using std::cout;
using std::endl;
using std::map;
using std::string;
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
    const auto fn = node_file(config, ConnectionType::TELNET, i);
    if (File::Exists(fn)) {
      File::Remove(fn);
    }
  }

  // Delete any BINKP semaphores.
  const auto binkp_file = node_file(config, ConnectionType::BINKP, 0);
  if (File::Exists(binkp_file)) {
    File::Remove(binkp_file);
  }

  return true;
}

/**
 *  This program is the manager of the nodes for the WWIV BBS software
 *  on UNIX platforms.
 */
int Main(CommandLine& cmdline) {
  auto wwiv_dir = cmdline.bbsdir();
  VLOG(2) << "Using WWIV_DIR: " << wwiv_dir;

  auto wwiv_user = cmdline.arg("wwiv_user").as_string();
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
    nodes[b.name] =
        std::make_shared<NodeManager>(b.name, ConnectionType::TELNET, b.start_node, b.end_node);
  }
  // Add node manager for binkp.
  nodes["BINKP"] = std::make_shared<NodeManager>("BINKP", ConnectionType::BINKP, 0, 0);

  for (const auto& n : nodes) {
    if (!DeleteAllSemaphores(config, n.second->start_node(), n.second->end_node())) {
      LOG(ERROR) << "Unable to clear semaphores.";
    }
  }

  auto concurrent_connections =
      std::make_shared<ConcurrentConnections>(c.blocking.max_concurrent_sessions);

  ConnectionData data(&config, &c, &nodes, concurrent_connections);
  if (c.blocking.use_goodip_txt) {
    data.good_ips_ = std::make_shared<GoodIp>(PathFilePath(config.datadir(), "goodip.txt"));
  }
  if (c.blocking.use_badip_txt) {
    data.bad_ips_ = std::make_shared<BadIp>(PathFilePath(config.datadir(), "badip.txt"));
  }
  if (c.blocking.auto_blacklist) {
    if (!data.bad_ips_) {
      data.bad_ips_ = std::make_shared<BadIp>(PathFilePath(config.datadir(), "badip.txt"));
    }
    data.auto_blocker_ = std::make_shared<AutoBlocker>(data.bad_ips_, c.blocking);
  }

  auto telnet_or_ssh_fn = [&](accepted_socket_t r) {
    std::thread client(HandleConnection, std::make_unique<ConnectionHandler>(data, r));
    client.detach();
  };
  auto binkp_fn = [&](accepted_socket_t r) {
    std::thread client(HandleBinkPConnection, std::make_unique<ConnectionHandler>(data, r));
    client.detach();
  };
  auto http_fn = [&](accepted_socket_t r) {
    std::thread client(HandleHttpConnection, data, r);
    client.detach();
  };

  SocketSet sockets;
  if (c.telnet_port > 0) {
    sockets.add(c.telnet_port, telnet_or_ssh_fn, "TELNET");
  }
  if (c.ssh_port > 0) {
    sockets.add(c.ssh_port, telnet_or_ssh_fn, "SSH");
  }
  if (c.binkp_port > 0) {
    sockets.add(c.binkp_port, binkp_fn, "BINKP");
  }
  if (c.http_port > 0) {
    sockets.add(c.http_port, http_fn, "HTTP");
    // TODO(rushfan):
    // http_address;
  }

  SwitchToNonRootUser(wwiv_user);
  need_to_exit.store(false);
  need_to_reload_config.store(false);

  // Do network callouts if enabled.
  do_wwivd_callouts(config, c);

  if (!sockets.Run(need_to_exit)) {
    LOG(INFO) << "Error accepting client socket. " << errno;
    return 2;
  }
  return EXIT_SUCCESS;
}

} // namespace wwiv

int main(int argc, char* argv[]) {
  LoggerConfig config(LogDirFromConfig);
  Logger::Init(argc, argv, config);

  ScopeExit at_exit(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");
  cmdline.AddStandardArgs();
  cmdline.add_argument({"wwiv_user", "WWIV User to use.", "wwiv", "WWIV_USER"});
  cmdline.add_argument(BooleanCommandLineArgument{"version", 'V', "Display version.", false});
  cmdline.set_no_args_allowed(true);

  if (!cmdline.Parse()) {
    cout << cmdline.GetHelp() << endl;
    return EXIT_FAILURE;
  }
  if (cmdline.help_requested()) {
    cout << cmdline.GetHelp() << endl;
    return EXIT_SUCCESS;
  }
  if (cmdline.barg("version")) {
    cout << wwiv_version << beta_version << std::endl;
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
