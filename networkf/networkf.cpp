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
#include "core/version.h"
#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "networkb/connection.h"
#include "networkb/net_util.h"
#include "networkb/fido_util.h"
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
#include "sdk/subscribers.h"
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
using namespace wwiv::net::fido;
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
  return os.str();
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

// TODO(rushfan): move this somewhere common since it's copied
// from dump_fido_packet.cpp
static std::string FidoToWWIVText(const std::string& ft) {
  std::string wt;
  for (auto& c : ft) {
    if (c == 13) {
      wt.push_back(13);
      wt.push_back(10);
    } else if (c == 0x8d) {
      // FIDOnet style Soft CR
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
    // Create file, write to LOCAL.NET for network2 to import.
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
  ScopeExit at_exit([=] { File::set_current_directory(saved_dir); });
  auto tempdir = net.fido.temp_inbound_dir;
  File::MakeAbsolutePath(net.dir, &tempdir);
  File::set_current_directory(tempdir);


  // were in the temp dir now.
  vector<arcrec> arcs = read_arcs(config.datadir());
  if (arcs.empty()) {
    LOG(ERROR) << "No archivers defined!";
    return false;
  }

  // TODO(rushfan): Need callout.json support to set file specific options here.
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

bool create_ftn_bundle(const Config& config, const FidoAddress& dest, const net_networks_rec& net, const std::string& tempdir, const string& fido_packet_name, std::string& out_bundle_name) {

  // were in the temp dir now.
  vector<arcrec> arcs = read_arcs(config.datadir());
  if (arcs.empty()) {
    LOG(ERROR) << "No archivers defined!";
    return false;
  }

  time_t now = time(nullptr);
  auto tm = localtime(&now);
  auto dow = tm->tm_wday;
  int bundle_num = 0;

  const std::string saved_dir = File::current_directory();
  ScopeExit at_exit([=] { File::set_current_directory(saved_dir); });

  string net_dir(net.dir);
  File::MakeAbsolutePath(config.root_directory(), &net_dir);
  string out_dir(net.fido.outbound_dir);
  File::MakeAbsolutePath(net_dir, &out_dir);
  string temp_dir(tempdir);
  File::MakeAbsolutePath(net_dir, &temp_dir);

  FidoAddress orig(net.fido.fido_address);
  for (int i = 0; i < 35; i++) {
    string bname = bundle_name(orig, dest, dow, bundle_num);
    if (File::Exists(temp_dir, bname)) {
      // Already exists.
      continue;
    }
    File::set_current_directory(out_dir);
    // TODO(rushfan): Need callout.json support to set file specific options here.
    const auto& arc = find_arc(arcs, net.fido.packet_config.compression_type);
    // We have no parameter 2 since we're extracting everything.
    string zip_cmd = arc_stuff_in(arc.arca, FilePath(out_dir, bname), FilePath(temp_dir, fido_packet_name));
    // Execute the command
    system(zip_cmd.c_str());
    File::set_current_directory(saved_dir);
    out_bundle_name = bname;
    return true;
  }
  return false;
}

static bool CleanupWWIVName(std::string& sender_name) {
  string::size_type idx = sender_name.find_first_of("#@");
  if (idx != string::npos) {
    sender_name = sender_name.substr(0, idx);
  }
  StringTrim(&sender_name);
  return true;
}

template <typename C, typename I>
static std::string get_control_line(const C& c, I& iter, std::set<char> stop, std::size_t max) {
  // No need to continue if we're already at the end.
  if (iter == c.end()) {
    return "";
  }
  if (*iter != '\004') {
    // #4 is control-D and a WWIV control line.
    return "";
  }
  return get_message_field(c, iter, stop, max);
}

template <typename C, typename I>
static std::string get_fido_addr(const C& c, I& iter, std::set<char> stop, std::size_t max) {
  static const string kFidoAddr = "\x04""0FidoAddr: ";
  string address;
  do {
    address = get_control_line(c, iter, stop, max);
    if (address.empty()) {
      return "";
    }
    if (address.front() != '\004') {
      // Bail if we have a non-control line.
      return "";
    }
    // HACK until I figure out why I get double \004 in wwiv...
    if (address.size() > 2 && address[1] == '\004') {
      address.erase(address.begin());
    }
    if (starts_with(address, kFidoAddr)) {
      // This is the address line.
      return address.substr(kFidoAddr.size());
    }
  } while (!address.empty());
  return "";
}

template <typename C, typename I>
static bool iter_starts_with(const C& c, I& iter, string expected) {
  for (const auto& ch : expected) {
    if (iter == c.end()) {
      return false;
    }
    if (*iter != ch) {
      return false;
    }
    iter++;
  }
  return true;
}

bool create_ftn_packet(const Config& config, const FidoAddress& dest, const net_networks_rec& net, const std::string& tempdir, const Packet& wwivnet_packet, string& fido_packet_name) {
  using wwiv::net::ReadPacketResponse;

  string temp_dir(tempdir);
  {
    string net_dir(net.dir);
    File::MakeAbsolutePath(config.root_directory(), &net_dir);
    File::MakeAbsolutePath(net_dir, &temp_dir);
  }

  FidoAddress address(net.fido.fido_address);
  for (int tries = 0; tries < 10; tries++) {
    time_t now = time(nullptr);
    File file(temp_dir, packet_name(now));
    if (!file.Open(File::modeCreateFile | File::modeExclusive | File::modeReadWrite | File::modeBinary, File::shareDenyReadWrite)) {
      LOG(INFO) << "Will try again: Unable to create packet file: " << file.full_pathname();
      wwiv::os::sleep_for(std::chrono::seconds(1));
      continue;
    }

    packet_header_2p_t header = {};
    header.orig_zone = address.zone();
    header.orig_net = address.net();
    header.orig_node = address.node();
    header.orig_point = address.point();
    header.dest_zone = dest.zone();
    header.dest_net = dest.net();
    header.dest_node = dest.node();
    header.dest_point = dest.point();

    auto tm = localtime(&now);
    header.year = tm->tm_year;
    header.month = tm->tm_mon;
    header.day = tm->tm_mday;
    header.hour = tm->tm_hour;
    header.minute = tm->tm_min;
    header.second = tm->tm_sec;
    header.baud = 33600;
    header.packet_ver = 2;
    header.product_code_high = 0x1d;
    header.product_code_low = 0xff;
    header.qm_dest_zone = dest.zone();
    header.qm_orig_zone = address.zone();
    header.capabilities = 0x0001;
    header.capabilities_valid = 
      ((header.capabilities_valid & 0x7f00) >> 8) | ((header.capabilities_valid & 0xff) << 8);
    header.product_code_high = 0;
    header.product_code_low = wwiv_net_version;
    // Add in packet password.
    to_char_array(header.password, net.fido.packet_config.packet_password);

    if (!write_fido_packet_header(file, header)) {
      LOG(ERROR) << "Error writing packet header.";
      return false;
    }

    const string raw_text = wwivnet_packet.text;
    auto iter = raw_text.cbegin();
    string subtype = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
    string title = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
    string sender_name = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
    string date_string = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
    string fido_addr = get_fido_addr(raw_text, iter, {'\0', '\r', '\n'}, 80);

    if (iter_starts_with(raw_text, iter, "BY: ")) {
      // Skip BY line.
      get_message_field(raw_text, iter, {'\r', '\n'}, 80);
    }

    // Clean up sender name.
    CleanupWWIVName(sender_name);

    string bbs_text = string(iter, raw_text.end());
    // Since WWIV uses CRLF, remove the LF's and we have happy CR's.
    bbs_text.erase(std::remove(bbs_text.begin(), bbs_text.end(), 10), bbs_text.end());
    if (!bbs_text.empty() && bbs_text.back() == '\x1a') {
      bbs_text.pop_back();
    }

    fido_variable_length_header_t vh;
    vh.date_time = daten_to_fido(wwivnet_packet.nh.daten);
    vh.from_user_name = sender_name;
    vh.subject = title;
    if (!fido_addr.empty()) {
      if (fido_addr == "ALL") {
        // WWIV uses all upper case ALL, let's make it look ftn-ish
        fido_addr = "All";
      }
      vh.to_user_name = fido_addr;
    } else {
      vh.to_user_name = "All";
    }

    // TODO(rushfan): need to add in MSGID and all that nonsense.
    std::ostringstream text;
    text << "AREA:" << subtype << "\r"
      << "\001PID: WWIV " << wwiv_version << beta_version << "\r"
      << "\001TID: WWIV NET" << wwiv_net_version << beta_version << "\r"
      << bbs_text << "\r"
      << "--- WWIV " << wwiv_version << beta_version << "\r"
      << " * Origin: Nowhere Yet.\r"
      << "SEEN-BY: " << address.net() << "/" << address.node() << "\r\r";

    vh.text = text.str();

    fido_packed_message_t nh{};
    nh.message_type = 2;
    nh.attribute = 0;
    nh.cost = 0;
    nh.orig_net = address.net();
    nh.orig_node = address.node();
    nh.dest_net = dest.net();
    nh.dest_node = dest.node();
    FidoPackedMessage p(nh, vh);
    if (!write_packed_message(file, p)) {
      LOG(ERROR) << "Error writing packed message.";
      return false;
    }
    fido_packet_name = file.GetName();
    return true;
  }
  return false;
}

string NextNetmailFileName(const string& dir) {
  for (int i = 2; i < 10000; i++) {
    string candidate = FilePath(dir, StrCat(i, ".msg"));
    if (!File::Exists(candidate)) {
      return candidate;
    }
  }
  return "";
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
        import_bundles(net_cmdline.config(), net, tempdir, StrCat("*.", uext));
#endif
      }
    } else if (cmd == "export") {
      const string sfilename = StrCat("s", net.fido.fake_outbound_node, ".net");
      if (!File::Exists(net.dir, sfilename)) {
        LOG(INFO) << "No file '" << sfilename << "' exists to be exported to a FTN packet.";
        return 1;
      }

      // Packet file is created by us for sure.
      File f(net.dir, sfilename);
      if (!f.Open(File::modeBinary | File::modeReadOnly)) {
        LOG(ERROR) << "Unable to open file: " << net.dir << sfilename;
        return 1;
      }

      bool done = false;
      while (!done) {
        Packet p;
        wwiv::net::ReadPacketResponse response = read_packet(f, p);
        if (response == wwiv::net::ReadPacketResponse::END_OF_FILE) {
          break;
        } else if (response == wwiv::net::ReadPacketResponse::ERROR) {
          return 1;
        }

        if (p.nh.main_type == main_type_new_post) {
          string fido_packet_name;
          // Lame implementation that creates 1 file per message.
          string raw_text = p.text;
          auto it = p.text.cbegin();
          string subtype = get_message_field(p.text, it, {'\0', '\r', '\n'}, 80);
          LOG(INFO) << "Creating packet for subtype: " << subtype;

          std::set<FidoAddress> subscribers;
          ReadFidoSubcriberFile(net.dir, StrCat("n", subtype, ".net"), subscribers);
          if (subscribers.empty()) {
            LOG(INFO) << "There are no subscribers on echo: '" << subtype << "'. Nothing to do!";
          }
          for (const auto& sub : subscribers) {
            LOG(INFO) << "Creating packet for subscriber: " << sub.as_string();
            if (!create_ftn_packet(net_cmdline.config(), sub, net, net.fido.temp_inbound_dir, p, fido_packet_name)) {
              // oops. let's skip.
              LOG(ERROR) << "Failed to create FTN packet.";
              write_wwivnet_packet(DEAD_NET, net, p);
              continue;
            }
            LOG(INFO) << "Created packet: " << FilePath(net.fido.temp_inbound_dir, fido_packet_name);
            if (fido_packet_name.empty()) {
              LOG(ERROR) << "Error creating ftn packet name";
              continue;
            }
            string bundle_name;
            if (!create_ftn_bundle(net_cmdline.config(), sub, net, net.fido.temp_inbound_dir, fido_packet_name, bundle_name)) {
              // oops. let's skip.
              LOG(ERROR) << "Failed to create FTN bundle.";
              write_wwivnet_packet(DEAD_NET, net, p);
              continue;
            }
            string net_dir(net.dir);
            File::MakeAbsolutePath(net_cmdline.config().root_directory(), &net_dir);
            string out_dir(net.fido.outbound_dir);
            File::MakeAbsolutePath(net_dir, &out_dir);
            LOG(INFO) << "Created bundle: " << FilePath(out_dir, bundle_name);

            // Delete the file, since we made a bundle.
            File::Remove(net.fido.temp_inbound_dir, fido_packet_name);

            // TODO(rushfan): Create FLO or attach file.
            if (net.fido.mailer_type == fido_mailer_t::attach) {
              string netmail_filename = NextNetmailFileName(net.fido.netmail_dir);
              if (netmail_filename.empty()) {
                LOG(ERROR) << "Unable to figure out netmail filename in dir: '" << net.fido.netmail_dir << "'";
                continue;
              }
              File netmail(netmail_filename);
              if (!netmail.Open(File::modeBinary | File::modeCreateFile | File::modeExclusive | File::modeReadWrite, File::shareDenyReadWrite)) {
                LOG(ERROR) << "Unable to open netmail filen: '" << netmail.full_pathname() << "'";
                continue;
              }
              FidoAddress orig(net.fido.fido_address);
              fido_stored_message_t h{};
              h.attribute = MSGFILE;
              h.cost = 0;
              to_char_array(h.date_time, daten_to_fido(time(nullptr)));
              h.dest_net = sub.net();
              h.dest_node = sub.node();
              h.dest_point = sub.point();
              h.dest_zone = sub.zone();
              to_char_array(h.from, "ARCmail");
              h.next_reply = 0;
              h.orig_net = orig.net();
              h.orig_node = orig.node();
              h.orig_point = orig.point();
              h.orig_zone = orig.zone();
              to_char_array(h.subject, FilePath(out_dir, bundle_name));
              to_char_array(h.to, "ARCmail");
              FidoStoredMessage m(h, "");
              write_stored_message(netmail, m);
              LOG(INFO) << "Wrote attach netmail: " << netmail.full_pathname();
            } else if (net.fido.mailer_type == fido_mailer_t::flo) {
              LOG(ERROR) << "Don't know how to make FLO file.";
            } else {
              LOG(ERROR) << "Unknown mailer type: " << static_cast<int>(net.fido.mailer_type);
            }
          }
        } else {
          LOG(ERROR) << "Unhandled type: " << main_type_name(p.nh.main_type);
          // Let's write it to dead.net
          if (!write_wwivnet_packet(DEAD_NET, net, p)) {
            LOG(ERROR) << "Error writing to DEAD.NET";
          }
        }
      }

      // Create a ftn file.
      // string packet_name = create_ftn_packet(config, net, sfilename);
      // Add it to an existing bundle, of one exists.
      // upsert_bundle(config, net, packet_name);
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
