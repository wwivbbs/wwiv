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
#include "core/connection.h"
#include "networkb/net_util.h"
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
       << ".####      Network number (as defined in wwivconfig)" << endl
       << endl;
  exit(1);
}

static int LaunchOldNetworkingStack(const NetworkCommandLine& net_cmdline, const std::string exe, int argc, char** argv) {
  std::ostringstream os;
  os << FilePath(net_cmdline.bbsdir(), exe);
  for (int i = 1; i < argc; i++) {
    const string s(argv[1]);
    if (starts_with(s, "/")) {
      os << " " << argv[i];
    }
  }
  const string command_line = os.str();
  LOG(INFO) << "Executing Command: '" << command_line << "'";
  return system(command_line.c_str());
}


int main(int argc, char** argv) {
  Logger::Init(argc, argv);
  try {
    ScopeExit at_exit(Logger::ExitLogger);
    CommandLine cmdline(argc, argv, "net");
    cmdline.AddStandardArgs();
    AddStandardNetworkArgs(cmdline, File::current_directory());
    cmdline.add_argument({"node", 'n', "Network node number to dial.", "0"});
    cmdline.add_argument(BooleanCommandLineArgument("allow_sendback", 'A', "Allow sendback (only used by legacy network0)", true));
    cmdline.add_argument({"phone_number", 'P', "Network number to use (only used by legacy network0)", ""});
    cmdline.add_argument({"speed", 'S', "Modem Speedto use (only used by legacy network0)", ""});
    cmdline.add_argument({"callout_time", 'T', "Start time of the callout (only used by legacy network0)", ""});

    NetworkCommandLine net_cmdline(cmdline);
    if (!net_cmdline.IsInitialized() || cmdline.help_requested()) {
      ShowHelp(cmdline);
      return 1;
    }

    auto network_name = net_cmdline.network_name();
    LOG(INFO) << "NETWORK for network: " << network_name;

    auto node = cmdline.sarg("node");
    if (!contains(node, ':')) {
      int nodeint = to_number<int>(node);
      if (nodeint == 0 || nodeint > 32767) {
        LOG(ERROR) << "Invalid node number: '" << node << "' specified.";
        ShowHelp(cmdline);
        return 1;
      }
      if (nodeint == 32767) {
        // 32767 is the PPP project address for "send everything". Some people use this
        // "magic" node number.
        LOG(INFO) << "USE PPP Project to send to: Internet Email (@32767)";
        return LaunchOldNetworkingStack(net_cmdline, "networkp", argc, argv);
      }
    }

    BinkConfig bink_config(network_name, net_cmdline.config(), net_cmdline.networks());
    const binkp_session_config_t* node_config = bink_config.binkp_session_config_for(node);
    if (node_config != nullptr) {
      // We have a node configuration for this one, or it is a FTN
      // network, so we will use networkb.
      LOG(INFO) << "USE networkb: " << node_config->host << ":" << node_config->port;
      std::ostringstream ss;
      ss << FilePath(net_cmdline.bbsdir(), "networkb");
      ss << " --send --net=" << net_cmdline.network_number() << " --node=" << node;
      if (cmdline.barg("skip_net")) {
        ss << " --skip_net";
      }
      int verbose = cmdline.iarg("v");
      if (verbose > 0) {
        ss << " --v=" << verbose;
      }
      const auto command_line = ss.str();
      LOG(INFO) << "Executing Command: '" << command_line << "'";
      return system(command_line.c_str());
    }

    auto nodeint = to_number<int>(node);
    if (nodeint == 0) {
      LOG(ERROR) << "Not sure how to call out to: " << node;
      return 1;
    }
    PPPConfig ppp_config(network_name, net_cmdline.config(), net_cmdline.networks());
    const PPPNodeConfig* ppp_node_config = ppp_config.ppp_node_config_for(nodeint);
    if (ppp_node_config != nullptr) {
      LOG(INFO) << "USE PPP Project to send to: " << ppp_node_config->email_address;
      return LaunchOldNetworkingStack(net_cmdline, "networkp", argc, argv);
    }

    // Use legacy networking.
    return LaunchOldNetworkingStack(net_cmdline, "network0", argc, argv);
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [network]: " << e.what();
  }
  return 1;
}
