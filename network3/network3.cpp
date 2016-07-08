/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*                Copyright (C)2016, WWIV Software Services               */
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

// WWIV5 Network3
#include <cctype>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/datafile.h"
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

#include "sdk/bbslist.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
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
       << ".####      Network number (as defined in INIT)" << endl
       << endl;
  exit(1);
}



int main(int argc, char** argv) {
  Logger::Init(argc, argv);
  try {
    ScopeExit at_exit(Logger::ExitLogger);
    CommandLine cmdline(argc, argv, "network_number");
    cmdline.set_no_args_allowed(true);
    cmdline.add_argument({"network", "Network name to use (i.e. wwivnet).", ""});
    cmdline.add_argument({"network_number", "Network number to use (i.e. 0).", "0"});
    cmdline.add_argument({"bbsdir", "(optional) BBS directory if other than current directory", File::current_directory()});
    cmdline.add_argument(BooleanCommandLineArgument("help", '?', "displays help.", false));
    cmdline.add_argument(BooleanCommandLineArgument("feedback", 'y', "Sends feedback.", false));

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
    std::cout << "NETWORK3 for network: " << network_name << std::endl;

    const string network_dir = networks[network_name].dir;
    auto sysnum = networks[network_name].sysnum;

    BbsListNet b = BbsListNet::ParseBbsListNet(sysnum, network_dir);
    if (b.empty()) {
      LOG << "ERROR: bbslist.net didn't parse.";
      return 1;
    }

    vector<net_system_list_rec> bbsdata_data;
    vector<uint16_t> bbsdata_ind_data;
    vector<uint16_t> bbsdata_rou_data;
    for (const auto& entry : b.node_config()) {
      const auto& n = entry.second;
      bbsdata_data.push_back(n);
      bbsdata_ind_data.push_back((n.forsys == 65535) ? 0 : n.sysnum);
      bbsdata_rou_data.push_back(n.forsys);
    }

    {
      DataFile<net_system_list_rec> bbsdata_net_file(network_dir, BBSDATA_NET);
      bbsdata_net_file.WriteVector(bbsdata_data);
    }
    {
      DataFile<uint16_t> bbsdata_ind_file(network_dir, BBSDATA_IND);
      bbsdata_ind_file.WriteVector(bbsdata_ind_data);
    }
    {
      DataFile<uint16_t> bbsdata_rou_file(network_dir, BBSDATA_IND);
      bbsdata_rou_file.WriteVector(bbsdata_rou_data);
    }

    /*
     * TODO:
     * Read callout.net
     * open contact.net
     * add missing entries into contact.net
     *
     * check network for errors & send feedback
     * rename dead.net to pending file
     * rename all S*.NET files to pending file.
     * update status.dat increment status.filechange[filechange_net]
     *
     */
  } catch (const std::exception& e) {
    LOG << "ERROR: [network]: " << e.what();
  }
}
