/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2022, WWIV Software Services             */
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
#include "networkf/networkf.h"

#include "core/command_line.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/log.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/semaphore_file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/version.h"
#include "fmt/format.h"
#include "net_core/net_cmdline.h"
#include "sdk/bbslist.h"
#include "sdk/config.h"
#include "sdk/fido/fido_address.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/fido/fido_directories.h"
#include "sdk/fido/fido_packets.h"
#include "sdk/fido/fido_util.h"
#include "sdk/filenames.h"
#include "sdk/files/arc.h"
#include "sdk/net/ftn_msgdupe.h"
#include "sdk/net/packets.h"
#include "sdk/net/subscribers.h"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::os;
using namespace wwiv::sdk::fido;
using namespace wwiv::sdk::net;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::net::networkf {


static std::string arc_stuff_in(const std::string& command_line, const std::string& a1,
                                const std::string& a2) {
  std::ostringstream os;
  for (auto it = command_line.begin(); it != command_line.end(); ++it) {
    if (*it == '%') {
      ++it;
      switch (*it) {
      case '%':
        os << "%";
        break;
      case '1':
        os << a1;
        break;
      case '2':
        os << a2;
        break;
      }
    } else {
      os << *it;
    }
  }
  return os.str();
}

void ShowNetworkfHelp(const NetworkCommandLine& cmdline) {
  std::cout << cmdline.GetHelp() << std::endl
       << "commands: " << std::endl
       << std::endl
       << " import    Import messages from FTN Packet to WWIV (P*.net_)" << std::endl
       << " export    Export messages from WWIV (p*.net_) to FTN packet" << std::endl
       << std::endl;

  exit(1);
}

static std::string get_echomail_areaname(const std::string& text) {
  auto lines = split_message(text);
  for (const auto& line : lines) {
    if (starts_with(line, "AREA:")) {
      return line.substr(5);
    }
  }
  return "";
}

NetworkF::NetworkF(const sdk::BbsDirectories& bbsdirs, const networkf_options_t& opts,
                   const sdk::net::Network net, const sdk::BbsListNet& bbslist, core::Clock& clock)
    : bbslist_(bbslist), clock_(clock), net_(net),
      fido_callout_(bbsdirs.root_directory(), opts.max_backups, net_),
      netdat_(bbsdirs.gfilesdir(), bbsdirs.logdir(), net_, opts.net_cmd, clock_),
      dirs_(bbsdirs.root_directory(), net_), opts_(opts), datadir_(bbsdirs.datadir()) {
  const IniFile ini(FilePath(bbsdirs.root_directory(), WWIV_INI), "WWIV");
  if (ini.IsOpen()) {
    // pull out new user colors
    for (auto i = 0; i < 10; i++) {
      const auto num = ini.value<int>(fmt::format("NUCOLOR[{}]", i));
      if (num != 0) {
        colors_[i] = num;
      }
    }
  }
}

NetworkF::~NetworkF() = default;

int ftn_date_days_old(const Clock& clock, const std::string& ftn_date) {
  const auto daten = fido_to_daten(ftn_date);
  const auto pkt_dt = DateTime::from_daten(daten).to_system_clock();
  const auto now = clock.Now().to_system_clock();
  // TODO(rushfan): C++20 is needed for std::chrono::days
  const auto diff_hours = std::chrono::duration_cast<std::chrono::hours>(now - pkt_dt).count();
  const auto days = diff_hours / 24;
  return days;
}

bool NetworkF::import_packet_file(const std::filesystem::path& path) {
  LOG(INFO) << "Importing Packet: " << path.string();
  auto o = FidoPacket::Open(path);
  if (!o) {
    LOG(INFO) << "Unable to open file: " << path.string();
    return false;
  }
  auto& packet = o.value();

  FidoAddress address(packet.header().orig_zone, packet.header().orig_net,
                      packet.header().orig_node, packet.header().orig_point, "");
  const auto expected = ToStringUpperCase(fido_callout_.packet_config_for(address).packet_password);
  const auto actual = packet.password();
  if (!iequals(expected, actual)) {
    LOG(ERROR) << "Unexpected packet password from node: " << address << "; actual: '" << actual
               << "'; expected: '" << expected << "'";
    // Move to BADMSGS
    packet.Close();
    const auto bad_messages_paath = FilePath(dirs_.bad_packets_dir(), path.filename().string());

    if (!File::Move(path, bad_messages_paath)) {
      LOG(ERROR) << "Error moving file to BADMSGS; file: " << path.string();
    }
    return false;
  }

  while (true) {
    auto [response, msg] = packet.Read();
    if (response != ReadNetPacketResponse::OK) {
      return true;
    }

    const auto is_email = (msg.nh.attribute & MSGPRIVATE) != 0;
    if (!is_email) {
      // Don't check for dupes in emails since we certainly won't have a MSGID and also
      // likely the header may match for automated responses split over multiple messages (#1395)
      if (dupe().is_dupe(msg)) {
        const auto msgid = FtnMessageDupe::GetMessageIDFromText(msg.vh.text);
        LOG(ERROR) << "Skipping duplicate FTN message: '" << msg.vh.subject << "' msgid: (" << msgid
                   << ")";
        LOG(ERROR) << "Text: " << msg.vh.text;
        // TODO(rushfan): move this or write out saved copy?
        continue;
      }
      dupe().add(msg);
    }

    const auto max_days = net().fido.max_echomail_age_days;
    if (!is_email && max_days > 0) {
      // Only check if max_days > 0, otherwise 0 means unlimited.
      const auto days_old = ftn_date_days_old(clock_, msg.vh.date_time);
      if (days_old > max_days) {
        // Packet is too old, skip it.
        const auto msgid = FtnMessageDupe::GetMessageIDFromText(msg.vh.text);
        const auto logmsg = fmt::format("Too old FTN message ({} days): msgid:{}; '{}'", days_old,
                                        msgid, msg.vh.subject);
        LOG(ERROR) << logmsg;
        LOG(ERROR) << "Text: " << msg.vh.text;
        // TODO(rushfan): move this or write out saved copy?
        continue;
      }
    }

    const auto ftn_packet_daten = fido_to_daten(msg.vh.date_time);
    net_header_rec nh{};
    nh.daten = static_cast<uint32_t>(ftn_packet_daten);
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

    auto from_address = get_address_from_packet(msg, packet.header());

    std::string s1;
    // Email Format: TO_USER<nul>TITLE<nul>SENDER_NAME<cr/lf>DATE_STRING<cr/lf>MESSAGE_TEXT.
    // Non-Email Sub Format: SUBTYPE<nul>TITLE<nul>SENDER_NAME<cr/lf>DATE_STRING<cr/lf>MESSAGE_TEXT.
    std::string text = (is_email) ? msg.vh.to_user_name : get_echomail_areaname(msg.vh.text);
    // Adding a 0 is adding a NULL character to the string
    text.push_back(0);
    text.append(msg.vh.subject);
    text.push_back(0);
    text.append(StrCat(msg.vh.from_user_name, " (", from_address, ")\r\n"));
    text.append(daten_to_wwivnet_time(ftn_packet_daten));
    text.append("\r\n");

    if (!is_email) {
      // Add ^D0FidoAddr for the "To:" name of the post.
      static const char kFidoAddr[] = "\x04"
                                      "0FidoAddr: ";
      const auto to_name = msg.vh.to_user_name.empty() ? "All" : msg.vh.to_user_name;
      // If for some screwy reason we don't have a to name, address it to 'All'.
      text.append(kFidoAddr).append(to_name).append("\r\n");
    }
    text.append(FidoToWWIVText(msg.vh.text));

    nh.length = size_uint32(text);
    // Create file, write to local.net_ for network2 to import.
    NetPacket wwiv_packet(nh, {}, text);
    if (!write_wwivnet_packet(FilePath(net_.dir, LOCAL_NET), wwiv_packet)) {
      LOG(ERROR) << "ERROR Writing WWIV packet for message: " << wwiv_packet.nh.main_type << "/"
                 << wwiv_packet.nh.minor_type;
    } else {
      std::string itype = is_email ? "Email" : "Post";
      LOG(INFO) << fmt::format("Imported FTN {} '{}' to '{}'", itype, msg.vh.subject, s1);
    }
  }

  return true;
}

bool NetworkF::import_packets(const std::filesystem::path& dir, const std::string& mask) {
  VLOG(1) << "Importing packets from: " << dir << "' mask: '" << mask << "'";
  FindFiles files(FilePath(dir, mask), FindFiles::FindFilesType::files);
  if (files.empty()) {
    LOG(INFO) << "No packets to import in: '" << dir << "' mask: '" << mask << "'";
  }
  for (const auto& f : files) {
    if (import_packet_file(FilePath(dir, f.name))) {
      LOG(INFO) << "Successfully imported packet: " << FilePath(dir, f.name);
      if (opts_.skip_delete) {
        backup_file(FilePath(net_.dir, f.name));
      }
      File::Remove(FilePath(dir, f.name));
    }
  }
  return true;
}

bool NetworkF::import_bundle_file(const std::filesystem::path& path) {
  VLOG(1) << "import_bundle_file: path: " << path.string();

  {
    // Check to make sure the file is readable.
    File f(path);
    if (!f.Open(File::modeBinary | File::modeReadOnly)) {
      LOG(INFO) << "Unable to open file: " << path.string();
      return false;
    }
  }

  const auto saved_dir = File::current_directory();
  auto at_exit = finally([=] { File::set_current_directory(saved_dir); });
  File::set_current_directory(dirs_.temp_inbound_dir());

  // were in the temp dir now.
  const auto arcs = files::read_arcs(datadir_);
  if (arcs.empty()) {
    LOG(ERROR) << "No archivers defined!";
    return false;
  }

  const auto arc = files::find_arcrec(arcs, path, "ZIP");
  if (!arc) {
    LOG(ERROR) << "Unable to find archiver for file: " << path;
    return false;
  }
  // We have no parameter 2 since we're extracting everything.
  const auto unzip_cmd = arc_stuff_in(arc.value().arce, path.string(), "");
  // Execute the command
  LOG(INFO) << "Command: " << unzip_cmd;
  if (system(unzip_cmd.c_str()) != 0) {
    LOG(ERROR) << "Failed executing: " << unzip_cmd;
    return false;
  }
  // Need to be back home.
  File::set_current_directory(saved_dir);

  import_packets(dirs_.temp_inbound_dir(), "*.pkt");
  return true;
}

/**
 * Imports FTN Bundles (files like XXXXXXXXX.SU0)
 *
 * Returns the # of bundles processed.
 */
int NetworkF::import_bundles(const std::filesystem::path& dir, const std::string& mask) {
  auto num_bundles_processed = 0;

  VLOG(3) << "import_bundles: mask: " << mask;
  FindFiles files(FilePath(dir, mask), FindFiles::FindFilesType::files);
  for (const auto& f : files) {
    if (f.size == 0) {
      // skip zero byte files.
      LOG(INFO) << "Skipping zero byte bundle or packet: " << f.name;
      continue;
    }
    const auto lname = ToStringLowerCase(f.name);
    const auto path = FilePath(dir, f.name);
    LOG(INFO) << "Attempting to import packet: " << path;
    if (ends_with(lname, ".pkt")) {
      if (import_packet_file(path)) {
        LOG(INFO) << "Successfully imported packet: " << path;
        ++num_bundles_processed;
        if (opts_.skip_delete) {
          backup_file(path);
        }
        File::Remove(path);
      }
    } else if (import_bundle_file(path)) {
      LOG(INFO) << "Successfully imported bundle: " << path;
      ++num_bundles_processed;
      if (opts_.skip_delete) {
        backup_file(path);
      }
      File::Remove(path);
    }
  }
  return num_bundles_processed;
}

static std::string rename_fido_packet(const std::filesystem::path& dir, const std::string& origname) {
  if (!ends_with(origname, ".pkt") || origname.size() != 12) {
    LOG(ERROR) << "rename_fido_packet: not allowed on name: '" << origname << "'";
    return origname;
  }
  auto base = origname.substr(0, 8);
  for (auto i = 0; i < 26; i++) {
    base.pop_back();
    base.push_back(static_cast<char>('a' + i));
    auto newname = base + ".pkt";
    if (!File::Exists(FilePath(dir, newname))) {
      return newname;
    }
  }
  return origname;
}

std::optional<std::string> NetworkF::create_ftn_bundle(const FidoAddress& route_to,
                                                       const std::string& fido_packet_name) {
  // were in the temp dir now.
  auto arcs = files::read_arcs(datadir_);
  if (arcs.empty()) {
    LOG(ERROR) << "No archivers defined!";
    return std::nullopt;
  }
  const auto now = clock_.Now();
  const auto dow = now.dow();

  const auto saved_dir = File::current_directory();
  auto at_exit = finally([=] { File::set_current_directory(saved_dir); });

  const auto ctype = fido_callout_.packet_config_for(route_to).compression_type;

  if (ctype == "PKT") {
    // No bundles, only packet files.
    const auto in = FilePath(dirs_.temp_outbound_dir(), fido_packet_name);
    auto out = FilePath(dirs_.outbound_dir(), fido_packet_name);
    if (File::Exists(out)) {
      LOG(INFO) << "Outbound dir already has a packet named: " << fido_packet_name;
      const auto newname = rename_fido_packet(dirs_.outbound_dir(), fido_packet_name);
      LOG(INFO) << "Renamed to: " << newname;
      out = FilePath(dirs_.outbound_dir(), newname);
    }
    if (!File::Move(in, out)) {
      LOG(ERROR) << "Unable to move packet file into outbound dir. file: " << fido_packet_name;
      return std::nullopt;
    }
    LOG(INFO) << "Created bundle(packet): " << FilePath(dirs_.outbound_dir(), fido_packet_name);
    return fido_packet_name;
  }

  FidoAddress orig(net_.fido.fido_address);
  for (auto i = 0; i < 35; i++) {
    const auto bname = bundle_name(orig, route_to, dow, i);
    const auto full_bundle_path = FilePath(dirs_.outbound_dir(), bname);
    if (File::Exists(full_bundle_path)) {
      // Already exists.
      VLOG(1) << "Skipping candidate bundle: " << full_bundle_path.string();
      continue;
    }
    // We should actually change to the temp outbound dir so that we won't add paths.
    File::set_current_directory(dirs_.temp_outbound_dir());
    LOG(INFO) << "Changed directory to: " << dirs_.temp_outbound_dir();
    // Use ZIP by default if we can't find anything that matches, then we'll hope for the best.
    const auto arc = files::find_arcrec(arcs, ctype, "ZIP"); 
    if (!arc) {
      LOG(WARNING) << "Skipping candidate bundle. Unable to find archiver for type: '" << ctype
                   << "'";
      continue;
    }
    const auto zip_cmd = arc_stuff_in(arc->arca, full_bundle_path.string(), fido_packet_name);
    LOG(INFO) << "Command: " << zip_cmd;
    if (0 != system(zip_cmd.c_str())) {
      LOG(ERROR) << "Failed executing: " << zip_cmd;
      return std::nullopt;
    }
    // Need to be back home.
    LOG(INFO) << "Changed directory back to: " << saved_dir;
    File::set_current_directory(saved_dir);

    LOG(INFO) << "Created bundle: " << full_bundle_path.string();
    if (!File::Remove(FilePath(dirs_.temp_outbound_dir(), fido_packet_name))) {
      LOG(ERROR) << "Error removing packet: "
                 << FilePath(dirs_.temp_outbound_dir(), fido_packet_name);
    }
    return bname;
  }
  return std::nullopt;
}

static bool CleanupWWIVName(std::string& sender_name) {
  // #NN, @NODE or (FIDO_ADDR)
  const auto idx = sender_name.find_first_of("#@(");
  if (idx != std::string::npos) {
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
  static const std::string kFidoAddr = "\x04"
                                       "0FidoAddr: ";
  std::string address;
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
static bool iter_starts_with(const C& c, I& iter, const std::string& expected) {
  for (const auto& ch : expected) {
    if (iter == c.end()) {
      return false;
    }
    if (*iter != ch) {
      return false;
    }
    ++iter;
  }
  return true;
}


static std::string remove_fido_addr(std::string to_user) {
  std::string to_user_new;

  for (auto i = 0; i < ssize(to_user); i++) {
    if (to_user[i] == '(' || (to_user[i] == ' ' && to_user[i + 1] == '#')) {
      break;
    }
    to_user_new += to_user[i];
  }
  return to_user_new;
}

std::optional<std::string> NetworkF::create_ftn_packet(const FidoAddress& dest,
                                                       const FidoAddress& route_to,
                                                       const NetPacket& wwivnet_packet) {

  VLOG(1) << "create_ftn_packet: dest: " << dest << "; route: " << route_to;

  FidoAddress from_address(net_.fido.fido_address);
  std::string pw = fido_callout_.packet_config_for(route_to).packet_password;
  for (auto tries = 0; tries < 10; tries++) {
    auto now = clock_.Now().now();
    File file(FilePath(dirs_.temp_outbound_dir(), packet_name(now)));
    if (!file.Open(File::modeCreateFile | File::modeExclusive | File::modeReadWrite |
                       File::modeBinary,
                   File::shareDenyReadWrite)) {
      LOG(INFO) << "Will try again: Unable to create packet file: " << file;
      clock_.SleepFor(std::chrono::seconds(1));
      continue;
    }

    auto header = CreateType2PlusPacketHeader(from_address, route_to, now, pw);

    if (!write_fido_packet_header(file, header)) {
      LOG(ERROR) << "Error writing packet header.";
      return std::nullopt;
    }

    auto is_email = wwivnet_packet.nh.main_type == main_type_email ||
                    wwivnet_packet.nh.main_type == main_type_email_name;
    const auto raw_text = wwivnet_packet.text();
    auto iter = raw_text.cbegin();

    std::string subtype;
    std::string to_user_name;
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

    fido_variable_length_header_t vh{};
    vh.date_time = daten_to_fido(wwivnet_packet.nh.daten);
    // Clean up sender name.
    CleanupWWIVName(sender_name);
    vh.from_user_name = sender_name;
    vh.subject = title;
    if (!to_user_name.empty()) {
      const auto username_only = remove_fido_addr(to_user_name);
      vh.to_user_name = properize(username_only);
    } else {
      vh.to_user_name = "All";
    }

    FtnMessageDupe dupe(datadir_, true);
    auto msgid = FtnMessageDupe::GetMessageIDFromWWIVText(raw_text);
    auto needs_msgid = false;
    if (msgid.empty()) {
      // Create a new MSGID if the BBS didn't put one in there already.
      // We'll do this for emails too since Mystic needs this for a proper
      // reply to address. Otherwise we'd just do it for conference mail.
      msgid = dupe.CreateMessageID(from_address);
      needs_msgid = true;
    }

    // TODO(rushfan): need to add in INTL for netmails, and all that nonsense.
    // We probably have other stuff we need to add for echomail too.
    std::ostringstream text;
    if (is_email) {
      text << "\001"
           << "INTL " << dest.as_string(false, false) << " "
           << from_address.as_string(false, false) << "\r";
      if (from_address.point()) {
        // FMPT (FROM POINT) just has the point address
        text << "\001" << "FMPT " << from_address.point() << "\r";
      }
      if (dest.point()) {
        // TOPT (TO POINT) just has the point address
        text << "\001" << "TOPT " << dest.point() << "\r";
      }
    } else {
      text << "AREA:" << subtype << "\r";
    }
    // As of 5.3, the PID is added by the BBS software.
    // text << "\001PID: WWIV " << full_version() << "\r";
    text << "\001TID: WWIV NET" << full_version() << "\r";
    if (needs_msgid && !is_email) {
      text << "\001MSGID: " << msgid << "\r";
    }
    // Implement FTS-5003. [http://ftsc.org/docs/fts-5003.001]
    // All outbound WWIV messages are always CP437.
    text << "\001CHRS: CP437 2\r";

    // Implement FRL-1004. [http://ftsc.org/docs/frl-1004.002]
    text << "\001TZUTC: " << tz_offset_from_utc(clock_.Now()) << "\r";

    // TODO(rushfan): We should rip through the bbs_text here.
    // and add in any special kludges like ^AREPLY here.
    // Add the text from the message (as entered from the BBS).
    wwiv_to_fido_options opts{};
    opts.colors = colors_;
    opts.wwiv_heart_color_codes = net_.fido.wwiv_heart_color_codes;
    opts.wwiv_pipe_color_codes = net_.fido.wwiv_pipe_color_codes;
    opts.allow_any_pipe_codes = net_.fido.allow_any_pipe_codes;
    auto bbs_text = WWIVToFidoText(std::string(iter, raw_text.end()), opts);
    text << bbs_text;

    // Now we need tear + origin lines
    auto origin_line = net_.fido.origin_line;
    if (origin_line.empty()) {
      // default origin line to system name if it doesn't exist.
      origin_line = opts_.system_name;
    }

    if (from_address.point() == 0) {
      text << "\r"
           << "--- WWIV " << full_version() << "[" << os_version_string() << "]\r"
           << " * Origin: " << origin_line << " (" << to_zone_net_node(from_address) << ")\r";
    } else {
      text << "\r"
           << "--- WWIV " << full_version() << "\r"
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
      return std::nullopt;
    }

    // Since we wrote the packed message, let's add it to the
    // duplicate message database if it's a post.
    if (!is_email) {
      dupe.add(p);
    }
    return file.path().filename().string();
  }
  return std::nullopt;
}

std::optional<std::string> NetworkF::create_ftn_packet_and_bundle(const FidoAddress& dest,
                                                                  const FidoAddress& route_to,
                                                                  NetPacket& p) {
  LOG(INFO) << "Creating packet for subscriber: " << dest << "; route_to: " << route_to;
  // TODO(rushfan): Really this shouldn't go to dead.net since the wwivnet side exported it
  // already, not really sure what to do here.
  auto fido_packet_name = create_ftn_packet(dest, route_to, p);
  if (!fido_packet_name) {
    LOG(ERROR) << "    ! ERROR Failed to create FTN packet; writing to dead.net";
    write_deadnet_packet(net_.dir, p);
    return std::nullopt;
  }
  LOG(INFO) << "Created packet: " << FilePath(dirs_.temp_outbound_dir(), fido_packet_name.value());

  const auto bundlename = create_ftn_bundle(route_to, fido_packet_name.value());
  if (!bundlename) {
    LOG(ERROR) << "    ! ERROR Failed to create FTN bundle; writing to dead.net";
    write_deadnet_packet(net_.dir, p);
    return std::nullopt;
  }
  return bundlename;
}

static std::string NextNetmailFilePath(const std::filesystem::path& path) {
  for (int i = 2; i < 10000; i++) {
    auto candidate = FilePath(path, StrCat(i, ".msg"));
    if (!File::Exists(candidate)) {
      return candidate.string();
    }
  }
  return "";
}

static bool CreateFidoNetAttachNetMail(const FidoAddress& orig, const FidoAddress& dest,
                                       const std::string& netmail_filename,
                                       const std::string& bundle_path,
                                       const fido_packet_config_t& packet_config) {
  File netmail(netmail_filename);
  if (!netmail.Open(File::modeBinary | File::modeCreateFile | File::modeExclusive |
                        File::modeReadWrite,
                    File::shareDenyReadWrite)) {
    LOG(ERROR) << "Unable to open netmail filen: '" << netmail << "'";
    return false;
  }

  // FTC-0053 flags
  std::string flags = "FLAGS FIL TFS PVT";
  fido_stored_message_t h{};
  h.attribute = (MSGFILE | MSGKILL | MSGLOCAL);
  const auto& st = packet_config.netmail_status;
  switch (st) {
  case fido_bundle_status_t::hold:
    h.attribute |= MSGHOLD;
    flags += " HLD";
    break;
  case fido_bundle_status_t::immediate:
    h.attribute |= MSGHOLD;
    flags += " IMM";
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

  std::string from = StrCat("WWIV NET ", full_version());
  std::string to = "SYSOP";

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

  std::string msgid;
  std::ostringstream text;

  // FTC-0053 flags
  text << "\001" << flags << "\r";
  // FTS-4001 INTL line
  text << "\001"
       << "INTL " << dest << " " << orig << "\r";
  text << "\001PID: WWIV " << full_version() << "\r"
       << "\001TID: WWIV NET" << full_version() << "\r";
  if (!msgid.empty()) {
    text << "\001MSGID: " << msgid << "\r";
  }

  FidoStoredMessage m(h, text.str());
  write_stored_message(netmail, m);

  return true;
}

std::optional<std::string> NetworkF::CreateFloFile(const wwiv::sdk::fido::FidoAddress& dest,
                                                   const std::string& bundlename,
                                                   const fido_packet_config_t& packet_config) {
  FidoAddress orig(net_.fido.fido_address);
  const auto floname = flo_name(dest, packet_config.netmail_status);
  const auto bsyname = net_node_name(dest, "bsy");

  try {
    auto sem_file = SemaphoreFile::try_acquire(bsyname, std::chrono::seconds(15));

    TextFile flo_file(FilePath(dirs_.outbound_dir(), floname), "a+");
    if (!flo_file.IsOpen()) {
      LOG(ERROR) << "Unable to open FLO file: " << flo_file;
      return std::nullopt;
    }
    const auto num_written =
        flo_file.WriteLine(StrCat("^", FilePath(dirs_.outbound_dir(), bundlename).string()));
    return num_written == 0 ? std::nullopt : std::make_optional(flo_file.full_pathname());

  } catch (const semaphore_not_acquired& e) {
    LOG(ERROR) << "Unable to create BSY file semaphore trying to create FLO file.";
    LOG(ERROR) << e.what();
  }
  return std::nullopt;
}

std::optional<std::filesystem::path>
NetworkF::CreateNetmailAttach(const FidoAddress& dest, const std::string& bundlename,
                              const fido_packet_config_t& packet_config) {
  const auto netmail_filepath = NextNetmailFilePath(dirs_.netmail_dir());

  if (netmail_filepath.empty()) {
    LOG(ERROR) << "Unable to figure out netmail filename in dir: '" << dirs_.netmail_dir() << "'";
    return std::nullopt;
  }
  const auto bundlepath = FilePath(dirs_.outbound_dir(), bundlename);
  if (!CreateFidoNetAttachNetMail(FidoAddress(net_.fido.fido_address), dest, netmail_filepath,
                                  bundlepath.string(), packet_config)) {
    LOG(ERROR) << "Unable to create netmail: " << netmail_filepath;
    return std::nullopt;
  }
  LOG(INFO) << "Wrote attach netmail: " << netmail_filepath;
  return netmail_filepath;
}

std::optional<std::filesystem::path>
NetworkF::CreateNetmailAttachOrFloFile(const FidoAddress& dest, const std::string& bundlename,
                                       const fido_packet_config_t& packet_config) {
  if (net_.fido.mailer_type == fido_mailer_t::attach) {
    return CreateNetmailAttach(dest, bundlename, packet_config);
  }
  if (net_.fido.mailer_type == fido_mailer_t::flo) {
    return CreateFloFile(dest, bundlename, packet_config);
  }
  LOG(ERROR) << "Unknown mailer type: " << static_cast<int>(net_.fido.mailer_type);
  return std::nullopt;
}

static FidoAddress find_route_to(const FidoAddress& dest, const FidoCallout& callout,
                                 const fido_packet_config_t& packet_config) {
  if (packet_config.netmail_status == fido_bundle_status_t::direct) {
    return dest;
  }

  if (const auto a = FindRouteToAddress(dest, callout)) {
    return a.value();
  }
  // is this right? returning direct if we have no route?
  return dest;
}

bool NetworkF::export_main_type_new_post(std::set<std::string>& bundles, NetPacket& p) {
  // Lame implementation that creates 1 file per message.
  auto subtype = get_subtype_from_packet_text(p.text());
  LOG(INFO) << "Creating packet for subtype: " << subtype;

  auto subscribers = ReadFidoSubcriberFile(FilePath(net_.dir, StrCat("n", subtype, ".net")));
  if (subscribers.empty()) {
    LOG(INFO) << "There are no subscribers on echo: '" << subtype << "'. Nothing to do!";
  }
  for (const auto& sub : subscribers) {
    const auto packet_config = fido_callout_.packet_config_for(sub);
    const auto route_to = find_route_to(sub, fido_callout_, packet_config);
    if (auto bundlename = create_ftn_packet_and_bundle(sub, route_to, p)) {
      if (!contains(bundles, bundlename.value())) {
        // We only want to attach the bundle (or add it to the flo file)
        // one time, so skip ones that have already been done.
        bundles.insert(bundlename.value());
        auto route_packet_config = fido_callout_.packet_config_for(route_to);
        CreateNetmailAttachOrFloFile(route_to, bundlename.value(), route_packet_config);
      }
    }
  }
  return true;
}

bool NetworkF::export_main_type_email_name(std::set<std::string>& bundles, NetPacket& p) {
  // Lame implementation that creates 1 file per message.
  LOG(INFO) << "Creating packet for netmail.";

  auto it = std::begin(p.text());
  const auto to = get_message_field(p.text(), it, {0}, 80);
  const auto odest = get_address_from_single_line(to);
  if (!odest.has_value()) {
    LOG(ERROR) << "Unable to get address from to line: " << to;
    return false;
  }
  const auto& dest = odest.value();

  // todo - actually we need a new way of making the ftn packet that works
  // right with net mail
  const auto packet_config = fido_callout_.packet_config_for(dest);
  const FidoAddress route_to = find_route_to(dest, fido_callout_, packet_config);
  if (auto bundlename = create_ftn_packet_and_bundle(dest, route_to, p)) {
    if (!contains(bundles, bundlename.value())) {
      // We only want to attach the bundle (or add it to the flo file)
      // one time, so skip ones that have already been done.
      bundles.insert(bundlename.value());
      CreateNetmailAttachOrFloFile(route_to, bundlename.value(), packet_config);
    }
  }
  return true;
}

sdk::FtnMessageDupe& NetworkF::dupe() {
  if (!dupe_) {
    dupe_ = std::make_unique<wwiv::sdk::FtnMessageDupe>(datadir_, true);
  }
  return *dupe_;
}

bool NetworkF::DoImport() {
  auto num_packets_processed = 0;
  std::unique_ptr<FtnMessageDupe> dupe;
  const std::vector<std::string> extensions{"su?", "mo?", "tu?", "we?", "th?", "fr?", "sa?", "pkt"};
  for (const auto& ext : extensions) {
    num_packets_processed += import_bundles(dirs_.inbound_dir(), StrCat("*.", ext));
#ifndef _WIN32
    num_packets_processed +=
        import_bundles(dirs_.inbound_dir(), StrCat("*.", ToStringUpperCase(ext)));
#endif
  }
  return num_packets_processed > 0;
}

bool NetworkF::DoExport() {
  const auto sfile_name = StrCat("s", FTN_FAKE_OUTBOUND_NODE, ".net");
  const auto path = FilePath(net_.dir, sfile_name);
  if (!File::Exists(path)) {
    LOG(INFO) << "No file '" << path.string() << "' exists to be exported to a FTN packet.";
    return false;
  }

  NetMailFile file(path, true);
  if (!file) {
    LOG(ERROR) << "Unable to open file: " << path.string();
    return false;
  }

  std::set<std::string> bundles;
  auto num_packets_processed = 0;
  for (auto p : file) {
    // If we got here, we had a packet to process.
    ++num_packets_processed;

    if (p.nh.main_type == main_type_new_post) {
      if (!export_main_type_new_post(bundles, p)) {
        LOG(ERROR) << "Error exporting post.";
      }
    } else if (p.nh.main_type == main_type_email_name) {
      if (!export_main_type_email_name(bundles, p)) {
        LOG(ERROR) << "Error exporting email.";
      }
    } else {
      LOG(ERROR) << "    ! ERROR Unhandled type: '" << main_type_name(p.nh.main_type)
                 << "'; writing to dead.net";
      // Let's write it to dead.net_
      if (!write_deadnet_packet(net_.dir, p)) {
        LOG(ERROR) << "Error writing to dead.net";
      }
    }
  }

  // Delete the packet.
  file.Close();
  if (opts_.skip_delete) {
    backup_file(path);
  }
  File::Remove(path);

  return file.last_read_response() != ReadNetPacketResponse::ERROR && num_packets_processed > 0;
}

bool NetworkF::Run(std::vector<std::string> cmds) {
  if (!fido_callout_.IsInitialized()) {
    LOG(ERROR) << "Unable to initialize fido_callout.";
    return false;
  }

  if (cmds.empty()) {
    LOG(ERROR) << "No command specified. Exiting.";
    return false;
  }

  const auto cmd = cmds.front();

  if (cmd == "import") {
    return DoImport();

  } else if (cmd == "export") {
    return DoExport();

  } else {
    LOG(ERROR) << "Unknown command: " << cmd;
    return false;
  }
}

} // namespace wwiv::net::networkf
