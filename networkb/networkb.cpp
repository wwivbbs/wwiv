/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*               Copyright (C)2015, WWIV Software Services                */
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
#include "networkb/callout.h"
#include "networkb/connection.h"
#include "networkb/socket_connection.h"
#include "networkb/socket_exceptions.h"
#include "networkb/wfile_transfer_file.h"

#include "sdk/config.h"
#include "sdk/networks.h"

using std::cout;
using std::endl;
using std::exception;
using std::map;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

static void RegisterHelpCommands(CommandLine& cmdline) {
  cmdline.add(BooleanCommandLineArgument("help", '?', "Displys Help", false));
  cmdline.add({"bbsdir", "(optional) BBS directory if other than current directory", File::current_directory()});
  cmdline.add({"network", "Network name to use (i.e. wwivnet)"});
  cmdline.add(BooleanCommandLineArgument("send", "Send network traffic to --node"));
  cmdline.add(BooleanCommandLineArgument("receive", "Receive from any node"));
  cmdline.add({"node", "Node number (only used when sending)", "0"});
  cmdline.add({"port", "Port number to use (receiving only)", "24554"});
  cmdline.add(BooleanCommandLineArgument("skip_net", "Skip invoking network1/network2/network3"));
}
  
static void ShowHelp(CommandLine& cmdline) {
  cout << cmdline.GetHelp() << endl;
}

int main(int argc, char** argv) {
  try {
    Logger::Init(argc, argv);
    wwiv::core::ScopeExit at_exit(Logger::ExitLogger);

    CommandLine cmdline(argc, argv, "network_number");
    RegisterHelpCommands(cmdline);
    if (!cmdline.Parse()) {
      ShowHelp(cmdline);
      return 1;
    }
    if (cmdline.arg("help").as_bool()) {
      ShowHelp(cmdline);
      return 0;
    }
    
    string network_name = cmdline.arg("network").as_string();
    StringLowerCase(&network_name);
    int port = cmdline.arg("port").as_int();
    bool skip_net = cmdline.arg("skip_net").as_bool();
    string bbsdir = cmdline.arg("bbsdir").as_string();

    Config config(bbsdir);
    if (!config.IsInitialized()) {
      LOG << "Unable to load CONFIG.DAT.";
      return 1;
    }
    Networks networks(config);
    if (!networks.IsInitialized()) {
      LOG << "Unable to load networks.";
      return 1;
    }

    int expected_remote_node = cmdline.arg("node").as_int();
    BinkConfig bink_config(network_name, config, networks);
    bink_config.set_skip_net(skip_net);
    unique_ptr<SocketConnection> c;
    BinkSide side = BinkSide::ORIGINATING;

    map<const string, Callout> callouts;
    for (const auto net : bink_config.networks().networks()) {
      string lower_case_network_name(net.name);
      StringLowerCase(&lower_case_network_name);
      callouts.emplace(lower_case_network_name, Callout(net.dir));
    }

    if (cmdline.arg("receive").as_bool()) {
      LOG << "BinkP receive";
      side = BinkSide::ANSWERING;
      c = Accept(port);

    } else if (cmdline.arg("send").as_bool()) {
      LOG << "BinkP send to: " << expected_remote_node;

      if (cmdline.arg("network").as_string().empty()) {
        LOG << "--network=[network name] must be specified.";
        ShowHelp(cmdline);
        return 1;
      }

      const BinkNodeConfig* node_config = bink_config.node_config_for(expected_remote_node);
      if (node_config == nullptr) {
        LOG << "Unable to find node config for node: " << expected_remote_node;
        return 2;
      }

      c = Connect(node_config->host, node_config->port);
    } else {
      ShowHelp(cmdline);
      return 1;
    } 

    BinkP::received_transfer_file_factory_t factory = [&](const string& network_name, const string& filename) { 
      const net_networks_rec& net = bink_config.networks()[network_name];
      File* f = new File(net.dir, filename);
      return new WFileTransferFile(filename, unique_ptr<File>(f)); 
    };
    BinkP binkp(c.get(), &bink_config, callouts, side, expected_remote_node, factory);
    binkp.Run();
  } catch (const socket_error& e) {
    LOG << "ERROR: [networkb]: " << e.what() << "\n";
  } catch (const exception& e) {
    LOG << "ERROR: [networkb]: " << e.what() << "\nStacktrace:\n" << stacktrace();
  }
}
