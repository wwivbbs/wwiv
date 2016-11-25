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
using namespace wwiv::net::fido;
using namespace wwiv::strings;
using namespace wwiv::sdk;
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
  vector<string> lines = split_message(text);
  for (const auto& line : lines) {
    if (starts_with(line, "AREA:")) {
      return line.substr(5);
    }
  }
  return "";
}

static bool import_packet_file(const Config& config, const FidoCallout& callout, const net_networks_rec& net, const std::string& dir, const string& name) {
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
    auto net_dir = File::MakeAbsolutePath(config.root_directory(), net.dir);
    string badmsgs_path = File::MakeAbsolutePath(net_dir, net.fido.bad_packets_dir);
    const string dest = FilePath(badmsgs_path, f.GetName());

    if (!File::Move(f.full_pathname(), dest)) {
      LOG(ERROR) << "Error moving file to BADMSGS; file: " << f;
    }
    return false;
  }

  // TODO(rushfan): move this outside do we only load it once.
  FtnMessageDupe dupe(config);

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

static bool import_packets(const Config& config, const FidoCallout& callout, const net_networks_rec& net, const std::string& dir, const std::string& mask) {
  VLOG(1) << "Importing packets from: " << dir;
  WFindFile files;
  bool has_next = files.open(FilePath(dir, mask), WFINDFILE_FILES);
  if (!has_next) {
    LOG(INFO) << "No packets to import in: " << dir;
  }
  while (has_next) {
    const auto& name = files.GetFileName();
    if (import_packet_file(config, callout, net, dir, name)) {
      LOG(INFO) << "Successfully imported packet: " << FilePath(dir, name);
      File::Remove(dir, name);
    }
    has_next = files.next();
  }
  return true;
}

static bool import_bundle_file(const Config& config, const FidoCallout& callout, const net_networks_rec& net, const std::string& dir, const string& name) {
  VLOG(1) << "import_bundle_file: name: " << name;
  File f(dir, name);
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    LOG(INFO) << "Unable to open file: " << dir << name;
    return false;
  }

  const std::string saved_dir = File::current_directory();
  ScopeExit at_exit([=] { File::set_current_directory(saved_dir); });
  auto net_dir = File::MakeAbsolutePath(config.root_directory(), net.dir);
  auto tempdir = File::MakeAbsolutePath(net_dir, net.fido.temp_inbound_dir);
  File::set_current_directory(tempdir);

  // were in the temp dir now.
  vector<arcrec> arcs = read_arcs(config.datadir());
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

  import_packets(config, callout, net, tempdir, "*.pkt");
#ifndef _WIN32
  import_packets(config, callout, net, tempdir, "*.PKT");
#endif  // _WIN32
  return true;
}

static bool import_bundles(const Config& config, const FidoCallout& callout,
  const net_networks_rec& net, const std::string& dir, const std::string& mask) {
  VLOG(1) << "import_bundles: mask: " << mask;
  WFindFile files;
  bool has_next = files.open(FilePath(dir, mask), WFINDFILE_FILES);
  while (has_next) {
    if (files.GetFileSize() == 0) {
      // skip zero byte files.
      continue;
    }
    const auto& name = files.GetFileName();
    string lname = name;
    StringLowerCase(&lname);
    if (ends_with(lname, ".pkt")) {
      if (import_packet_file(config, callout, net, dir, name)) {
        LOG(INFO) << "Successfully imported packet: " << FilePath(dir, name);
        File::Remove(dir, name);
      }
    } else if (import_bundle_file(config, callout, net, dir, name)) {
      LOG(INFO) << "Successfully imported bundle: " << FilePath(dir, name);
      File::Remove(dir, name);
    }
    has_next = files.next();
  }
  return true;
}

static bool create_ftn_bundle(const Config& config, const FidoCallout& fido_callout, const FidoAddress& dest, 
    const FidoAddress& route_to, const net_networks_rec& net, const string& fido_packet_name, 
    std::string& out_bundle_name) {
  // were in the temp dir now.
  vector<arcrec> arcs = read_arcs(config.datadir());
  if (arcs.empty()) {
    LOG(ERROR) << "No archivers defined!";
    return false;
  }

  time_t now = time(nullptr);
  auto tm = localtime(&now);
  auto dow = tm->tm_wday;

  const std::string saved_dir = File::current_directory();
  ScopeExit at_exit([=] { File::set_current_directory(saved_dir); });

  string net_dir(File::MakeAbsolutePath(config.root_directory(), net.dir));
  string out_dir(File::MakeAbsolutePath(net_dir, net.fido.outbound_dir));
  string temp_dir(File::MakeAbsolutePath(net_dir, net.fido.temp_outbound_dir));
  const string ctype = fido_callout.packet_config_for(route_to).compression_type;

  if (ctype == "PKT") {
    // No bundles, only packet files.
    string in = FilePath(temp_dir, fido_packet_name);
    string out = FilePath(out_dir, fido_packet_name);
    if (!File::Move(in, out)) {
      LOG(ERROR) << "Unable to move packet file into outbound dir. file: " << fido_packet_name;
      return false;
    }
    LOG(INFO) << "Created bundle(packet): " << FilePath(out_dir, fido_packet_name);
    out_bundle_name = fido_packet_name;
    return true;
  }

  FidoAddress orig(net.fido.fido_address);
  for (int i = 0; i < 35; i++) {
    string bname = bundle_name(orig, route_to, dow, i);
    if (File::Exists(out_dir, bname)) {
      VLOG(1) << "Skipping candidate bundle: " << FilePath(out_dir, bname);
      // Already exists.
      continue;
    }
    File::set_current_directory(out_dir);
    const auto& arc = find_arc(arcs, ctype);
    // We have no parameter 2 since we're extracting everything.
    string zip_cmd = arc_stuff_in(arc.arca, FilePath(out_dir, bname), FilePath(temp_dir, fido_packet_name));
    // Execute the command
    LOG(INFO) << "Command: " << zip_cmd;
    if (0 != system(zip_cmd.c_str())) {
      LOG(ERROR) << "Failed executing: " << zip_cmd;
      return false;
    }
    // Need to be back home.
    File::set_current_directory(saved_dir);
    out_bundle_name = bname;

    LOG(INFO) << "Created bundle: " << FilePath(out_dir, bname);
    if (!File::Remove(temp_dir, fido_packet_name)) {
      LOG(ERROR) << "Error removing packet: " << FilePath(temp_dir, fido_packet_name);
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
  header.qm_orig_zone = from_address.zone();
  header.capabilities = 0x0001;
  header.capabilities_valid =
    ((header.capabilities_valid & 0x7f00) >> 8) | ((header.capabilities_valid & 0xff) << 8);
  header.product_code_high = 0;
  header.product_code_low = wwiv_net_version;
  // Add in packet password.
  to_char_array(header.password, packet_password);

  return header;
}

static bool create_ftn_packet(const Config& config, const FidoCallout& fido_callout, 
  const FidoAddress& dest, const FidoAddress& route_to, const net_networks_rec& net,
  const Packet& wwivnet_packet, string& fido_packet_name) {

  VLOG(1) << "create_ftn_packet: dest: " << dest << "; route: " << route_to;
  using wwiv::net::ReadPacketResponse;

  string temp_dir(net.fido.temp_outbound_dir);
  {
    string net_dir(File::MakeAbsolutePath(config.root_directory(), net.dir));
    File::MakeAbsolutePath(net_dir, &temp_dir);
  }

  FidoAddress from_address(net.fido.fido_address);
  for (int tries = 0; tries < 10; tries++) {
    time_t now = time(nullptr);
    File file(temp_dir, packet_name(now));
    if (!file.Open(File::modeCreateFile | File::modeExclusive | File::modeReadWrite | File::modeBinary, File::shareDenyReadWrite)) {
      LOG(INFO) << "Will try again: Unable to create packet file: " << file.full_pathname();
      wwiv::os::sleep_for(std::chrono::seconds(1));
      continue;
    }

    auto pw = fido_callout.packet_config_for(route_to).packet_password;
    packet_header_2p_t header = CreateType2PlusPacketHeader(from_address, route_to, now, pw);

    if (!write_fido_packet_header(file, header)) {
      LOG(ERROR) << "Error writing packet header.";
      return false;
    }

    bool is_email = (wwivnet_packet.nh.main_type == main_type_email || wwivnet_packet.nh.main_type == main_type_email_name);
    const string raw_text = wwivnet_packet.text;
    auto iter = raw_text.cbegin();

    string subtype;
    string title;
    string sender_name;
    string date_string;
    string to_user_name;
    // or we can put code in for email here??

    if (is_email) {
      to_user_name = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
      CleanupWWIVName(to_user_name);
    } else {
      subtype = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
    }
    title = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
    sender_name = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
    date_string = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
    if (!is_email) {
      to_user_name = get_fido_addr(raw_text, iter, {'\0', '\r', '\n'}, 80);
    }

    if (!is_email && iter_starts_with(raw_text, iter, "BY: ")) {
      // Skip BY line.
      get_message_field(raw_text, iter, {'\r', '\n'}, 80);
    }

    // Clean up sender name.
    CleanupWWIVName(sender_name);
    string bbs_text = WWIVToFidoText(string(iter, raw_text.end()));
    
    fido_variable_length_header_t vh{};
    vh.date_time = daten_to_fido(wwivnet_packet.nh.daten);
    vh.from_user_name = sender_name;
    vh.subject = title;
    if (!to_user_name.empty()) {
      vh.to_user_name = properize(to_user_name);
    } else {
      vh.to_user_name = "All";
    }

    string msgid;
    FtnMessageDupe dupe(config);
    if (!dupe.IsInitialized()) {
      LOG(ERROR) << "Unable to initialize FtnDupe";
      msgid = StrCat(to_zone_net_node(from_address), " DEADBEEF");
    } else {
      msgid = dupe.CreateMessageID(from_address);
    }

    // TODO(rushfan): need to add in INTL for netmails, and all that nonsense.
    // We probably have other stuff we need to add for echomail too.
    std::ostringstream text;
    if (is_email) {
      text << "\001" << "INTL " << dest << " " << from_address << "\r";
    } else {
      text << "AREA:" << subtype << "\r";
    }
    text << "\001PID: WWIV " << wwiv_version << beta_version << "\r"
      << "\001TID: WWIV NET" << wwiv_net_version << beta_version << "\r";
    if (!msgid.empty()) {
      text << "\001MSGID: " << msgid << "\r";
    }
    auto origin_line = net.fido.origin_line;
    if (origin_line.empty()) {
      // default origin line to system name if it doesn't exist.
      origin_line = config.config()->systemname;
    }

    text << bbs_text << "\r"
      << "--- WWIV " << wwiv_version << beta_version << "\r"
      << " * Origin: " << origin_line << " (" << to_zone_net_node(from_address) << ")\r";
    if (!is_email) {
      text << "SEEN-BY: " << to_net_node(from_address) << "\r\r";
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
  string fido_packet_name;
  if (!create_ftn_packet(net_cmdline.config(), fido_callout, dest, route_to, net, p, fido_packet_name)) {
    LOG(ERROR) << "Failed to create FTN packet.";
    write_wwivnet_packet(DEAD_NET, net, p);
    return false;
  }
  LOG(INFO) << "Created packet: " << FilePath(net.fido.temp_outbound_dir, fido_packet_name);

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
  string net_dir(File::MakeAbsolutePath(net_cmdline.config().root_directory(), net.dir));
  string out_dir(File::MakeAbsolutePath(net_dir, net.fido.outbound_dir));

  const string floname = flo_name(orig, dest, packet_config.netmail_status);
  const string bsyname = bundle_name(orig, dest, "bsy");

  for (int i = 1; i < 7; i++) {
    File bsy(out_dir, bsyname);
    if (!bsy.Open(File::modeCreateFile | File::modeExclusive | File::modeWriteOnly, File::shareDenyReadWrite)) {
      if (bsy.Exists()) {
        LOG(ERROR) << "BSY file: '" << bsy.full_pathname() << "' already exists. Will try again...";
      } else {
        LOG(ERROR) << "Unable to create BSY file: '" << bsy.full_pathname() << "'. Will try again...";
      }
      sleep_for(std::chrono::milliseconds((i ^ 2) * 50));
      continue;
    }
  }
  ScopeExit at_exit([=] { File::Remove(out_dir, bsyname); });
  TextFile flo_file(out_dir, floname, "a+");
  if (!flo_file.IsOpen()) {
    LOG(ERROR) << "Unable to open FLO file: " << flo_file.full_pathname();
    return false;
  }
  int num_written = flo_file.WriteLine(StrCat("^", FilePath(out_dir, bundlename)));
  return num_written > 0;
}

bool CreateNetmailAttach(const NetworkCommandLine& net_cmdline,const FidoAddress& dest, const net_networks_rec& net, const string& bundlename, const fido_packet_config_t& packet_config) {
  string net_dir(File::MakeAbsolutePath(net_cmdline.config().root_directory(), net.dir));
  string out_dir(File::MakeAbsolutePath(net_dir, net.fido.outbound_dir));
  string netmail_dir(File::MakeAbsolutePath(net_dir, net.fido.netmail_dir));
  
  string netmail_filepath = NextNetmailFilePath(netmail_dir);

  if (netmail_filepath.empty()) {
    LOG(ERROR) << "Unable to figure out netmail filename in dir: '" << net.fido.netmail_dir << "'";
    return false;
  }
  const string bundlepath = FilePath(out_dir, bundlename);
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
  string raw_text = p.text;
  auto it = p.text.cbegin();
  string subtype = get_message_field(p.text, it, {'\0', '\r', '\n'}, 80);
  LOG(INFO) << "Creating packet for subtype: " << subtype;

  auto net_dir = File::MakeAbsolutePath(net_cmdline.config().root_directory(), net.dir);
  std::set<FidoAddress> subscribers = ReadFidoSubcriberFile(net_dir, StrCat("n", subtype, ".net"));
  if (subscribers.empty()) {
    LOG(INFO) << "There are no subscribers on echo: '" << subtype << "'. Nothing to do!";
  }
  for (const auto& sub : subscribers) {
    string bundlename;
    auto packet_config = fido_callout.packet_config_for(sub);
    FidoAddress route_to = find_route_to(sub, fido_callout, packet_config);
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
  auto dest = get_address_from_line(to);
  if (dest.node() == 0) {
    LOG(ERROR) << "Unable to get address from to line: " << to;
    return false;
  }

  // todo - actually we need a new way of making hte ftn packet that works right
  // with netmaol
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
      const std::vector<string> extensions{"su?", "mo?", "tu?", "we?", "th?", "fr?", "sa?", "pkt"};
      auto net_dir = File::MakeAbsolutePath(net_cmdline.config().root_directory(), net.dir);
      auto inbounddir = File::MakeAbsolutePath(net_dir, net.fido.inbound_dir);
      for (const auto& ext : extensions) {
        import_bundles(net_cmdline.config(), fido_callout, net, inbounddir, StrCat("*.", ext));
#ifndef _WIN32
        string uext = ext;
        StringUpperCase(&uext);
        import_bundles(net_cmdline.config(), fido_callout, net, tempdir, StrCat("*.", uext));
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
        wwiv::net::ReadPacketResponse response = read_packet(f, p);
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
      return 1;
    }
    return 0;
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [networkf]: " << e.what();
  }
  return 2;
}
