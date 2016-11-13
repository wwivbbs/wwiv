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

// WWIV5 Networkf
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <set>
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
#include "networkb/packets.h"
#include "networkb/ppp_config.h"

#include "sdk/bbslist.h"
#include "sdk/callout.h"
#include "sdk/connect.h"
#include "sdk/config.h"
#include "sdk/contact.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"
#include "sdk/networks.h"
#include "sdk/fido/fido_address.h"
#include "sdk/fido/fido_packets.h"

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
using namespace wwiv::sdk::fido;

static vector<arcrec> read_arcs(const std::string& datadir) {
  vector<arcrec> arcs;
  DataFile<arcrec> file(datadir, ARCHIVER_DAT);
  if (file) {
    file.ReadVector(arcs, 20);
  }
  return arcs;
}

/** returns the arcrec for the extension, or the 1st one if none match */
static arcrec find_arc(const vector<arcrec> arcs, const std::string extension) {
  string ue = extension;
  StringUpperCase(&ue);
  for (const auto& a : arcs) {
    if (ue == a.extension) {
      return a;
    }
  }
  return arcs.front();
}

static string arc_stuff_in(const string& command_line, const string& a1, const string& a2) {
  std::ostringstream os;
  for (auto it = command_line.begin(); it != command_line.end(); it++) {
    if (*it == '%') {
      it++;
      switch (*it) {
      case '%': os << "%"; break;
      case '1': os << a1; break;
      case '2': os << a2; break;
      }
    } else {
      os << *it;
    }
  }
  return string(os.str());
}

static void ShowHelp(CommandLine& cmdline) {
  cout << cmdline.GetHelp()
    << ".####      Network number (as defined in INIT)" << endl
    << endl
    << "commands: " << endl 
    << endl
    << " import    Import messages from FTN Packet to WWIV (P*.net)" << endl
    << " export    Export messages from WWIV (p*.net) to FTN packet" << endl
    << endl;
  exit(1);
}
/*
static bool handle_packet(
  const BbsListNet& b,
  const net_networks_rec& net, Packet& p) {

  if (p.nh.tosys == net.sysnum) {
    // Local Packet.
    return write_wwivnet_packet(LOCAL_NET, net, p);
  } else if (p.list.empty()) {
    // Network packet, single destination
    return write_wwivnet_packet(wwivnet_packet_name(net, get_forsys(b, p.nh.tosys)), net, p);
  } else {
    // Network packet, multiple destinations.
    std::map<uint16_t, std::set<uint16_t>> forsys_to_all;
    for (const auto& node : p.list) {
      uint16_t forsys = get_forsys(b, node);
      forsys_to_all[forsys].insert(node);
    }

    bool result = true;
    for (const auto& fa : forsys_to_all) {
      const auto forsys = fa.first;
      Packet np(p.nh, std::vector<uint16_t>(fa.second.begin(), fa.second.end()), p.text);
      np.nh.list_len = static_cast<uint16_t>(np.list.size());
      if (np.list.size() == 1) {
        // If we only have 1, move it out of list into tosys.
        np.nh.tosys = *np.list.begin();
        np.nh.list_len = 0;
        np.list.clear();
      }
      if (!write_wwivnet_packet(wwivnet_packet_name(net, forsys), net, np)) {
        result = false;
      }
    }
    return result;
  }

  return false;
}*/

// TODO(rushfan): move this somewhere common since it's copied
// from dump_fido_packet.cpp
static std::string FidoToWWIVText(const std::string& ft) {
  std::string wt;
  for (auto& c : ft) {
    if (c == 13) {
      wt.push_back(13);
      wt.push_back(10);
    } else if (c == 10) {
      // NOP
    } else {
      wt.push_back(c);
    }
  }
  return wt;
}

static string get_echomail_areaname(const std::string& text) {
  string temp = text;
  temp.erase(std::remove(temp.begin(), temp.end(), 10), temp.end());
  temp.erase(std::remove(temp.begin(), temp.end(), '\x8d'), temp.end());
  vector<string> lines = SplitString(temp, "\r");
  for (const auto& line : lines) {
    if (starts_with(line, "AREA:")) {
      return line.substr(5);
    }
  }
  return "";
}

static bool import_packet_file(const net_networks_rec& net, const std::string& dir, const string& name) {
  using wwiv::sdk::fido::ReadPacketResponse;

  File f(dir, name);
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    LOG(INFO) << "Unable to open file: " << dir << name;
    return false;
  }

  bool done = false;
  packet_header_2p_t header = {};
  auto num_header_read = f.Read(&header, sizeof(packet_header_2p_t));
  if (num_header_read < sizeof(packet_header_2p_t)) {
    LOG(ERROR) << "Read less than packet header";
    return 1;
  }
  while (!done) {
    FidoPackedMessage msg;
    ReadPacketResponse response = read_packed_message(f, msg);
    if (response == ReadPacketResponse::END_OF_FILE) {
      return 0;
    } else if (response == ReadPacketResponse::ERROR) {
      return 1;
    }

    net_header_rec nh{};
    // TODO(rushfan): Hack - need to parse the fidonet date.
    nh.daten = static_cast<uint32_t>(time(nullptr));
    nh.fromsys = net.fido.fake_outbound_node;
    nh.fromuser = 0;
    nh.list_len = 0;
    nh.main_type = main_type_new_post;
    nh.method = 0;
    nh.minor_type = 0;
    nh.tosys = 1; // always 1 in new fido
    nh.touser = 0;

    // SUBTYPE<nul>TITLE<nul>SENDER_NAME<cr / lf>DATE_STRING<cr / lf>MESSAGE_TEXT.
    string text = get_echomail_areaname(msg.vh.text);
    text.push_back(0);
    text.append(msg.vh.subject);
    text.push_back(0);
    text.append(StrCat(msg.vh.from_user_name, "(", msg.nh.orig_net, "/", msg.nh.orig_node, ")\r\n"));
    text.append(StrCat(msg.vh.date_time, "\r\n"));
    text.append(msg.vh.text);

    nh.length = text.size();
    // Create packet, write to LOCAL.NET for network2 to import.
    Packet packet(nh, {}, text);
    write_wwivnet_packet(LOCAL_NET, net, packet);
  }

  return true;
}

static bool import_packets(const net_networks_rec& net, const std::string& dir, const std::string& mask) {
  WFindFile files;
  bool has_next = files.open(FilePath(dir, mask), WFINDFILE_FILES);
  while (has_next) {
    const auto& name = files.GetFileName();
    if (import_packet_file(net, dir, name)) {
      File::Remove(dir, name);
    }
    has_next = files.next();
  }
  return true;
}


static bool import_bundle_file(const Config& config, const net_networks_rec& net, const std::string& dir, const string& name) {
  File f(dir, name);
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    LOG(INFO) << "Unable to open file: " << dir << name;
    return false;
  }

  const std::string saved_dir = File::current_directory();
  auto tempdir = net.fido.temp_inbound_dir;
  File::MakeAbsolutePath(net.dir, &tempdir);
  File::set_current_directory(tempdir);

  ScopeExit at_exit([=] { File::set_current_directory(saved_dir); });

  // were in the temp dir now.
  vector<arcrec> arcs = read_arcs(config.datadir());
  if (arcs.empty()) {
    LOG(ERROR) << "No archivers defined!";
    return false;
  }

  // TODO(rushfan): Need callout.json support to set packet specific options here.
  const auto& arc = find_arc(arcs, net.fido.packet_config.compression_type);
  // We have no parameter 2 since we're extracting everything.
  string unzip_cmd = arc_stuff_in(arc.arce, FilePath(dir, name), "");
  // Execute the command
  system(unzip_cmd.c_str());
  File::set_current_directory(saved_dir);

  import_packets(net, tempdir, "*.pkt");
#ifndef _WIN32
  import_packets(net, tempdir, "*.PKT");
#endif  // _WIN32
  return true;
}

static bool import_bundles(const Config& config, const net_networks_rec& net, const std::string& dir, const std::string& mask) {
  WFindFile files;
  bool has_next = files.open(FilePath(dir, mask), WFINDFILE_FILES);
  while (has_next) {
    const auto& name = files.GetFileName();
    if (import_bundle_file(config, net, dir, name)) {
      File::Remove(dir, name);
    }
    has_next = files.next();
  }
  return true;
}

int main(int argc, char** argv) {
  Logger::Init(argc, argv);
  try {
    ScopeExit at_exit(Logger::ExitLogger);
    CommandLine cmdline(argc, argv, "net");
    cmdline.set_no_args_allowed(true);
    cmdline.AddStandardArgs();
    AddStandardNetworkArgs(cmdline, File::current_directory());

    if (!cmdline.Parse() || cmdline.arg("help").as_bool()) {
      ShowHelp(cmdline);
      return 1;
    }
    NetworkCommandLine net_cmdline(cmdline);
    if (!net_cmdline.IsInitialized()) {
      return 1;
    }

    const auto& net = net_cmdline.network();
    LOG(INFO) << "NETWORKF for network: " << net.name;

    if (net.type != network_type_t::ftn) {
      LOG(ERROR) << "NETWORKF is only for use on FTN type networks.";
      return 1;
    }

    VLOG(1) << "Reading BBSDATA.NET..";
    BbsListNet b = BbsListNet::ReadBbsDataNet(net.dir);
    if (b.empty()) {
      LOG(ERROR) << "ERROR: Unable to read BBSDATA.NET.";
      LOG(ERROR) << "Have you run network3?";
      return 3;
    }

    const net_system_list_rec* fake_ftn_node = b.node_config_for(net.fido.fake_outbound_node);
    if (!fake_ftn_node) {
      LOG(ERROR) << "Can not find node for Fake FTN address: " << net.fido.fake_outbound_node;
      LOG(ERROR) << "Have you run network3?";
      return 3;
    }

    auto cmds = cmdline.remaining();
    if (cmds.empty()) {
      LOG(ERROR) << "No command specified. Exiting.";
      return 1;
    }

    const string cmd = cmds.front();
    cmds.erase(cmds.begin());
    LOG(INFO) << "Command: " << cmd;
    LOG(INFO) << "Args: ";
    for (const auto& r : cmds) {
      LOG(INFO) << r << endl;
    }

    if (cmd == "import") {
      const std::vector<string> extensions{"su?", "mo?", "tu?", "we?", "th?", "fr?", "sa?"};
      auto net_dir = net.dir;
      File::MakeAbsolutePath(net_cmdline.config().root_directory(), &net_dir);
      auto tempdir = net.fido.inbound_dir;
      File::MakeAbsolutePath(net_dir, &tempdir);
      for (const auto& ext : extensions) {
        import_bundles(net_cmdline.config(), net, tempdir, StrCat("*.", ext));
#ifndef _WIN32
        string uext = ext;
        StringUpperCase(&uext);
        import_bundle(net, net_dir, uext);
#endif
      }
    } else if (cmd == "export") {

    } else {
      LOG(ERROR) << "Unknown command: " << cmd;
      return 1;
    }
    return 0;
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [networkf]: " << e.what();
  }
  return 2;
}
