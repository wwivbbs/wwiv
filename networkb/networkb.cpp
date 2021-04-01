/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#include "binkp/binkp.h"
#include "binkp/binkp_config.h"
#include "binkp/config_exceptions.h"
#include "binkp/wfile_transfer_file.h"
#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/semaphore_file.h"
#include "core/socket_connection.h"
#include "core/socket_exceptions.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "net_core/net_cmdline.h"
#include "sdk/config.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/net/callout.h"
#include "sdk/net/contact.h"
#include "sdk/net/networks.h"
#include "sdk/status.h"
#include <chrono>
#include <csignal>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace std::chrono;
using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::sdk;
using namespace wwiv::sdk::fido;
using namespace wwiv::sdk::net;
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

static void ShowHelp(const NetworkCommandLine& cmdline) {
  std::cout << cmdline.GetHelp() << std::endl;
  exit(1);
}

static bool Receive(const CommandLine& cmdline, BinkConfig& bink_config, int port) {
  auto side = BinkSide::ANSWERING;
  auto loop = false;
  auto sock = INVALID_SOCKET;
  auto socket_connected = false;
  if (cmdline.iarg("handle")) {
    sock = static_cast<SOCKET>(cmdline.iarg("handle"));
    LOG(INFO) << "BinkP receive; existing socket; handle: " << sock;
    socket_connected = true;
  } else {
    sock = CreateListenSocket(port);
    loop = cmdline.barg("daemon");
  }

  do {  // NOLINT(bugprone-infinite-loop)
    try {
      std::string ip;
      if (GetRemotePeerAddress(sock, ip)) {
        LOG(INFO) << "Received connection from: " << ip;
      }
      std::unique_ptr<SocketConnection> c;
      if (socket_connected) {
        c = std::make_unique<SocketConnection>(sock);
      } else {
        LOG(INFO) << "BinkP receive; listening on port: " << port;
        sockaddr_in saddr{};
        socklen_t addr_length = sizeof(saddr);
        auto s = accept(sock, reinterpret_cast<struct sockaddr*>(&saddr), &addr_length);
        c = std::make_unique<SocketConnection>(s);
      }
      BinkP::received_transfer_file_factory_t factory = [&](const std::string& network_name,
                                                            const std::string& filename) {
        const auto dir = bink_config.receive_dir(network_name);
        return new WFileTransferFile(filename, std::make_unique<File>(FilePath(dir, filename)));
      };
      BinkP binkp(c.get(), &bink_config, side, "0", factory);
      binkp.Run(cmdline);
    } catch (const connection_error& e) {
      LOG(ERROR) << "CONNECTION ERROR: [networkb]: " << e.what();
    } catch (const socket_error& e) {
      LOG(ERROR) << "SOCKET ERROR: [networkb]: " << e.what();
    } catch (const std::exception& e) {
      LOG(ERROR) << "ERROR: [networkb]: " << e.what();
    }
  } while (loop);
  return true;
}

static bool Send(const CommandLine& cmdline, BinkConfig& bink_config, const std::string& sendto_node,
                 const std::string& network_name) {
  LOG(INFO) << "BinkP send to: " << sendto_node;
  const auto start_time = system_clock::now();

  auto* node_config = bink_config.binkp_session_config_for(sendto_node);
  if (node_config == nullptr) {
    LOG(ERROR) << "Unable to find node config for node: " << sendto_node;
    return false;
  }
  std::unique_ptr<SocketConnection> c;
  try {
    c = Connect(node_config->host, node_config->port);
  } catch (const connection_error& e) {
    LOG(ERROR) << "Recording failure: '" << e.what() << "'";
    const auto& net = bink_config.networks()[network_name];
    Contact contact(net, true);

    auto dt = DateTime::from_time_t(system_clock::to_time_t(start_time));
    if (net.type == network_type_t::wwivnet) {
      auto wwivnet_node = to_number<int>(sendto_node);
      contact.add_failure(wwivnet_node, dt);
    } else {
      contact.add_failure(sendto_node, dt);
    }

    throw;
  }

  const auto& net = bink_config.networks()[network_name];
  BinkP::received_transfer_file_factory_t factory = [&](const std::string&, const std::string& filename) {
    const auto dir = bink_config.receive_dir(network_name);
    return new WFileTransferFile(filename, std::make_unique<File>(FilePath(dir, filename)));
  };

  std::string sendto_ftn_node;
  if (net.type == network_type_t::wwivnet) {
    sendto_ftn_node = StrCat("20000:20000/", sendto_node, "@", network_name);
  } else if (net.type == network_type_t::ftn) {
    sendto_ftn_node = sendto_node;
  } else {
    throw config_error("BinkP only supports wwivnet or ftn networks.");
  }
  BinkP binkp(c.get(), &bink_config, BinkSide::ORIGINATING, sendto_ftn_node, factory);
  binkp.Run(cmdline);
  return true;
}

static int Main(const NetworkCommandLine& net_cmdline) {
  try {
    [[maybe_unused]] static bool initialized = wwiv::core::InitializeSockets();

    const auto port = net_cmdline.cmdline().iarg("port");
    const auto skip_net = net_cmdline.skip_net();

    StatusMgr sm(net_cmdline.config().datadir(), [](int) {});
    const auto status = sm.get_status();

    const auto& network_name = net_cmdline.network_name();

    const auto sendto_node = net_cmdline.cmdline().sarg("node");
    BinkConfig bink_config(network_name, net_cmdline.config(), net_cmdline.networks());

    bink_config.set_skip_net(skip_net);
    bink_config.set_verbose(net_cmdline.cmdline().verbose());
    bink_config.set_network_version(status->status_net_version());

    for (const auto& n : bink_config.networks().networks()) {
      auto domain = ToStringLowerCase(n.name);
      if (n.type == network_type_t::wwivnet) {
        auto c = std::make_unique<Callout>(n, bink_config.config().max_backups());
        for (const auto& kv : c->callout_config()) {
          if (const auto o = try_parse_fidoaddr(fmt::format("20000:20000/{}@{}", kv.first, domain))) {
            bink_config.address_pw_map.try_emplace(o.value(), kv.second.session_password);
          }
        }
        bink_config.callouts()[domain] = std::move(c);
      } else if (n.type == network_type_t::ftn) {
        if (auto o = try_parse_fidoaddr(n.fido.fido_address)) {
          if (o->has_domain()) {
            domain = o->domain();
          } else {
            LOG(ERROR) << "Domain is empty for FTN address. Please set it for: "
                       << n.fido.fido_address;
          }
          auto c = std::make_unique<FidoCallout>(net_cmdline.config(), n);
          for (const auto& kv : c->node_configs_map()) {
            bink_config.address_pw_map.try_emplace(kv.first, kv.second.binkp_config.password);
          }
          VLOG(1) << "Adding FidoCallout for network domain: " << domain;
          bink_config.callouts()[domain] = std::move(c);
        } else {
          LOG(WARNING) << "Unable to parse FTN Address for network: " << n.name;
          LOG(WARNING) << "Address: " << n.fido.fido_address;
        }
      }
    }

    if (net_cmdline.cmdline().barg("receive")) {
      Receive(net_cmdline.cmdline(), bink_config, port);
    } else if (net_cmdline.cmdline().barg("send")) {
      if (Send(net_cmdline.cmdline(), bink_config, sendto_node, network_name)) {
        VLOG(1) << "Normal Exit";
        return 0;
      }
      return 1;
    } else {
      ShowHelp(net_cmdline);
      return 1;
    }
  } catch (const connection_error& e) {
    LOG(ERROR) << "CONNECTION ERROR: [networkb]: " << e.what();
  } catch (const socket_error& e) {
    LOG(ERROR) << "SOCKET ERROR: [networkb]: " << e.what();
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [networkb]: " << e.what();
  } catch (...) {
    LOG(ERROR) << "ERROR: [networkb]: (Unknown)";
  }
  return 0;
}

int main(int argc, char** argv) {
  LoggerConfig config(LogDirFromConfig);
  Logger::Init(argc, argv, config);

  ScopeExit at_exit(Logger::ExitLogger);

#ifdef __unix__
  // Let the socket library handle EPIPE
  signal(SIGPIPE, SIG_IGN);
#endif // __unix__

  CommandLine cmdline(argc, argv, "net");
  RegisterNetworkBCommands(cmdline);
  const NetworkCommandLine net_cmdline(cmdline, 'b');
  if (!net_cmdline.IsInitialized() || net_cmdline.cmdline().help_requested()) {
    ShowHelp(net_cmdline);
    return 1;
  }
  try {
    auto semaphore = SemaphoreFile::try_acquire(net_cmdline.semaphore_path(),
                                                net_cmdline.semaphore_timeout());
    return Main(net_cmdline);
  } catch (const semaphore_not_acquired& e) {
    LOG(ERROR) << "ERROR: [network" << net_cmdline.net_cmd()
               << "]: Unable to Acquire Network Semaphore: " << e.what();
    return 3;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Caught uncaught exception: " << e.what();
    return 2;
  } catch (...) {
    LOG(ERROR) << "Caught uncaught throwable that doesn't extend std::exception: ";
    return 1;
  }
}
