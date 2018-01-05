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
#include "core/findfiles.h"
#include "core/version.h"
#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "core/connection.h"
#include "networkb/net_util.h"
#include "sdk/fido/fido_util.h"
#include "networkb/packets.h"
#include "networkb/ppp_config.h"

#include "sdk/bbslist.h"
#include "sdk/callout.h"
#include "sdk/connect.h"
#include "sdk/config.h"
#include "sdk/contact.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"
#include "sdk/ftn_msgdupe.h"
#include "sdk/networks.h"
#include "sdk/subscribers.h"
#include "sdk/fido/fido_address.h"
#include "sdk/fido/fido_callout.h"
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
using namespace wwiv::sdk::fido;
using namespace wwiv::stl;
using namespace wwiv::os;
using namespace wwiv::sdk::fido;


static string determine_arc_extension(const std::string& filename) {
  File f(filename);
  if (!f.Open(File::modeReadOnly)) {
    return "";
  }

  char header[10];
  auto num_read = f.Read(&header, 10);
  if (num_read < 10) {
    return "";
  }

  switch (header[0]) {
  case 0x60:
    if ((unsigned char)header[1] == (unsigned char)0xEA)
      return "ARJ";
    break;
  case 0x1a:
    return "ARC";
  case 'P':
    if (header[1] == 'K')
      return "ZIP";
    break;
  case 'R':
    if (header[1] == 'a')
      return "RAR";
    break;
  case 'Z':
    if (header[1] == 'O' && header[2] == 'O')
      return "ZOO";
    break;
  }
  if (header[0] == 'P') {
    return "";
  }
  header[9] = 0;
  if (strstr(header, "-lh")) {
    return "LHA";
  }
  return "";
}

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

static string get_echomail_areaname(const std::string& text) {
  auto lines = split_message(text);
  for (const auto& line : lines) {
    if (starts_with(line, "AREA:")) {
      return line.substr(5);
    }
  }
  return "";
}

static bool import_packet_file(const Config& config, FtnMessageDupe& dupe, const FidoCallout& callout, const net_networks_rec& net, const std::string& dir, const string& name) {
  VLOG(1) << "import_packet_file: " << dir << name;
  using wwiv::sdk::fido::ReadPacketResponse;

  File f(dir, name);
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    LOG(INFO) << "Unable to open file: " << dir << name;
    return false;
  }

  bool done = false;
  packet_header_2p_t header{};
  auto num_header_read = f.Read(&header, sizeof(packet_header_2p_t));
  if (num_header_read < sizeof(packet_header_2p_t)) {
    LOG(ERROR) << "Read less than packet header";
    return false;
  }

  FidoAddress address(header.orig_zone, header.orig_net, header.orig_node, header.orig_point, "");
  string expected = callout.packet_config_for(address).packet_password;
  string actual = header.password;
  if (!iequals(expected, actual)) {
    LOG(ERROR) << "Unexpected packet password from node: " << address
      << "; actual: '" << actual << "; expected: " << expected << "'";
    // Move to BADMSGS
    f.Close();
    wwiv::sdk::fido::FtnDirectories dirs(config.root_directory(), net);
    const string dest = FilePath(dirs.bad_packets_dir(), f.GetName());

    if (!File::Move(f.full_pathname(), dest)) {
      LOG(ERROR) << "Error moving file to BADMSGS; file: " << f;
    }
    return false;
  }

  while (!done) {
    FidoPackedMessage msg;
    ReadPacketResponse response = read_packed_message(f, msg);
    if (response == ReadPacketResponse::END_OF_FILE) {
      return true;
    } else if (response == ReadPacketResponse::ERROR) {
      return false;
    }

    if (dupe.is_dupe(msg)) {
      LOG(ERROR) << "Skipping duplicate FTN message: " << msg.vh.subject;
      continue;
    }
    dupe.add(msg);

    bool is_email = (msg.nh.attribute & MSGPRIVATE);
    net_header_rec nh{};
    nh.daten = static_cast<uint32_t>(fido_to_daten(msg.vh.date_time));
    nh.fromsys = FTN_FAKE_OUTBOUND_NODE;
    nh.fromuser = 0;
    nh.list_len = 0;
    if (is_email) {
      nh.main_type = main_type_email_name;
    } else {
      nh.main_type = main_type_new_post;
    }

    nh.method = 0;
    nh.minor_type = 0;
    nh.tosys = 1; // always 1 in new fido
    nh.touser = 0;

    auto from_address = get_address_from_origin(msg.vh.text);

    std::string text;
    std::string s1;
    if (is_email) {
      // TO_USER<nul>SUBJECT<nul>SENDER_NAME<cr / lf>DATE_STRING<cr / lf>MESSAGE_TEXT.
      s1 = msg.vh.to_user_name;
    } else {
      // SUBTYPE<nul>TITLE<nul>SENDER_NAME<cr / lf>DATE_STRING<cr / lf>MESSAGE_TEXT.
      s1 = get_echomail_areaname(msg.vh.text);
    }
    text.append(s1);

    text.push_back(0);
    text.append(msg.vh.subject);
    text.push_back(0);
    text.append(StrCat(msg.vh.from_user_name, "(", from_address, ")\r\n"));
    auto dt = fido_to_daten(msg.vh.date_time);
    text.append(daten_to_wwivnet_time(dt));
    text.append("\r\n");

    if (!is_email) {
      // Add ^D0FidoAddr for the "To:" name of the post.
      static const string kFidoAddr = "\x04""0FidoAddr: ";
      string to_name = msg.vh.to_user_name;
      if (to_name.empty()) {
        // If for some screwy reason we don't have a to name, address
        // it to 'All'.
        LOG(WARNING) << "Somehow have empty msg.vh.to_user_name";
        to_name = "All";
      }
      text.append(kFidoAddr).append(msg.vh.to_user_name).append("\r\n");
    }
    text.append(FidoToWWIVText(msg.vh.text));

    nh.length = text.size();
    // Create file, write to LOCAL.NET for network2 to import.
    Packet packet(nh, {}, text);
    if (!write_wwivnet_packet(LOCAL_NET, net, packet)) {
      LOG(ERROR) << "ERROR Writing WWIV packet for message: " << packet.nh.main_type << "/" << packet.nh.minor_type;
    } else {
      if (is_email) {
        LOG(INFO) << "     + Imported Email '" << msg.vh.subject << "' to '" << s1;
      } else {
        LOG(INFO) << "     + Imported Post '" << msg.vh.subject << "' in area '" << s1;
      }
    }
  }

  return true;
}

static bool import_packets(const Config& config, FtnMessageDupe& dupe, const FidoCallout& callout, const net_networks_rec& net, const std::string& dir, const std::string& mask) {
  VLOG(1) << "Importing packets from: " << dir;
  FindFiles files(FilePath(dir, mask), FindFilesType::files);
  if (files.empty()) {
    LOG(INFO) << "No packets to import in: " << dir;
  }
  for (const auto& f : files) {
    if (import_packet_file(config, dupe, callout, net, dir, f.name)) {
      LOG(INFO) << "Successfully imported packet: " << FilePath(dir, f.name);
      File::Remove(dir, f.name);
    }
  }
  return true;
}

static bool import_bundle_file(const Config& config, FtnMessageDupe& dupe, const FidoCallout& callout, const net_networks_rec& net, const std::string& dir, const string& name) {
  VLOG(1) << "import_bundle_file: name: " << name;

  {
    // Check to make sure the file is readable.
    File f(dir, name);
    if (!f.Open(File::modeBinary | File::modeReadOnly)) {
      LOG(INFO) << "Unable to open file: " << dir << name;
      return false;
    }
  }

  const std::string saved_dir = File::current_directory();
  ScopeExit at_exit([=] { File::set_current_directory(saved_dir); });
  wwiv::sdk::fido::FtnDirectories dirs(config.root_directory(), net);
  File::set_current_directory(dirs.temp_inbound_dir());

  // were in the temp dir now.
  auto arcs = read_arcs(config.datadir());
  if (arcs.empty()) {
    LOG(ERROR) << "No archivers defined!";
    return false;
  }

  string extension = determine_arc_extension(FilePath(dir, name));
  if (extension.empty()) {
    LOG(INFO) << "Unable to determine archiver type for packet: " << name;
    extension = net.fido.packet_config.compression_type;
  }
  const auto& arc = find_arc(arcs, extension);
  // We have no parameter 2 since we're extracting everything.
  string unzip_cmd = arc_stuff_in(arc.arce, FilePath(dir, name), "");
  // Execute the command
  LOG(INFO) << "Command: " << unzip_cmd;
  if (system(unzip_cmd.c_str()) != 0) {
    LOG(ERROR) << "Failed executing: " << unzip_cmd;
    return false;
  }
  // Need to be back home.
  File::set_current_directory(saved_dir);

  import_packets(config, dupe, callout, net, dirs.temp_inbound_dir(), "*.pkt");
#ifndef _WIN32
  import_packets(config, dupe, callout, net, dirs.temp_inbound_dir(), "*.PKT");
#endif  // _WIN32
  return true;
}

static bool import_bundles(const Config& config, const FidoCallout& callout,
  const net_networks_rec& net, const std::string& dir, const std::string& mask) {

  FtnMessageDupe dupe(config);

  VLOG(1) << "import_bundles: mask: " << mask;
  FindFiles files(FilePath(dir, mask), FindFilesType::files);
  for (const auto& f : files) {
    if (f.size == 0) {
      // skip zero byte files.
      continue;
    }
    string lname = ToStringLowerCase(f.name);
    if (ends_with(lname, ".pkt")) {
      if (import_packet_file(config, dupe, callout, net, dir, f.name)) {
        LOG(INFO) << "Successfully imported packet: " << FilePath(dir, f.name);
        File::Remove(dir, f.name);
      }
    } else if (import_bundle_file(config, dupe, callout, net, dir, f.name)) {
      LOG(INFO) << "Successfully imported bundle: " << FilePath(dir, f.name);
      File::Remove(dir, f.name);
    }
  }
  return true;
}

static bool create_ftn_bundle(const Config& config, const FidoCallout& fido_callout, const FidoAddress& dest, 
    const FidoAddress& route_to, const net_networks_rec& net, const string& fido_packet_name, 
    std::string& out_bundle_name) {
  // were in the temp dir now.
 auto arcs = read_arcs(config.datadir());
  if (arcs.empty()) {
    LOG(ERROR) << "No archivers defined!";
    return false;
  }

  auto now = time(nullptr);
  auto tm = localtime(&now);
  auto dow = tm->tm_wday;

  const std::string saved_dir = File::current_directory();
  ScopeExit at_exit([=] { File::set_current_directory(saved_dir); });

  wwiv::sdk::fido::FtnDirectories dirs(config.root_directory(), net);
  const string ctype = fido_callout.packet_config_for(route_to).compression_type;

  if (ctype == "PKT") {
    // No bundles, only packet files.
    string in = FilePath(dirs.temp_outbound_dir(), fido_packet_name);
    string out = FilePath(dirs.outbound_dir(), fido_packet_name);
    if (!File::Move(in, out)) {
      LOG(ERROR) << "Unable to move packet file into outbound dir. file: " << fido_packet_name;
      return false;
    }
    LOG(INFO) << "Created bundle(packet): " << FilePath(dirs.outbound_dir(), fido_packet_name);
    out_bundle_name = fido_packet_name;
    return true;
  }

  FidoAddress orig(net.fido.fido_address);
  for (int i = 0; i < 35; i++) {
    string bname = bundle_name(orig, route_to, dow, i);
    if (File::Exists(dirs.outbound_dir(), bname)) {
      VLOG(1) << "Skipping candidate bundle: " << FilePath(dirs.outbound_dir(), bname);
      // Already exists.
      continue;
    }
    File::set_current_directory(dirs.outbound_dir());
    const auto& arc = find_arc(arcs, ctype);
    // We have no parameter 2 since we're extracting everything.
    string zip_cmd = arc_stuff_in(arc.arca, FilePath(dirs.outbound_dir(), bname), 
      FilePath(dirs.temp_outbound_dir(), fido_packet_name));
    // Execute the command
    LOG(INFO) << "Command: " << zip_cmd;
    if (0 != system(zip_cmd.c_str())) {
      LOG(ERROR) << "Failed executing: " << zip_cmd;
      return false;
    }
    // Need to be back home.
    File::set_current_directory(saved_dir);
    out_bundle_name = bname;

    LOG(INFO) << "Created bundle: " << FilePath(dirs.outbound_dir(), bname);
    if (!File::Remove(dirs.temp_outbound_dir(), fido_packet_name)) {
      LOG(ERROR) << "Error removing packet: " << FilePath(dirs.temp_outbound_dir(), fido_packet_name);
    }
    return true;
  }
  return false;
}

static bool CleanupWWIVName(std::string& sender_name) {
  // #NN, @NODE or (FIDO_ADDR)
  string::size_type idx = sender_name.find_first_of("#@(");
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
    // HACK until the double \004 in wwiv has been out in 5.2
    // for a while (fixed 2016-11-22).  Let's leave this here until
    // 5.2 is GA since some may use net52 with wwiv51.
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

static packet_header_2p_t CreateType2PlusPacketHeader(
  const FidoAddress& from_address, const FidoAddress& dest, 
  time_t now, const std::string& packet_password) {

  packet_header_2p_t header = {};
  header.orig_zone = from_address.zone();
  header.orig_net = from_address.net();
  header.orig_node = from_address.node();
  header.orig_point = from_address.point();
  header.dest_zone = dest.zone();
  header.dest_net = dest.net();
  header.dest_node = dest.node();
  header.dest_point = dest.point();

  auto tm = localtime(&now);
  header.year = static_cast<uint16_t>(tm->tm_year);
  header.month = static_cast<uint16_t>(tm->tm_mon);
  header.day = static_cast<uint16_t>(tm->tm_mday);
  header.hour = static_cast<uint16_t>(tm->tm_hour);
  header.minute = static_cast<uint16_t>(tm->tm_min);
  header.second = static_cast<uint16_t>(tm->tm_sec);
  header.baud = 33600;
  header.packet_ver = 2;
  header.product_code_high = 0x1d;
  header.product_code_low = 0xff;
  header.qm_dest_zone = dest.zone();
  header.qm_orig_zone = from_address.zone();
  header.capabilities = 0x0001;
  // Ideally we'd just use bswap_16 if it was available everywhere.
  header.capabilities_valid =
    ((header.capabilities & 0xff00) >> 8) | ((header.capabilities & 0xff) << 8);
  header.product_code_high = 0;
  header.product_code_low = static_cast<uint8_t>(wwiv_net_version & 0xff);
  // Add in packet password.
  to_char_array(header.password, packet_password);

  return header;
}

static string remove_fido_addr(string to_user) {
  string to_user_new = "";

  for (auto i=0; i < size_int(to_user); i++) {
    if (to_user[i] == '(') {
      break;
    } else if (to_user[i] == ' ' && to_user[i+1] == '#') {
      break;
    }
    to_user_new += to_user[i];
  }
  return to_user_new;
}

static bool create_ftn_packet(const Config& config, const FidoCallout& fido_callout, 
  const FidoAddress& dest, const FidoAddress& route_to, const net_networks_rec& net,
  const Packet& wwivnet_packet, string& fido_packet_name) {

  VLOG(1) << "create_ftn_packet: dest: " << dest << "; route: " << route_to;
  using wwiv::net::ReadPacketResponse;

  FtnDirectories dirs(config.root_directory(), net);

  FidoAddress from_address(net.fido.fido_address);
  for (int tries = 0; tries < 10; tries++) {
    time_t now = time(nullptr);
    File file(dirs.temp_outbound_dir(), packet_name(now));
    if (!file.Open(File::modeCreateFile | File::modeExclusive | File::modeReadWrite | File::modeBinary, File::shareDenyReadWrite)) {
      LOG(INFO) << "Will try again: Unable to create packet file: " << file.full_pathname();
      sleep_for(std::chrono::seconds(1));
      continue;
    }

    auto pw = fido_callout.packet_config_for(route_to).packet_password;
    auto header = CreateType2PlusPacketHeader(from_address, route_to, now, pw);

    if (!write_fido_packet_header(file, header)) {
      LOG(ERROR) << "Error writing packet header.";
      return false;
    }

    bool is_email = (wwivnet_packet.nh.main_type == main_type_email || wwivnet_packet.nh.main_type == main_type_email_name);
    const auto raw_text = wwivnet_packet.text;
    auto iter = raw_text.cbegin();

    string subtype;
    string to_user_name;
    // or we can put code in for email here??

    if (is_email) {
      to_user_name = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
      CleanupWWIVName(to_user_name);
    } else {
      subtype = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
    }
    auto title = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
    auto sender_name = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
    auto date_string = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);

    // TODO(rushfan: These next 2 here should be done differently. We should
    // split the message here and look for these in all lines.  For the By:
    // line we just want to remove it since it's useless.
    if (!is_email) {
      to_user_name = get_fido_addr(raw_text, iter, {'\0', '\r', '\n'}, 80);
    }

    if (!is_email && iter_starts_with(raw_text, iter, "BY: ")) {
      // Skip BY line.
      get_message_field(raw_text, iter, {'\r', '\n'}, 80);
    }

    // Clean up sender name.
    CleanupWWIVName(sender_name);
    auto bbs_text = WWIVToFidoText(string(iter, raw_text.end()));
    
    fido_variable_length_header_t vh{};
    vh.date_time = daten_to_fido(wwivnet_packet.nh.daten);
    vh.from_user_name = sender_name;
    vh.subject = title;
    if (!to_user_name.empty()) {
      auto username_only = remove_fido_addr(to_user_name);
      vh.to_user_name = properize(username_only);
    } else {
      vh.to_user_name = "All";
    }

    FtnMessageDupe dupe(config);
    auto msgid = FtnMessageDupe::GetMessageIDFromWWIVText(raw_text);
    bool needs_msgid = false;
    if (msgid.empty()) {
      // Create a new MSGID if the BBS didn't put one in there already.
      msgid = dupe.CreateMessageID(from_address);
      needs_msgid = true;
    }

    // TODO(rushfan): need to add in INTL for netmails, and all that nonsense.
    // We probably have other stuff we need to add for echomail too.
    std::ostringstream text;
    if (is_email) {
      text << "\001" << "INTL " << dest << " " << from_address << "\r";
    } else {
      text << "AREA:" << subtype << "\r";
    }
    // As of 5.3, the PID is added by the BBS software.
    // text << "\001PID: WWIV " << wwiv_version << beta_version << "\r";
    text << "\001TID: WWIV NET" << wwiv_net_version << beta_version << "\r";
    if (needs_msgid) {
      text << "\001MSGID: " << msgid << "\r";
    }
    // Implement FTS-5003. [http://ftsc.org/docs/fts-5003.001]
    // All outbound WWIV messages are always CP437.
    text << "\001CHRS: CP437 2\r";
    
    // Implement FRL-1004. [http://ftsc.org/docs/frl-1004.002]
    text << "\001TZUTC: " << tz_offset_from_utc() << "\r";

    // TODO(rushfan): We should rip through the bbs_text here.
    // and add in any special kludges like ^AREPLY here.
    // Add the text from the message (as entered from the BBS).
    text << bbs_text;

    // Now we need tear + origin lines
    auto origin_line = net.fido.origin_line;
    if (origin_line.empty()) {
      // default origin line to system name if it doesn't exist.
      origin_line = config.system_name();
    }

    if (from_address.point() == 0) {
      text << "\r"
        << "--- WWIV " << wwiv_version << beta_version << "\r"
        << " * Origin: " << origin_line << " (" << to_zone_net_node(from_address) << ")\r";
    } else {
      text << "\r"
        << "--- WWIV " << wwiv_version << beta_version << "\r"
        << " * Origin: " << origin_line << " (" << to_zone_net_node_point(from_address) << ")\r";
    }
    // Finally we need SEEN-BY and PATH lines for routing.
    if (!is_email) {
      // TODO(rushfan): Add the nodes we are exporting this to.
      text << "SEEN-BY: " << to_net_node(from_address) << "\r\r";
      // Also we need to add a ^APATH: line here, starting with us.
    }

    vh.text = text.str();

    fido_packed_message_t nh{};
    nh.message_type = 2;
    nh.attribute = 0;
    nh.cost = 0;
    nh.orig_net = from_address.net();
    nh.orig_node = from_address.node();
    nh.dest_net = dest.net();
    nh.dest_node = dest.node();
    nh.attribute = MSGLOCAL;

    if (wwivnet_packet.nh.main_type == main_type_email_name) {
      nh.attribute |= MSGPRIVATE;
    }

    FidoPackedMessage p(nh, vh);
    if (!write_packed_message(file, p)) {
      LOG(ERROR) << "Error writing packed message.";
      return false;
    }

    // Since we wrote the packed message, let's add it to the 
    // duplicate message database.
    dupe.add(p);
    fido_packet_name = file.GetName();
    return true;
  }
  return false;
}

static bool create_ftn_packet_and_bundle(
    const NetworkCommandLine& net_cmdline, const FidoCallout& fido_callout, const FidoAddress& dest,
    const FidoAddress& route_to, const net_networks_rec& net, const Packet& p, string& bundlename) {
  LOG(INFO) << "Creating packet for subscriber: " << dest << "; route_to: " << route_to;
  wwiv::sdk::fido::FtnDirectories dirs(net_cmdline.config().root_directory(), net);
  string fido_packet_name;
  if (!create_ftn_packet(net_cmdline.config(), fido_callout, dest, route_to, net, p, fido_packet_name)) {
    LOG(ERROR) << "Failed to create FTN packet.";
    write_wwivnet_packet(DEAD_NET, net, p);
    return false;
  }
  LOG(INFO) << "Created packet: " << FilePath(dirs.temp_outbound_dir(), fido_packet_name);

  if (!create_ftn_bundle(net_cmdline.config(), fido_callout, dest, route_to, net, fido_packet_name, bundlename)) {
    LOG(ERROR) << "Failed to create FTN bundle.";
    write_wwivnet_packet(DEAD_NET, net, p);
    return false;
  }
  return true;
}

static string NextNetmailFilePath(const string& dir) {
  for (int i = 2; i < 10000; i++) {
    string candidate = FilePath(dir, StrCat(i, ".msg"));
    if (!File::Exists(candidate)) {
      return candidate;
    }
  }
  return "";
}

static bool CreateFidoNetAttachNetMail(const FidoAddress& orig, const FidoAddress& dest, const string& netmail_filename, const string& bundle_path, const fido_packet_config_t& packet_config) {
  File netmail(netmail_filename);
  if (!netmail.Open(File::modeBinary | File::modeCreateFile | File::modeExclusive | File::modeReadWrite, File::shareDenyReadWrite)) {
    LOG(ERROR) << "Unable to open netmail filen: '" << netmail.full_pathname() << "'";
    return false;
  }

  // FTC-0053 flags
  string flags = "FLAGS FIL TFS PVT";
  fido_stored_message_t h{};
  h.attribute = (MSGFILE | MSGKILL | MSGLOCAL);
  const auto& st = packet_config.netmail_status;
  switch (st) {
  case fido_bundle_status_t::hold:h.attribute |= MSGHOLD;
    flags += " HLD";
    break;
  case fido_bundle_status_t::direct:
    h.attribute |= MSGCRASH;
    flags += " CRA";
    break;
  case fido_bundle_status_t::crash:
    h.attribute |= MSGCRASH;
    flags += " IMM";
    break;
  case fido_bundle_status_t::unknown:
  case fido_bundle_status_t::normal: // NOP
    break;
  }

  string from = StrCat("WWIV NET ", wwiv_version, beta_version);
  string to = "SYSOP";

  h.cost = 0;
  to_char_array(h.date_time, daten_to_fido(time(nullptr)));
  h.dest_net = dest.net();
  h.dest_node = dest.node();
  h.dest_point = dest.point();
  h.dest_zone = dest.zone();
  to_char_array(h.from, from);
  h.next_reply = 0;
  h.orig_net = orig.net();
  h.orig_node = orig.node();
  h.orig_point = orig.point();
  h.orig_zone = orig.zone();
  to_char_array(h.subject, bundle_path);
  to_char_array(h.to, to);

  string msgid;
  std::ostringstream text;

  // FTC-0053 flags
  text << "\001" << flags << "\r";
  // FTS-4001 INTL line
  text << "\001" << "INTL " << dest << " " << orig << "\r";
  text << "\001PID: WWIV " << wwiv_version << beta_version << "\r"
    << "\001TID: WWIV NET" << wwiv_net_version << beta_version << "\r";
  if (!msgid.empty()) {
    text << "\001MSGID: " << msgid << "\r";
  }

  FidoStoredMessage m(h, text.str());
  write_stored_message(netmail, m);

  return true;
}

bool CreateFloFile(const NetworkCommandLine& net_cmdline, const FidoAddress& dest, const net_networks_rec& net, const string& bundlename, const fido_packet_config_t& packet_config) {
  FidoAddress orig(net.fido.fido_address);
  wwiv::sdk::fido::FtnDirectories dirs(net_cmdline.config().root_directory(), net);

  const string floname = flo_name(dest, packet_config.netmail_status);
  const string bsyname = net_node_name(dest, "bsy");

  for (int i = 1; i < 7; i++) {
    File bsy(dirs.outbound_dir(), bsyname);
    if (bsy.Open(File::modeCreateFile | File::modeExclusive | File::modeWriteOnly, File::shareDenyReadWrite)) {
      break;
    }
    if (bsy.Exists()) {
      LOG(ERROR) << "BSY file: '" << bsy.full_pathname() << "' already exists. Will try again...";
    } else {
      LOG(ERROR) << "Unable to create BSY file: '" << bsy.full_pathname() << "'. Will try again...";
    }
    wwiv::os::sleep_for(std::chrono::milliseconds((i ^ 2) * 50));
  }
  ScopeExit at_exit([=] { File::Remove(dirs.outbound_dir(), bsyname); });
  TextFile flo_file(dirs.outbound_dir(), floname, "a+");
  if (!flo_file.IsOpen()) {
    LOG(ERROR) << "Unable to open FLO file: " << flo_file.full_pathname();
    return false;
  }
  int num_written = flo_file.WriteLine(StrCat("^", FilePath(dirs.outbound_dir(), bundlename)));
  return num_written > 0;
}

bool CreateNetmailAttach(const NetworkCommandLine& net_cmdline,const FidoAddress& dest, const net_networks_rec& net, const string& bundlename, const fido_packet_config_t& packet_config) {
  wwiv::sdk::fido::FtnDirectories dirs(net_cmdline.config().root_directory(), net);
  string netmail_filepath = NextNetmailFilePath(dirs.netmail_dir());

  if (netmail_filepath.empty()) {
    LOG(ERROR) << "Unable to figure out netmail filename in dir: '" << dirs.netmail_dir() << "'";
    return false;
  }
  const string bundlepath = FilePath(dirs.outbound_dir(), bundlename);
  if (!CreateFidoNetAttachNetMail(FidoAddress(net.fido.fido_address), dest, netmail_filepath, bundlepath, packet_config)) {
    LOG(ERROR) << "Unable to create netmail: " << netmail_filepath;
    return false;
  }
  LOG(INFO) << "Wrote attach netmail: " << netmail_filepath;
  return true;
}

static bool CreateNetmailAttachOrFloFile(const NetworkCommandLine& net_cmdline, const FidoAddress& dest, const net_networks_rec& net, const string& bundlename, const fido_packet_config_t& packet_config) {
  if (net.fido.mailer_type == fido_mailer_t::attach) {
    return CreateNetmailAttach(net_cmdline, dest, net, bundlename, packet_config);
  } else if (net.fido.mailer_type == fido_mailer_t::flo) {
    return CreateFloFile(net_cmdline, dest, net, bundlename, packet_config);
  } else {
    LOG(ERROR) << "Unknown mailer type: " << static_cast<int>(net.fido.mailer_type);
    return false;
  }
}

static FidoAddress find_route_to(const FidoAddress& dest, const FidoCallout& callout, const fido_packet_config_t& packet_config) {
  if (packet_config.netmail_status == fido_bundle_status_t::direct) {
    return dest;
  }

  FidoAddress a = FindRouteToAddress(dest, callout);
  if (a.node() == 0) {
    // is this right? returning direct if we have no route?
    return dest;
  }
  return a;
}

static bool export_main_type_new_post(const NetworkCommandLine& net_cmdline, const net_networks_rec& net, const FidoCallout& fido_callout, std::set<string>& bundles, Packet& p) {
  // Lame implementation that creates 1 file per message.
  auto raw_text = p.text;
  auto it = p.text.cbegin();
  string subtype = get_message_field(p.text, it, {'\0', '\r', '\n'}, 80);
  LOG(INFO) << "Creating packet for subtype: " << subtype;

  auto net_dir = File::absolute(net_cmdline.config().root_directory(), net.dir);
  auto subscribers = ReadFidoSubcriberFile(net_dir, StrCat("n", subtype, ".net"));
  if (subscribers.empty()) {
    LOG(INFO) << "There are no subscribers on echo: '" << subtype << "'. Nothing to do!";
  }
  for (const auto& sub : subscribers) {
    string bundlename;
    auto packet_config = fido_callout.packet_config_for(sub);
    auto route_to = find_route_to(sub, fido_callout, packet_config);
    if (!create_ftn_packet_and_bundle(net_cmdline, fido_callout, sub, route_to, net, p, bundlename)) {
      continue;
    }
    if (!contains(bundles, bundlename)) {
      // We only want to attach the bundle (or add it to the flo file)
      // one time, so skip ones that have already been done.
      bundles.insert(bundlename);
      auto route_packet_config = fido_callout.packet_config_for(route_to);
      CreateNetmailAttachOrFloFile(net_cmdline, route_to, net, bundlename, route_packet_config);
    }
  }
  return true;
}

bool export_main_type_email_name(const NetworkCommandLine& net_cmdline, const net_networks_rec& net, const FidoCallout& fido_callout, std::set<string>& bundles, Packet& p) {
  // Lame implementation that creates 1 file per message.
  LOG(INFO) << "Creating packet for netmail.";

  string bundlename;
  auto it = p.text.begin();
  auto to = get_message_field(p.text, it, {0}, 80);
  auto dest = get_address_from_single_line(to);
  if (dest.node() == 0) {
    LOG(ERROR) << "Unable to get address from to line: " << to;
    return false;
  }

  // todo - actually we need a new way of making the ftn packet that works right
  // with netmail
  auto packet_config = fido_callout.packet_config_for(dest);
  FidoAddress route_to = find_route_to(dest, fido_callout, packet_config);
  if (create_ftn_packet_and_bundle(net_cmdline, fido_callout, dest, route_to, net, p, bundlename)) {
    if (!contains(bundles, bundlename)) {
      // We only want to attach the bundle (or add it to the flo file)
      // one time, so skip ones that have already been done.
      bundles.insert(bundlename);
      CreateNetmailAttachOrFloFile(net_cmdline, route_to, net, bundlename, fido_callout.packet_config_for(dest));
    }
  }
  return true;
}


int main(int argc, char** argv) {
  Logger::Init(argc, argv);
  try {
    ScopeExit at_exit(Logger::ExitLogger);
    CommandLine cmdline(argc, argv, "net");
    NetworkCommandLine net_cmdline(cmdline);
    if (!net_cmdline.IsInitialized() || cmdline.help_requested()) {
      ShowHelp(cmdline);
      return 1;
    }

    const auto& net = net_cmdline.network();
    LOG(INFO) << "NETWORKF for network: " << net.name;

    if (net.type != network_type_t::ftn) {
      LOG(ERROR) << "NETWORKF is only for use on FTN type networks.";
      ShowHelp(cmdline);
      return 1;
    }

    VLOG(1) << "Reading BBSDATA.NET..";
    BbsListNet b = BbsListNet::ReadBbsDataNet(net.dir);
    if (b.empty()) {
      LOG(ERROR) << "ERROR: Unable to read BBSDATA.NET.";
      LOG(ERROR) << "       Do you need to run network3?";
      return 3;
    }

    const net_system_list_rec* fake_ftn_node = b.node_config_for(FTN_FAKE_OUTBOUND_NODE);
    if (!fake_ftn_node) {
      LOG(ERROR) << "Can not find node for outbound FTN address.";
      LOG(ERROR) << "       Do you need to run network3?";
      return 3;
    }

    FidoCallout fido_callout(net_cmdline.config(), net);
    if (!fido_callout.IsInitialized()) {
      LOG(ERROR) << "Unable to initialize fido_callout.";
      return 1;
    }

    auto cmds = cmdline.remaining();
    if (cmds.empty()) {
      LOG(ERROR) << "No command specified. Exiting.";
      ShowHelp(cmdline);
      return 1;
    }

    const string cmd = cmds.front();
    cmds.erase(cmds.begin());
    VLOG(1) << "Command: " << cmd;
    VLOG(1) << "Args: ";
    for (const auto& r : cmds) {
      VLOG(1) << r << endl;
    }

    wwiv::sdk::fido::FtnDirectories dirs(net_cmdline.config().root_directory(), net);
    if (cmd == "import") {
      const std::vector<string> extensions{"su?", "mo?", "tu?", "we?", "th?", "fr?", "sa?", "pkt"};
      for (const auto& ext : extensions) {
        import_bundles(net_cmdline.config(), fido_callout, net, dirs.inbound_dir(), StrCat("*.", ext));
#ifndef _WIN32
        import_bundles(net_cmdline.config(), fido_callout, net, dirs.inbound_dir(), StrCat("*.", ToStringUpperCase(ext)));
#endif
      }
    } else if (cmd == "export") {
      const string sfilename = StrCat("s", FTN_FAKE_OUTBOUND_NODE, ".net");
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
      std::set<std::string> bundles;
      while (!done) {
        Packet p;
        wwiv::net::ReadPacketResponse response = read_packet(f, p, true);
        if (response == wwiv::net::ReadPacketResponse::END_OF_FILE) {
          // Delete the packet.
          f.Close();
          f.Delete();
          break;
        } else if (response == wwiv::net::ReadPacketResponse::ERROR) {
          return 1;
        }

        if (p.nh.main_type == main_type_new_post) {
          if (!export_main_type_new_post(net_cmdline, net, fido_callout, bundles, p)) {
            LOG(ERROR) << "Error exporting post.";
          }
        } else if (p.nh.main_type == main_type_email_name) {
          if (!export_main_type_email_name(net_cmdline, net, fido_callout, bundles, p)) {
            LOG(ERROR) << "Error exporting email.";
          }
        } else {
          LOG(ERROR) << "Unhandled type: " << main_type_name(p.nh.main_type);
          // Let's write it to dead.net
          if (!write_wwivnet_packet(DEAD_NET, net, p)) {
            LOG(ERROR) << "Error writing to DEAD.NET";
          }
        }
      }

    } else {
      LOG(ERROR) << "Unknown command: " << cmd;
      ShowHelp(cmdline);
      return 1;
    }
    return 0;
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [networkf]: " << e.what();
  }
  return 2;
}
