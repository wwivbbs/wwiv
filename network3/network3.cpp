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
#include <ctime>
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
#include "core/textfile.h"
#include "core/wfndfile.h"
#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "networkb/connection.h"
#include "networkb/net_util.h"
#include "networkb/ppp_config.h"

#include "sdk/bbslist.h"
#include "sdk/callout.h"
#include "sdk/config.h"
#include "sdk/contact.h"
#include "sdk/datetime.h"
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

static bool send_feedback(const net_networks_rec& net, const std::string& text) {
  net_header_rec nh = {};

  string now_human = wwiv::sdk::daten_to_date(time(nullptr));
  string title = StringPrintf("%s analysis on %s", net.name, now_human.c_str());
  string byname = StringPrintf("%s @%u", net.name, net.sysnum);

  nh.touser = 1;
  nh.fromuser = std::numeric_limits<uint16_t>::max();
  nh.main_type = main_type_email;
  nh.daten = wwiv::sdk::time_t_to_daten(time(nullptr));

  return send_local(net, &nh, text, byname, title);
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

    LOG << "Reading BBSLIST.NET..";
    BbsListNet b = BbsListNet::ParseBbsListNet(sysnum, network_dir);
    if (b.empty()) {
      LOG << "ERROR: bbslist.net didn't parse.";
      return 1;
    }

    vector<net_system_list_rec> bbsdata_data;
    vector<uint16_t> bbsdata_ind_data;
    vector<uint16_t> bbsdata_rou_data;
    vector<int32_t> bbsdata_reg_data;
    const auto& reg = b.reg_number();
    for (const auto& entry : b.node_config()) {
      const auto& n = entry.second;
      bbsdata_data.push_back(n);
      bbsdata_ind_data.push_back((n.forsys == 65535) ? 0 : n.sysnum);
      bbsdata_rou_data.push_back(n.forsys);
      bbsdata_reg_data.push_back(reg.at(entry.first));
    }

    {
      LOG << "Writing BBSDATA.NET...";
      DataFile<net_system_list_rec> bbsdata_net_file(network_dir, BBSDATA_NET, File::modeBinary | File::modeReadWrite | File::modeCreateFile);
      bbsdata_net_file.WriteVector(bbsdata_data);
    }
    {
      LOG << "Writing BBSDATA.IND...";
      DataFile<uint16_t> bbsdata_ind_file(network_dir, BBSDATA_IND, File::modeBinary | File::modeReadWrite | File::modeCreateFile);
      bbsdata_ind_file.WriteVector(bbsdata_ind_data);
    }
    {
      LOG << "Writing BBSDATA.ROU...";
      DataFile<uint16_t> bbsdata_rou_file(network_dir, BBSDATA_ROU, File::modeBinary | File::modeReadWrite | File::modeCreateFile);
      bbsdata_rou_file.WriteVector(bbsdata_rou_data);
    }
    {
      DataFile<int32_t> bbsdata_reg_file(network_dir, BBSDATA_REG, File::modeBinary | File::modeReadWrite | File::modeCreateFile);
      LOG << "Writing BBSDATA.REG...";
      bbsdata_reg_file.WriteVector(bbsdata_reg_data);
    }

    {
      LOG << "Reading CALLOUT.NET...";
      Callout callout(network_dir);
      Contact contact(network_dir, true);

      for (const auto& entry : callout.node_config()) {
        // Ensure we have a contact entry for each node in CALLOUT.NET
        const auto node = entry.first;
        contact.ensure_rec_for(node);
      }
    }

    {
      statusrec status{};
      DataFile<statusrec> file(config.datadir(), STATUS_DAT, File::modeBinary | File::modeReadWrite);
      if (file) {
        if (file.Read(0, &status)) {
          status.filechange[filechange_net]++;
          file.Write(0, &status);
        }
      }
    }

    File dead_net_file(network_dir, DEAD_NET);
    if (dead_net_file.Exists()) {
      rename_pend(network_dir, DEAD_NET);
    }

    WFindFile s_files;
    bool has_next = s_files.open(StrCat(network_dir, "s*.net"), WFNDFILE_ANY);
    while (has_next) {
      const string name = s_files.GetFileName();
      rename_pend(network_dir, name);
      has_next = s_files.next();
    }

    /* Still TODO: check network for errors */

    const auto net = networks[network_name];
    string text = "\r\n";
    TextFile feedback_hdr(network_dir, "FBACKHDR.NET", "rt");
    if (feedback_hdr.IsOpen()) {
      string line;
      while (feedback_hdr.ReadLine(&line)) {
        text += line;
        text += "\r\n";
      }
    }

    uint16_t nc = 0;
    uint16_t gc = 0;
    uint16_t ac = 0;
    map<int, int> hops_to_count;
    map<int, int> system_to_route_count;
    int total_hops = 0;
    for (const auto& b : bbsdata_data) {
      if (b.other & other_net_coord) {
        nc = b.sysnum;
      } else if (b.other & other_group_coord) {
        gc = b.sysnum;
      } else if (b.other & other_area_coord) {
        ac = b.sysnum;
      }
      
      // Make num hops map.
      int hops = hops_to_count[b.numhops];
      hops_to_count[b.numhops] = hops + 1;

      total_hops += b.numhops;
      if (b.forsys != 65535) {
        int num_route = system_to_route_count[b.forsys];
        system_to_route_count[b.forsys] = num_route + 1;
      }
    }
    
    text += StringPrintf("Network Coordinator is @%u\r\n", nc);
    text += StringPrintf("Group Coordinator is @%u\r\n", (gc != 0) ? gc : nc);
    text += StringPrintf("Area Coordinator is @%u\r\n", (ac != 0) ? ac : nc);
    text += "\r\n";
    text += "Using bias of 0.00100 $ / k / hop.\r\n";
    text += "\r\n";
    text += "\r\n";
    for (const auto& e : hops_to_count) {
      if (e.first > 0 && e.first < 10000) {
        text += StringPrintf("%d systems are %d hops away.\r\n", e.second, e.first);
      }
    }
    text += "\r\n";
    for (const auto& e : system_to_route_count) {
      if (e.first != sysnum) {
        text += StringPrintf("%d systems route through @%d.\r\n", e.second, e.first);
      }
    }
    text += "\r\n";
    send_feedback(net, text);
  } catch (const std::exception& e) {
    LOG << "ERROR: [network]: " << e.what();
  }
}
