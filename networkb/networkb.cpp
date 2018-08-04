/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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
/**************************************************************************/

// WWIV BINKP Network Stack. (networkb.exe)

#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/semaphore_file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"

#include "core/connection.h"
#include "core/socket_connection.h"
#include "core/socket_exceptions.h"
#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "networkb/net_util.h"
#include "networkb/wfile_transfer_file.h"

#include "sdk/callout.h"
#include "sdk/config.h"
#include "sdk/contact.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/networks.h"
#include "sdk/status.h"

using std::cout;
using std::endl;
using std::exception;
using std::map;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace std::chrono;
using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::sdk;
using namespace wwiv::sdk::fido;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

static void RegisterNetworkBCommands(CommandLine& cmdline) {
  cmdline.add_argument(BooleanCommandLineArgument("send", "Send network traffic to --node"));
  cmdline.add_argument(BooleanCommandLineArgument("receive", "Receive from any node"));
  cmdline.add_argument({"node", "Node number (only used when sending)", "0"});
  cmdline.add_argument({"handle", "Existing socket handle (only used when receiving)", "0"});
  cmdline.add_argument({"port", "Port number to use (receiving only)", "24554"});
  cmdline.add_argument(BooleanCommandLineArgument(
      "daemon", "Run continually as a daemon until stopped  (only used when receiving)", true));
}

static void ShowHelp(const CommandLine& cmdline) { cout << cmdline.GetHelp() << endl; }

static bool Receive(const CommandLine& cmdline, BinkConfig& bink_config, int port) {
  BinkSide side = BinkSide::ANSWERING;
  bool loop = false;
  SOCKET sock = INVALID_SOCKET;
  bool socket_connected = false;
  if (cmdline.iarg("handle")) {
    sock = static_cast<SOCKET>(cmdline.iarg("handle"));
    LOG(INFO) << "BinkP receive; existing socket; handle: " << sock;
    socket_connected = true;
  } else {
    sock = CreateListenSocket(port);
    loop = cmdline.barg("daemon");
  }

  do {
    try {
      string ip;
      if (wwiv::core::GetRemotePeerAddress(sock, ip)) {
        LOG(INFO) << "Received connection from: " << ip;
      }
      unique_ptr<SocketConnection> c;
      if (socket_connected) {
        c = std::make_unique<SocketConnection>(sock);
      } else {
        LOG(INFO) << "BinkP receive; listening on port: " << port;
        sockaddr_in saddr{};
        socklen_t addr_length = sizeof(saddr);
        SOCKET s = accept(sock, reinterpret_cast<struct sockaddr*>(&saddr), &addr_length);
        c = std::make_unique<SocketConnection>(s);
      }
      BinkP::received_transfer_file_factory_t factory = [&](const string& network_name,
                                                            const string& filename) {
        const net_networks_rec& net = bink_config.networks()[network_name];
        return new WFileTransferFile(filename, std::make_unique<File>(FilePath(net.dir, filename)));
      };
      BinkP binkp(c.get(), &bink_config, side, "0", factory);
      binkp.Run();
    } catch (const connection_error& e) {
      LOG(ERROR) << "CONNECTION ERROR: [networkb]: " << e.what();
    } catch (const socket_error& e) {
      LOG(ERROR) << "SOCKET ERROR: [networkb]: " << e.what();
    } catch (const exception& e) {
      LOG(ERROR) << "ERROR: [networkb]: " << e.what();
    }
  } while (loop);
  return true;
}

static bool Send(BinkConfig& bink_config, const string& sendto_node,
                 const std::string& network_name) {
  LOG(INFO) << "BinkP send to: " << sendto_node;
  const auto start_time = system_clock::now();

  const binkp_session_config_t* node_config = bink_config.binkp_session_config_for(sendto_node);
  if (node_config == nullptr) {
    LOG(ERROR) << "Unable to find node config for node: " << sendto_node;
    return false;
  }
  unique_ptr<SocketConnection> c;
  try {
    c = Connect(node_config->host, node_config->port);
  } catch (const connection_error& e) {
    LOG(ERROR) << e.what();
    const net_networks_rec& net = bink_config.networks()[network_name];
    Contact contact(net, true);

    LOG(ERROR) << "Recording failure";
    if (net.type == network_type_t::wwivnet) {
      auto wwivnet_node = to_number<uint16_t>(sendto_node);
      contact.add_failure(wwivnet_node, system_clock::to_time_t(start_time));
    } else {
      contact.add_failure(sendto_node, system_clock::to_time_t(start_time));
    }

    throw e;
  }

  const net_networks_rec& net = bink_config.networks()[network_name];
  BinkP::received_transfer_file_factory_t factory = [&](const string&, const string& filename) {
    return new WFileTransferFile(filename, std::make_unique<File>(FilePath(net.dir, filename)));
  };

  string sendto_ftn_node;
  if (net.type == network_type_t::wwivnet) {
    sendto_ftn_node = StrCat("20000:20000/", sendto_node, "@", network_name);
  } else if (net.type == network_type_t::ftn) {
    sendto_ftn_node = sendto_node;
  } else {
    throw config_error("BinkP only supports wwivnet or ftn networks.");
  }
  BinkP binkp(c.get(), &bink_config, BinkSide::ORIGINATING, sendto_ftn_node, factory);
  binkp.Run();
  return true;
}

static int Main(const NetworkCommandLine& net_cmdline) {
  try {
    static bool initialized = wwiv::core::InitializeSockets();

    int port = net_cmdline.cmdline().iarg("port");
    bool skip_net = net_cmdline.skip_net();

    StatusMgr sm(net_cmdline.config().datadir(), [](int) {});
    std::unique_ptr<WStatus> status(sm.GetStatus());

    const auto& network_name = net_cmdline.network_name();

    const string sendto_node = net_cmdline.cmdline().sarg("node");
    BinkConfig bink_config(network_name, net_cmdline.config(), net_cmdline.networks());

    bink_config.set_skip_net(skip_net);
    bink_config.set_verbose(net_cmdline.cmdline().iarg("v"));
    bink_config.set_network_version(status->GetNetworkVersion());

    for (const auto& n : bink_config.networks().networks()) {
      auto lower_case_network_name = ToStringLowerCase(n.name);
      if (n.type == network_type_t::wwivnet) {
        bink_config.callouts()[lower_case_network_name] = std::unique_ptr<Callout>(new Callout(n));
      } else if (n.type == network_type_t::ftn) {
        LOG(INFO) << "Adding FidoCallout for " << n.name;
        bink_config.callouts()[lower_case_network_name] =
            std::unique_ptr<Callout>(new FidoCallout(net_cmdline.config(), n));
      }
    }

    if (net_cmdline.cmdline().barg("receive")) {
      Receive(net_cmdline.cmdline(), bink_config, port);
    } else if (net_cmdline.cmdline().barg("send")) {
      if (Send(bink_config, sendto_node, network_name)) {
        return 0;
      }
      return 1;
    } else {
      ShowHelp(net_cmdline.cmdline());
      return 1;
    }
  } catch (const connection_error& e) {
    LOG(ERROR) << "CONNECTION ERROR: [networkb]: " << e.what();
  } catch (const socket_error& e) {
    LOG(ERROR) << "SOCKET ERROR: [networkb]: " << e.what();
  } catch (const exception& e) {
    LOG(ERROR) << "ERROR: [networkb]: " << e.what();
  }
  return 0;
}

int main(int argc, char** argv) {
  Logger::Init(argc, argv);
  wwiv::core::ScopeExit at_exit(Logger::ExitLogger);

#ifdef __unix__
  // Let the socket library handle EPIPE
  signal(SIGPIPE, SIG_IGN);
#endif // __unix__

  CommandLine cmdline(argc, argv, "net");
  RegisterNetworkBCommands(cmdline);
  NetworkCommandLine net_cmdline(cmdline, 'b');
  if (!net_cmdline.IsInitialized() || net_cmdline.cmdline().help_requested()) {
    ShowHelp(net_cmdline.cmdline());
    return 1;
  }
  try {
    auto semaphore = SemaphoreFile::try_acquire(net_cmdline.semaphore_filename(),
                                                net_cmdline.semaphore_timeout());
    return Main(net_cmdline);
  } catch (const semaphore_not_acquired& e) {
    LOG(ERROR) << "ERROR: [network" << net_cmdline.net_cmd()
               << "]: Unable to Acquire Network Semaphore: " << e.what();
  }
}