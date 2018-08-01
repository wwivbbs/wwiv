/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5                            */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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

#include <atomic>
#include <cctype>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include <signal.h>
#include <string>
#include <sys/types.h>
#include <vector>

#ifdef _WIN32

#include <WS2tcpip.h>
#include <process.h>

#else // _WIN32

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#endif // __linux__

#include <cereal/access.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "core/command_line.h"
#include "core/file.h"
#include "core/http_server.h"
#include "core/inifile.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/semaphore_file.h"
#include "core/socket_connection.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "core/wwivport.h"
#include "sdk/config.h"
#include "core/datetime.h"
#include "wwivd/connection_data.h"
#include "wwivd/nets.h"
#include "wwivd/node_manager.h"
#include "wwivd/wwivd.h"
#include "wwivd/wwivd_http.h"
#include "wwivd/wwivd_non_http.h"

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

namespace wwiv {
namespace wwivd {

extern std::atomic<bool> need_to_exit;
extern std::atomic<bool> need_to_reload_config;

static bool DeleteAllSemaphores(const Config& config, int start_node, int end_node) {
  // Delete telnet/SSH node semaphore files.
  for (int i = start_node; i <= end_node; i++) {
    File semaphore_file(node_file(config, ConnectionType::TELNET, i));
    if (semaphore_file.Exists()) {
      semaphore_file.Delete();
    }
  }

  // Delete any BINKP semaphores.
  File binkp_file(node_file(config, ConnectionType::BINKP, 0));
  if (binkp_file.Exists()) {
    binkp_file.Delete();
  }

  return true;
}

/**
 *  This program is the manager of the nodes for the WWIV BBS software
 *  on UNIX platforms.
 */
int Main(CommandLine& cmdline) {
  auto wwiv_dir = environment_variable("WWIV_DIR");
  if (wwiv_dir.empty()) {
    wwiv_dir = cmdline.arg("bbsdir").as_string();
  }
  VLOG(2) << "Using WWIV_DIR: " << wwiv_dir;

  auto wwiv_user = environment_variable("WWIV_USER");
  if (wwiv_user.empty()) {
    wwiv_user = cmdline.arg("wwiv_user").as_string();
    VLOG(2) << "Using WWIV_USER(cmdline): " << wwiv_user;
  } else {
    VLOG(2) << "Using WWIV_USER(env): " << wwiv_user;
  }

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
    File goodip(config.datadir(), "goodip.txt");
    data.good_ips_ = std::make_shared<GoodIp>(goodip.full_pathname());
  }
  if (c.blocking.use_badip_txt) {
    File badip(config.datadir(), "badip.txt");
    data.bad_ips_ = std::make_shared<BadIp>(badip.full_pathname());
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
  return EXIT_FAILURE;
}

} // namespace wwivd
} // namespace wwiv

int main(int argc, char* argv[]) {
  Logger::Init(argc, argv);
  ScopeExit at_exit(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");
  cmdline.AddStandardArgs();
  cmdline.add_argument({"wwiv_user", "WWIV User to use.", "wwiv"});
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
