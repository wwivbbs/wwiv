/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2016, WWIV Software Services             */
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
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/scope_exit.h"
#include "core/version.h"

#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "networkb/connection.h"
#include "networkb/net_util.h"
#include "networkb/socket_connection.h"
#include "networkb/socket_exceptions.h"
#include "networkb/wfile_transfer_file.h"

#include "sdk/callout.h"
#include "sdk/config.h"
#include "sdk/contact.h"
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
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

static void RegisterNetworkBCommands(CommandLine& cmdline) {
  cmdline.add_argument(BooleanCommandLineArgument("send", "Send network traffic to --node"));
  cmdline.add_argument(BooleanCommandLineArgument("receive", "Receive from any node"));
  cmdline.add_argument({"node", "Node number (only used when sending)", "0"});
  cmdline.add_argument({"handle", "Existing socket handle (only used when receiving)", "0"});
  cmdline.add_argument({"port", "Port number to use (receiving only)", "24554"});
}
  
static void ShowHelp(CommandLine& cmdline) {
  cout << cmdline.GetHelp() << endl;
}

static bool Receive(CommandLine& cmdline, BinkConfig& bink_config, int port) {
  BinkSide side = BinkSide::ANSWERING;
  bool loop = false;
  SOCKET sock = -1;
  bool socket_connected = false;
  if (cmdline.arg("handle").as_int()) {
    sock = static_cast<SOCKET>(cmdline.arg("handle").as_int());
    LOG(INFO) << "BinkP receive; existing socket; handle: " << sock;
    socket_connected = true;
  } else {
    sock = Listen(port);
    loop = true;
  }

  do {
    try {
      unique_ptr<SocketConnection> c;
      if (socket_connected) {
        c = Wrap(sock, port);
      } else {
        LOG(INFO) << "BinkP receive; listening on port: " << port;
        c = Accept(sock, port);
      }
      BinkP::received_transfer_file_factory_t factory = [&](const string& network_name, const string& filename) {
        const net_networks_rec& net = bink_config.networks()[network_name];
        File* f = new File(net.dir, filename);
        return new WFileTransferFile(filename, unique_ptr<File>(f));
      };
      BinkP binkp(c.get(), &bink_config, side, 0, factory);
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

static bool Send(CommandLine& cmdline, BinkConfig& bink_config, int port, int sendto_node, const std::string& network_name) {
  LOG(INFO) << "BinkP send to: " << sendto_node;
  const auto start_time = system_clock::now();

  const BinkNodeConfig* node_config = bink_config.node_config_for(sendto_node);
  if (node_config == nullptr) {
    LOG(ERROR) << "Unable to find node config for node: " << sendto_node;
    return false;
  }
  unique_ptr<SocketConnection> c;
  try {
    c = Connect(node_config->host, node_config->port);
  } catch (const connection_error& e) {
    const net_networks_rec& net = bink_config.networks()[network_name];
    Contact c(net.dir, true);
    c.add_failure(sendto_node, system_clock::to_time_t(start_time));
    throw e;
  }

  BinkP::received_transfer_file_factory_t factory = [&](const string& network_name, const string& filename) {
    const net_networks_rec& net = bink_config.networks()[network_name];
    File* f = new File(net.dir, filename);
    return new WFileTransferFile(filename, unique_ptr<File>(f));
  };
  BinkP binkp(c.get(), &bink_config, BinkSide::ORIGINATING, sendto_node, factory);
  binkp.Run();
  return true;
}

static int Main(CommandLine& cmdline, const NetworkCommandLine& net_cmdline) {
  try {
    int port = cmdline.arg("port").as_int();
    bool skip_net = cmdline.arg("skip_net").as_bool();

    StatusMgr sm(net_cmdline.config().datadir(), [](int) {});
    std::unique_ptr<WStatus> status(sm.GetStatus());

    const auto& net = net_cmdline.network();
    const auto& network_name = net_cmdline.network_name();

    int sendto_node = cmdline.arg("node").as_int();
    BinkConfig bink_config(network_name, net_cmdline.config(), net_cmdline.networks());

    File inifile(net_cmdline.config().root_directory(), "net.ini");
    if (inifile.Exists()) {
      const string ini_net_node = StrCat("networkb-", net_cmdline.network_name(), "-", sendto_node);
      const string ini_net = StrCat("networkb-", net_cmdline.network_name());
      IniFile ini(inifile.full_pathname(), {ini_net_node, ini_net, "networkb"});
      if (!bink_config.ProcessIniFile(ini)) {
        LOG(INFO) << "Unable to open INI file: " << inifile.full_pathname();
      }
    } else {
      // We don't want to warn now until we have usefull stuff in it.
      VLOG(1) << "FYI: INI file for this command doesn't exist; filename: " << inifile.full_pathname();
    }

    bink_config.set_skip_net(skip_net);
    bink_config.set_verbose(cmdline.iarg("v"));
    bink_config.set_network_version(status->GetNetworkVersion());

    for (const auto& n : bink_config.networks().networks()) {
      string lower_case_network_name(n.name);
      StringLowerCase(&lower_case_network_name);
      bink_config.callouts().emplace(lower_case_network_name, Callout(n.dir));
    }

    if (cmdline.arg("receive").as_bool()) {
      Receive(cmdline, bink_config, port);
    } else if (cmdline.arg("send").as_bool()) {
      if (Send(cmdline, bink_config, port, sendto_node, network_name)) {
        return 0;
      }
      return 1;
    } else {
      ShowHelp(cmdline);
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

  CommandLine cmdline(argc, argv, "net");
  RegisterNetworkBCommands(cmdline);
  NetworkCommandLine net_cmdline(cmdline);
  if (!net_cmdline.IsInitialized()) {
    ShowHelp(cmdline);
    return 1;
  }
  if (cmdline.help_requested()) {
    std::clog << "Help Requested." << endl;
    ShowHelp(cmdline);
    return 0;
  }

  return Main(cmdline, net_cmdline);
}