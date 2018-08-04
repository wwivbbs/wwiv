/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2017, WWIV Software Services             */
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
#include "core/semaphore_file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/os.h"
#include "core/textfile.h"
#include "core/version.h"
#include "core/findfiles.h"
#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "core/connection.h"
#include "networkb/net_util.h"
#include "sdk/net/packets.h"
#include "networkb/ppp_config.h"

#include "sdk/bbslist.h"
#include "sdk/callout.h"
#include "sdk/connect.h"
#include "sdk/config.h"
#include "sdk/contact.h"
#include "core/datetime.h"
#include "sdk/filenames.h"
#include "sdk/networks.h"
#include "sdk/subscribers.h"
#include "sdk/subxtr.h"
#include "sdk/fido/fido_address.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/fido/nodelist.h"
#include "sdk/net/packets.h"

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
using namespace wwiv::sdk::fido;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::os;

static void ShowHelp(const CommandLine& cmdline) {
  cout << cmdline.GetHelp()
       << ".####      Network number (as defined in wwivconfig)" << endl
       << endl;
  exit(1);
}

static bool check_wwivnet_host_networks(
  const wwiv::sdk::Config& config, 
  const wwiv::sdk::Networks& network,
  const BbsListNet& b,
  int network_number,
  std::ostringstream& text) {
  
  wwiv::sdk::Subs subs(config.datadir(), network.networks());
  if (!subs.Load()) {
    LOG(ERROR) << "Unable to load subs (.dat and .xtr)";
    return false;
  }

 const auto& net = network.networks()[network_number];

  for (const auto& x : subs.subs()) {
    for (const auto& n : x.nets) {
      if (n.net_num == network_number) {
        if (n.host == 0) {
          // Sub hosted here.
          std::set<uint16_t> subscribers;

          const string filename = StrCat("n", n.stype, ".net");
          if (ReadSubcriberFile(net.dir, filename, subscribers)) {
            for (uint16_t subscriber : subscribers) {
              auto c = b.node_config_for(subscriber);
              if (c == nullptr) {
                text << "Unknown system @" << subscriber << " subscribed to sub '" << n.stype << "'\r\n";
              }
            }
          } else {
            text << "Unable to find subscribers file for stype: " << n.stype << "\r\n";
          }
        } else {
          // Sub hosted elsewhere.
          auto c = b.node_config_for(n.host);
          if (c == nullptr) {
            text << "Unknown system @" << n.host << " hosting subtype '" << n.stype << "'\r\n";
          }
        }
      }
    }
  }
  return true;
}

static bool check_fido_host_networks(
  const wwiv::sdk::Config& config,
  const wwiv::sdk::Networks& network,
  const net_networks_rec& net,
  int network_number,
  std::ostringstream& text) {

  wwiv::sdk::Subs subs(config.datadir(), network.networks());
  if (!subs.Load()) {
    LOG(ERROR) << "Unable to load subs (.dat and .xtr)";
    text << "Unable to load subs (.dat and .xtr)\r\n";
    return false;
  }

  for (const auto& x : subs.subs()) {
    for (const auto& n : x.nets) {
      if (n.net_num != network_number) {
        continue;
      }
      if (n.host != 0) {
        continue;
      }
      const auto filename = StrCat("n", n.stype, ".net");
      if (!File::Exists(net.dir, filename)) {
        text << "subscriber file '" << filename << "' for echotag: '" << n.stype << "' is missing.\r\n";
        text << " ** Please fix it.\r\n\n";
      }
      LOG(INFO) << "Checking FTN Subscribers in file " << FilePath(net.dir, filename);
      auto subscribers = ReadFidoSubcriberFile(net.dir, filename);
      if (subscribers.empty()) {
        text << "Unable to find any uplinks in subscriber file for echotag: " << n.stype << "\r\n";
        text << " ** Please fix it.\r\n\n";
      }
    }
  }

  return true;
}

static bool check_connect_net(
  const BbsListNet& b,
  const net_networks_rec& net,
  std::ostringstream& text) {

  Connect connect(net.dir);
  for (const auto& entry : b.node_config()) {
    const auto n = connect.node_config_for(entry.first);
    if (n == nullptr) {
      text << "connect.net entry missing for node @" << entry.first << "\r\n";
    }
  }
  return true;
}

static bool check_binkp_net(
  const BbsListNet& b,
  const BinkConfig& bink_config,
  std::ostringstream& text) {
  for (const auto& entry : b.node_config()) {
    const auto binkp_entry = bink_config.binkp_session_config_for(entry.first);
    if (binkp_entry == nullptr) {
      text << "binkp.net entry missing for node @" << entry.first << "\r\n";
    }
  }
  return true;
}

static bool send_feedback_email(const net_networks_rec& net, const std::string& text) {
  net_header_rec nh = {};

  string now_mmddyy = daten_to_mmddyy(daten_t_now());
  string title = StringPrintf("%s analysis on %s", net.name, now_mmddyy.c_str());
  string byname = StringPrintf("%s @%u", net.name, net.sysnum);

  nh.touser = 1;
  nh.fromuser = std::numeric_limits<uint16_t>::max();
  nh.main_type = main_type_email;
  nh.daten = daten_t_now();

  return send_local_email(net, nh, text, byname, title);
}

static bool add_feedback_header(const std::string& net_dir, std::ostringstream& text) {
  TextFile feedback_hdr(net_dir, "fbackhdr.net", "rt");
  if (!feedback_hdr.IsOpen()) {
    return true;
  }
  text << "\r\n";
  auto lines = feedback_hdr.ReadFileIntoVector();
  for (const auto& line : lines) {
    text << line << "\r\n";
  }
  return true;
}

static uint16_t get_network_cordinator(const BbsListNet& b) {
  for (const auto& entry : b.node_config()) {
    if (entry.second.other & other_net_coord) {
      return entry.second.sysnum;
    }
  }
  return WWIVNET_NO_NODE;
}

static bool add_feedback_general_info(
    const BbsListNet& b, 
    const Callout& callout, 
    const net_networks_rec& net,
    const vector<net_system_list_rec>& bbsdata_data,
    std::ostringstream& text) {

  uint16_t nc = 0;
  uint16_t gc = 0;
  uint16_t ac = 0;
  map<int, int> hops_to_count;
  map<int, int> system_to_route_count;
  int total_hops = 0;
  for (const auto& d : bbsdata_data) {
    if (d.other & other_net_coord) {
      nc = d.sysnum;
    } else if (d.other & other_group_coord) {
      gc = d.sysnum;
    } else if (d.other & other_area_coord) {
      ac = d.sysnum;
    }

    // Make num hops map.
    auto hops = hops_to_count[d.numhops];
    hops_to_count[d.numhops] = hops + 1;

    total_hops += d.numhops;
    if (d.forsys != WWIVNET_NO_NODE) {
      auto num_route = system_to_route_count[d.forsys];
      system_to_route_count[d.forsys] = num_route + 1;
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
    text << " ** Missing connect.net entries.";
  } else {
    for (const auto& callout_node : c->connect) {
      const auto cnc = callout.net_call_out_for(callout_node);
      if (cnc == nullptr) {
        text << "Can call " << callout_node << " but isn't in callout.net.\r\n"
          << "  ** Add to callout.net\r\n\r\n";
      }
    }
  }

  return true;
}

void update_timestamps(const string& dir) {
  // Update timestamps on {bbslist,connect,callout}.net
  File bbsdata_net_file(FilePath(dir, BBSDATA_NET));
  time_t t = bbsdata_net_file.last_write_time();
  File(dir, BBSLIST_NET).set_last_write_time(t);
  File(dir, CONNECT_NET).set_last_write_time(t);
  File(dir, CALLOUT_NET).set_last_write_time(t);
}

static void write_bbsdata_reg_file(const BbsListNet& b, const string& dir) {
  LOG(INFO) << "Writing BBSDATA.REG...";
  vector<int32_t> bbsdata_reg_data;
  const auto& reg = b.reg_number();
  for (const auto& entry : b.node_config()) {
    bbsdata_reg_data.push_back(reg.at(entry.first));
  }
  DataFile<int32_t> bbsdata_reg_file(dir, BBSDATA_REG, File::modeBinary | File::modeReadWrite | File::modeCreateFile);
  bbsdata_reg_file.WriteVector(bbsdata_reg_data);
}

static void write_bbsdata_files(const vector<net_system_list_rec>& bbsdata_data, const string& dir) {
  {
    LOG(INFO) << "Writing bbsdata.net...";
    DataFile<net_system_list_rec> bbsdata_net_file(dir, BBSDATA_NET, File::modeBinary | File::modeReadWrite | File::modeCreateFile);
    bbsdata_net_file.WriteVector(bbsdata_data);
   }
  update_timestamps(dir);
  {
    LOG(INFO) << "Writing bbsdata.ind...";
    vector<uint16_t> bbsdata_ind_data;
    for (const auto& n : bbsdata_data) {
      bbsdata_ind_data.push_back((n.forsys == WWIVNET_NO_NODE) ? 0 : n.sysnum);
    }
    DataFile<uint16_t> bbsdata_ind_file(dir, BBSDATA_IND, File::modeBinary | File::modeReadWrite | File::modeCreateFile);
    bbsdata_ind_file.WriteVector(bbsdata_ind_data);
  }
  {
    LOG(INFO) << "Writing bbsdata.rou...";
    vector<uint16_t> bbsdata_rou_data;
    for (const auto& n : bbsdata_data) {
      bbsdata_rou_data.push_back(n.forsys);
    }
    DataFile<uint16_t> bbsdata_rou_file(dir, BBSDATA_ROU, File::modeBinary | File::modeReadWrite | File::modeCreateFile);
    bbsdata_rou_file.WriteVector(bbsdata_rou_data);
  }
}

static void update_net_ver_status_dat(const string& datadir) {
  statusrec_t statusrec{};
  DataFile<statusrec_t> file(datadir, STATUS_DAT, File::modeBinary | File::modeReadWrite);
  if (!file) {
    return;
  }
  if (!file.Read(0, &statusrec)) {
    return;
  }
  if (statusrec.net_version == wwiv_net_version) {
    return;
  }
  statusrec.net_bias = 0;
  statusrec.net_req_free = 0;
  statusrec.net_version = static_cast<uint16_t>(wwiv_net_version);
  file.Write(0, &statusrec);
}

static void update_filechange_status_dat(const string& datadir) {
  statusrec_t statusrec{};
  DataFile<statusrec_t> file(datadir, STATUS_DAT, File::modeBinary | File::modeReadWrite);
  if (file) {
    if (file.Read(0, &statusrec)) {
      statusrec.filechange[filechange_net]++;
      file.Write(0, &statusrec);
    }
  }
}

static void rename_pending_files(const string& dir) {
  File dead_net_file(FilePath(dir, DEAD_NET));
  if (dead_net_file.Exists()) {
    rename_pend(dir, DEAD_NET, '3');
  }

  FindFiles ff(dir, "s*.net", FindFilesType::files);
  for (const auto& f : ff) {
    rename_pend(dir, f.name, '3');
  }
}

static void ensure_contact_net_entries(const Callout& callout, const net_networks_rec& net) {
  Contact contact(net, true);
  for (const auto& entry : callout.callout_config()) {
    // Ensure we have a contact entry for each node in callout.net
    contact.ensure_rec_for(entry.first);
  }
}

static bool need_to_send_feedback(const CommandLine& cmdline) {
  if (cmdline.barg("feedback")) {
    return true;
  }
  for (const auto& s : cmdline.remaining()) {
    if (s == "Y" || s == "y") {
      return true;
    }
  }
  return false;
}

static int network3_fido(const NetworkCommandLine& net_cmdline) {
  VLOG(1) << "network3_fido";
  const auto& net = net_cmdline.network();
  std::ostringstream text;
  add_feedback_header(net.dir, text);
  LOG(INFO) << "Sending Feedback.";

  vector<net_system_list_rec> bbsdata_data;
  auto phone = net_cmdline.config().system_phone();
  {
    net_system_list_rec n1{};
    to_char_array(n1.name, net_cmdline.config().system_name());
    to_char_array(n1.phone, phone);
    n1.forsys = FTN_FAKE_OUTBOUND_NODE;
    n1.group = 0;
    n1.speed = 33600;
    n1.sysnum = 1;
    n1.numhops = 0;
    n1.forsys = 1;
    bbsdata_data.emplace_back(n1);
  }
  {
    auto fake_phone = phone;
    if (fake_phone.size() > 3) {
      fake_phone = fake_phone.substr(0, 3);
    } else {
      fake_phone = "415";
    }
    fake_phone += "-000-FIDO";

    net_system_list_rec n2{};
    to_char_array(n2.name, StrCat(net.name, " Gateway"));
    to_char_array(n2.phone, fake_phone);
    n2.sysnum = FTN_FAKE_OUTBOUND_NODE;
    n2.group = 0;
    n2.speed = 33600;
    n2.numhops = 1;
    n2.forsys = FTN_FAKE_OUTBOUND_NODE;
    bbsdata_data.emplace_back(n2);
  }

  write_bbsdata_files(bbsdata_data, net.dir);

  // create bbsdata.reg
  {
    vector<int32_t> bbsdata_reg_data;
    bbsdata_reg_data.push_back(net_cmdline.config().config()->wwiv_reg_number);
    bbsdata_reg_data.push_back(0);
    DataFile<int32_t> bbsdata_reg_file(net.dir, BBSDATA_REG, File::modeBinary | File::modeReadWrite | File::modeCreateFile);
    bbsdata_reg_file.WriteVector(bbsdata_reg_data);
  }

  FidoAddress address;
  try {
    FidoAddress a(net.fido.fido_address);
    address = a;
  } catch (const std::exception&) {
    text << "Unable to parse your address of: " << net.fido.fido_address << "\r\n";
    text << " ** Please fix it.\r\n\n";
  }
  wwiv::sdk::fido::FtnDirectories dirs(net_cmdline.config().root_directory(), net);
  text << "Inbound dir:             " << dirs.inbound_dir() << "\r\n";
  text << "Outbound dir:            " << dirs.outbound_dir() << "\r\n";
  text << "Temporary Inbound dir:   " << dirs.temp_inbound_dir() << "\r\n";
  text << "Temporary Outbound dir:  " << dirs.temp_outbound_dir() << "\r\n";
  text << "Bad Packets dir:         " << dirs.bad_packets_dir() << "\r\n";
  text << "\r\n";

  if (!File::Exists(dirs.net_dir(), FIDO_CALLOUT_JSON)) {
    text << " ** fido_callout.json file DOES NOT EXIST.\r\n\n";
  }
  FidoCallout callout(net_cmdline.config(), net);
  if (!callout.IsInitialized()) {
    text << " ** Unable to read fido_callout.json\r\n\n";
  } else {
    check_fido_host_networks(net_cmdline.config(), net_cmdline.networks(), net, net_cmdline.network_number(), text);
  }

  text << "Using nodelist base:     " << net.fido.nodelist_base << "\r\n";
  text << "Using nodelist base dir: " << dirs.net_dir() << "\r\n";
  auto nodelist = Nodelist::FindLatestNodelist(dirs.net_dir(), net.fido.nodelist_base);
  File nlfile(FilePath(dirs.net_dir(), nodelist));
  text << "Latest FTN is:           " << nodelist;
  if (!nlfile.Exists()) {
    text << " (DOES NOT EXIST)\r\n";
    text << " ** Please fix it.\r\n\n";
  } else {
    text << " [" << time_t_to_wwivnet_time(nlfile.creation_time()) << "]\r\n";
    auto nl_path = File::absolute(dirs.net_dir(), nodelist);
    Nodelist nl(nl_path);
    if (!nl.initialized()) {
      text << " ** Unable to parse nodelist.\r\n";
      text << " ** Please fix it.\r\n\n";
    } else {
      if (!nl.contains(address)) {
        text << " ** Your address: '" << address << "' does not exist in the nodelist: '" << nodelist << ".\r\n";
      }
      for (const auto& ncs : callout.node_configs_map()) {
        if (!nl.contains(ncs.first)) {
          text << " ** Callout address: '" << ncs.first.as_string() << "' does not exist in the nodelist.\r\n";
        }
      }
    }
  }

  text << "\r\n";

  if (net.fido.origin_line.empty()) {
    text << "You may want to define an origin line for your network.\r\n";
  }

  text << "\r\nBest,\r\n\r\n" << net.name << "@" << net.sysnum << "\r\n\r\n";

  if (need_to_send_feedback(net_cmdline.cmdline())) {
    send_feedback_email(net, text.str());
  }

  return 0;
}

static int network3_wwivnet(const NetworkCommandLine& net_cmdline) {
  VLOG(1) << "Reading bbslist.net..";
  const auto& net = net_cmdline.network();
  BbsListNet b = BbsListNet::ParseBbsListNet(net.sysnum, net.dir);
  if (b.empty()) {
    LOG(ERROR) << "ERROR: bbslist.net didn't parse.";
    return 1;
  }

  auto nc = get_network_cordinator(b);
  bool is_nc = (net.sysnum == nc);
  LOG(INFO) << "I am the nc, my node # is @" << net.sysnum;

  vector<net_system_list_rec> bbsdata_data;
  for (const auto& entry : b.node_config()) {
    const auto& n = entry.second;
    bbsdata_data.push_back(n);
  }

  write_bbsdata_files(bbsdata_data, net.dir);
  write_bbsdata_reg_file(b, net.dir);

  VLOG(1) << "Reading callout.net...";
  Callout callout(net);
  ensure_contact_net_entries(callout, net);
  update_filechange_status_dat(net_cmdline.config().datadir());
  rename_pending_files(net.dir);

  if (need_to_send_feedback(net_cmdline.cmdline()) || is_nc) {
    std::ostringstream text;
    add_feedback_header(net.dir, text);
    LOG(INFO) << "Sending Feedback.";
    add_feedback_general_info(b, callout, net, bbsdata_data, text);
    check_wwivnet_host_networks(net_cmdline.config(), net_cmdline.networks(), b, net_cmdline.network_number(), text);

    if (is_nc) {
      // We should alwyas send feedback to the NCs.
      BinkConfig bink_config(net_cmdline.network_name(), net_cmdline.config(), net_cmdline.networks());
      check_binkp_net(b, bink_config, text);
      check_connect_net(b, net, text);
    }
    text << "\r\nBest,\r\n\r\n" << net.name << "@" << net.sysnum << "\r\n\r\n";
    send_feedback_email(net, text.str());
  }
  return 0;
}

int network3_main(const NetworkCommandLine& net_cmdline) {
  try {
    const auto& net = net_cmdline.network();
    update_net_ver_status_dat(net_cmdline.config().datadir());

    if (!File::Exists(net.dir)) {
      LOG(ERROR) << "Network directory '" << net.dir << "' does not exist.";
      LOG(ERROR) << "Please create it.";
      return 1;
    }

    // Only run the net fido type network3 for 5.x
    if (net_cmdline.config().is_5xx_or_later() && net.type == network_type_t::ftn) {
      return network3_fido(net_cmdline);
    }
    return network3_wwivnet(net_cmdline);
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [network]: " << e.what();
  }
  return 2;
}


int main(int argc, char** argv) {
  Logger::Init(argc, argv);
  ScopeExit at_exit(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");
  cmdline.add_argument(BooleanCommandLineArgument("feedback", 'y', "Sends feedback.", false));
  NetworkCommandLine net_cmdline(cmdline, '2');
  if (!net_cmdline.IsInitialized() || net_cmdline.cmdline().help_requested()) {
    ShowHelp(net_cmdline.cmdline());
    return 1;
  }

  try {
    auto semaphore = SemaphoreFile::try_acquire(net_cmdline.semaphore_filename(),
                                                net_cmdline.semaphore_timeout());
    return network3_main(net_cmdline);
  } catch (const semaphore_not_acquired& e) {
    LOG(ERROR) << "ERROR: [network" << net_cmdline.net_cmd()
               << "]: Unable to Acquire Network Semaphore: " << e.what();
  }
}
