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

static void ShowHelp() {
  //       0        1         2         3         4         5         6         7          
  //       1234567890123456789012345678901234567890123456789012345678901234567890123456789
  cout << "\nUsage: networkb [flags]" << endl
       << "Flags:" << endl
       << "--network      Network name to use (i.e. wwivnet)" << endl
       << "--bbsdir       (optional) BBS directory if other than current directory " << endl
       << "--send         Send network traffic to --node" << endl
       << "--receive      Receive from any node" << endl
       << "--node         Node number (only used when sending)" << endl
       << "--port         Port number to use (receiving only)" << endl
       << "--skip_net     Skip invoking network1/network2/network3" << endl
       << "--dump_callout Displays the data in CALLOUT.NET as understood by networkb."
       << endl;
}

static map<string, string> ParseArgs(int argc, char** argv) {
  map<string, string> args;
  for (int i=0; i < argc; i++) {
    const string s(argv[i]);
    if (starts_with(s, "--")) {
      const vector<string> delims = SplitString(s, "=");
      const string value = (delims.size() > 1) ? delims[1] : "";
      args.emplace(delims[0].substr(2), value);
    }
  }
  return args;
}

int main(int argc, char** argv) {
  try {
    Logger::Init(argc, argv);
    wwiv::core::ScopeExit at_exit(Logger::ExitLogger);
    map<string, string> args = ParseArgs(argc, argv);

    for (const auto& arg : args) {
      if (!arg.second.empty()) {
        LOG << "arg: --" << arg.first << "=" << arg.second;
      } else {
        LOG << "arg: --" << arg.first;
      }
      if (arg.first == "help") {
        ShowHelp();
        return 0;
      }
    }

    if (!contains(args, "network") && contains(args, "send")) {
      LOG << "--network=[network name] must be specified.";
      ShowHelp();
      return 1;
    }
    string network_name;
    if (contains(args, "network")) {
      network_name = args.at("network");
      StringLowerCase(&network_name);
    }

    int port = 24554;
    if (contains(args, "port")) {
      port = std::stoi(args.at("port"));
    }

    bool skip_net = contains(args, "skip_net");
  
    string bbsdir = File::current_directory();
    if (contains(args, "bbsdir")) {
      bbsdir = args["bbsdir"];
    }
    Config config(bbsdir);
    if (!config.IsInitialized()) {
      LOG << "Unable to load config.dat.";
      ShowHelp();
      return 1;
    }
    Networks networks(config);
    if (!networks.IsInitialized()) {
      LOG << "Unable to load networks.";
      ShowHelp();
      return 1;
    }

    int expected_remote_node = 0;
    if (contains(args, "node")) {
      expected_remote_node = std::stoi(args["node"]);
    }

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

    if (contains(args, "dump_callout")) {
      for (const auto& c : callouts) {
        std::cout << "CALLOUT.NET information: : " << c.first << std::endl;
        std::cout << "===========================================================" << std::endl;
        std::cout << c.second.ToString() << std::endl;
      }
      return 0;
    }

    if (contains(args, "receive")) {
      LOG << "BinkP receive";
      side = BinkSide::ANSWERING;
      c = Accept(port);
    } else if (contains(args, "send")) {
      LOG << "BinkP send to: " << expected_remote_node;
      const BinkNodeConfig* node_config = bink_config.node_config_for(expected_remote_node);
      if (node_config == nullptr) {
        LOG << "Unable to find node config for node: " << expected_remote_node;
        return 2;
      }
      c = Connect(node_config->host, node_config->port);
    } else {
      LOG << "No command given to send or receive.  Either use '--send --node=#' or --receive";
      ShowHelp();
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
