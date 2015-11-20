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

// WWIV Network Redirector (network.exe)
#include <cctype>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/os.h"
#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "networkb/connection.h"
#include "networkb/ppp_config.h"

#include "sdk/config.h"
#include "sdk/networks.h"

using std::cout;
using std::endl;
using std::map;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::os;

static void ShowHelp(CommandLine& cmdline) {
  cout << cmdline.GetHelp()
       << "/N####     Network node number to dial." << endl
       << ".####      Network number (as defined in INIT)" << endl
       << endl;
  exit(1);
}

static int LaunchOldNetworkingStack(const std::string exe, int argc, char** argv) {
  std::ostringstream os;
  os << exe;
  for (int i = 1; i < argc; i++) {
    const string s(argv[1]);
    if (starts_with(s, "/")) {
      os << " " << argv[i];
    }
  }
  const string command_line = os.str();
  LOG << "Executing Command: '" << command_line << "'";
  return system(command_line.c_str());
}

int main(int argc, char** argv) {
  Logger::Init(argc, argv);
  try {
    ScopeExit at_exit(Logger::ExitLogger);
    CommandLine cmdline(argc, argv, "network_number");
    cmdline.add({"node", 'n', "Network node number to dial.", "0"});
    cmdline.add({"network", "Network name to use (i.e. wwivnet).", ""});
    cmdline.add({"network_number", "Network number to use (i.e. 0).", "0"});
    cmdline.add({"bbsdir", "(optional) BBS directory if other than current directory", File::current_directory()});
    cmdline.add(BooleanCommandLineArgument("help", '?', "displays help.", false));

    if (!cmdline.Parse() || cmdline.arg("help").as_bool()) {
      ShowHelp(cmdline);
      return 1;
    }
    string network_name = cmdline.arg("network").as_string();
    string network_number = cmdline.arg("network_number").as_string();
    if (network_name.empty() && network_number.empty()) {
      LOG << "--network=[network name] or .[network_number] must be specified.";
      ShowHelp(cmdline);
      return 1;
    }

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

    if (!network_number.empty() && network_name.empty()) {
      // Need to set the network name based on the number.
      network_name = networks[std::stoi(network_number)].name;
    }

    int expected_remote_node = cmdline.arg("node").as_int();
    if (expected_remote_node == 32767) {
      // 32767 is the PPP project address for "send everything". Some people use this
      // "magic" node number.
      LOG << "USE PPP Project to send to: Internet Email (@32767)";
      return LaunchOldNetworkingStack("networkp", argc, argv);
    }

    BinkConfig bink_config(network_name, config, networks);
    const BinkNodeConfig* node_config = bink_config.node_config_for(expected_remote_node);
    if (node_config != nullptr) {
      // We have a node configuration for this one, use networkb.
      LOG << "USE networkb: " << node_config->host << ":" << node_config->port;
      const string command_line = StringPrintf("networkb --send --network=%s --node=%d",
        network_name.c_str(), expected_remote_node);
      LOG << "Executing Command: '" << command_line << "'";
      return system(command_line.c_str());
    }

    PPPConfig ppp_config(network_name, config, networks);
    const PPPNodeConfig* ppp_node_config = ppp_config.node_config_for(expected_remote_node);
    if (ppp_node_config != nullptr) {
      LOG << "USE PPP Project to send to: " << ppp_node_config->email_address;
      return LaunchOldNetworkingStack("networkp", argc, argv);
    }

    // Use legacy networking.
    return LaunchOldNetworkingStack("network0", argc, argv);
  } catch (const std::exception& e) {
    LOG << "ERROR: [network]: " << e.what() << "\nStacktrace:\n";
    LOG << stacktrace();
  }
}
