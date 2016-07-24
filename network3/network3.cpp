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
#include <sstream>
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
#include "sdk/connect.h"
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

static bool send_feedback_email(const net_networks_rec& net, const std::string& text) {
  net_header_rec nh = {};

  string now_human = wwiv::sdk::daten_to_date(time(nullptr));
  string title = StringPrintf("%s analysis on %s", net.name, now_human.c_str());
  string byname = StringPrintf("%s @%u", net.name, net.sysnum);

  nh.touser = 1;
  nh.fromuser = std::numeric_limits<uint16_t>::max();
  nh.main_type = main_type_email;
  nh.daten = wwiv::sdk::time_t_to_daten(time(nullptr));

  return send_local(net, nh, text, byname, title);
}

static bool send_feedback(
    const BbsListNet& b, 
    const Callout& callout, 
    const net_networks_rec& net,
    const vector<net_system_list_rec>& bbsdata_data) {
  /*
  Still TODO:
  check network for errors.
  check subs to ensure subscribers and host both exist.
  */

  std::ostringstream text;
  text << "\r\n";
  TextFile feedback_hdr(net.dir, "fbackhdr.net", "rt");
  if (feedback_hdr.IsOpen()) {
    string line;
    while (feedback_hdr.ReadLine(&line)) {
      text << line << "\r\n";
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

  text << StringPrintf("Network Coordinator is @%u\r\n", nc);
  text << StringPrintf("Group Coordinator is @%u\r\n", (gc != 0) ? gc : nc);
  text << StringPrintf("Area Coordinator is @%u\r\n", (ac != 0) ? ac : nc);
  text << "\r\n";
  text << "Using bias of 0.00100 $ / k / hop.\r\n";
  text << "\r\n";
  text << "\r\n";
  for (const auto& e : hops_to_count) {
    if (e.first > 0 && e.first < 10000) {
      text << StringPrintf("%d systems are %d hops away.\r\n", e.second, e.first);
    }
  }
  text << "\r\n";
  for (const auto& e : system_to_route_count) {
    if (e.first != net.sysnum) {
      text << StringPrintf("%d systems route through @%d.\r\n", e.second, e.first);
    }
  }
  text << "\r\n";

  Connect connect(net.dir);
  const auto c = connect.node_config_for(net.sysnum);
  if (c == nullptr) {
    text << " ** Missing CONNECT.NET entries.";
  } else {
    for (const auto& callout_node : c->connect) {
      const auto cnc = callout.node_config_for(callout_node);
      if (cnc == nullptr) {
        text << "Can call " << callout_node << " but isn't in CALLOUT.NET.\r\n"
          << "  ** Add to CALLOUT.NET\r\n";
      }
    }
  }

  return send_feedback_email(net, text.str());
}

void update_timestamps(const string& dir) {
  // Update timestamps on {bbslist,connect,callout}.net
  File bbsdata_net_file(dir, BBSDATA_NET);
  time_t t = bbsdata_net_file.last_write_time();
  File(dir, BBSLIST_NET).set_last_write_time(t);
  File(dir, CONNECT_NET).set_last_write_time(t);
  File(dir, CALLOUT_NET).set_last_write_time(t);
}

void write_bbsdata_files(const BbsListNet& b, const vector<net_system_list_rec>& bbsdata_data,
  const string& dir) {
  {
    LOG(INFO) << "Writing BBSDATA.NET...";
    DataFile<net_system_list_rec> bbsdata_net_file(dir, BBSDATA_NET, File::modeBinary | File::modeReadWrite | File::modeCreateFile);
    bbsdata_net_file.WriteVector(bbsdata_data);
   }
  update_timestamps(dir);
  {
    LOG(INFO) << "Writing BBSDATA.IND...";
    vector<uint16_t> bbsdata_ind_data;
    for (const auto& n : bbsdata_data) {
      bbsdata_ind_data.push_back((n.forsys == 65535) ? 0 : n.sysnum);
    }
    DataFile<uint16_t> bbsdata_ind_file(dir, BBSDATA_IND, File::modeBinary | File::modeReadWrite | File::modeCreateFile);
    bbsdata_ind_file.WriteVector(bbsdata_ind_data);
  }
  {
    LOG(INFO) << "Writing BBSDATA.ROU...";
    vector<uint16_t> bbsdata_rou_data;
    for (const auto& n : bbsdata_data) {
      bbsdata_rou_data.push_back(n.forsys);
    }
    DataFile<uint16_t> bbsdata_rou_file(dir, BBSDATA_ROU, File::modeBinary | File::modeReadWrite | File::modeCreateFile);
    bbsdata_rou_file.WriteVector(bbsdata_rou_data);
  }
  {
    LOG(INFO) << "Writing BBSDATA.REG...";
    vector<int32_t> bbsdata_reg_data;
    const auto& reg = b.reg_number();
    for (const auto& entry : b.node_config()) {
      bbsdata_reg_data.push_back(reg.at(entry.first));
    }
    DataFile<int32_t> bbsdata_reg_file(dir, BBSDATA_REG, File::modeBinary | File::modeReadWrite | File::modeCreateFile);
    bbsdata_reg_file.WriteVector(bbsdata_reg_data);
  }
}

static void update_filechange_status_dat(const string& datadir) {
  statusrec status{};
  DataFile<statusrec> file(datadir, STATUS_DAT, File::modeBinary | File::modeReadWrite);
  if (file) {
    if (file.Read(0, &status)) {
      status.filechange[filechange_net]++;
      file.Write(0, &status);
    }
  }
}

static void rename_pending_files(const string& dir) {
  File dead_net_file(dir, DEAD_NET);
  if (dead_net_file.Exists()) {
    rename_pend(dir, DEAD_NET, 3);
  }

  WFindFile s_files;
  bool has_next = s_files.open(StrCat(dir, "s*.net"), WFINDFILE_FILES);
  while (has_next) {
    const string name = s_files.GetFileName();
    rename_pend(dir, name, 3);
    has_next = s_files.next();
  }
}

static void ensure_contact_net_entries(const Callout& callout, const string& dir) {
  Contact contact(dir, true);
  for (const auto& entry : callout.node_config()) {
    // Ensure we have a contact entry for each node in CALLOUT.NET
    contact.ensure_rec_for(entry.first);
  }
}

INITIALIZE_EASYLOGGINGPP
int main(int argc, char** argv) {
  Logger::Init(argc, argv);
  try {
    ScopeExit at_exit(Logger::ExitLogger);
    CommandLine cmdline(argc, argv, "network_number");
    cmdline.set_no_args_allowed(true);
    cmdline.AddStandardArgs();
    AddStandardNetworkArgs(cmdline, File::current_directory());

    cmdline.add_argument(BooleanCommandLineArgument("feedback", 'y', "Sends feedback.", false));

    if (!cmdline.Parse() || cmdline.arg("help").as_bool()) {
      ShowHelp(cmdline);
      return 1;
    }
    int network_number = cmdline.arg("network_number").as_int();

    string bbsdir = cmdline.arg("bbsdir").as_string();
    Config config(bbsdir);
    if (!config.IsInitialized()) {
      LOG(ERROR) << "Unable to load CONFIG.DAT.";
      return 1;
    }
    Networks networks(config);
    if (!networks.IsInitialized()) {
      LOG(ERROR) << "Unable to load networks.";
      return 1;
    }

    bool need_to_send_feedback = cmdline.barg("feedback");
    if (!need_to_send_feedback) {
      for (const auto& s : cmdline.remaining()) {
        if (s == "Y" || s == "y") {
          need_to_send_feedback = true;
        }
      }
    }

    const auto& net = networks[network_number];
    LOG(INFO) << "NETWORK3 for network: " << net.name;

    VLOG(1) << "Reading BBSLIST.NET..";
    BbsListNet b = BbsListNet::ParseBbsListNet(net.sysnum, net.dir);
    if (b.empty()) {
      LOG(ERROR) << "ERROR: bbslist.net didn't parse.";
      return 1;
    }

    vector<net_system_list_rec> bbsdata_data;
    for (const auto& entry : b.node_config()) {
      const auto& n = entry.second;
      bbsdata_data.push_back(n);
    }

    write_bbsdata_files(b, bbsdata_data, net.dir);

    VLOG(1) << "Reading CALLOUT.NET...";
    Callout callout(net.dir);
    ensure_contact_net_entries(callout, net.dir);
    update_filechange_status_dat(config.datadir());
    rename_pending_files(net.dir);

    if (need_to_send_feedback) {
      LOG(INFO) << "Sending Feedback.";
      send_feedback(b, callout, net, bbsdata_data);
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [network]: " << e.what();
  }
}
