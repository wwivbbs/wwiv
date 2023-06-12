/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*            Copyright (C)2016-2022, WWIV Software Services              */
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
#include "sdk/net/packets.h"

#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "fmt/format.h"
#include "sdk/filenames.h"
#include "sdk/subxtr.h"
#include "sdk/net/subscribers.h"
#include <string>
#include <utility>

using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::sdk::net {

bool send_local_email(const Network& network, net_header_rec& nh, const std::string& text,
                      const std::string& byname, const std::string& title) {
  return send_network_email(LOCAL_NET, network, nh, {}, text, byname, title);
}

bool send_network_email(const std::string& filename, const Network& network, net_header_rec& nh,
                        const std::vector<uint16_t>& list, const std::string& text,
                        const std::string& byname, const std::string& title) {

  LOG(INFO) << "send_network_email: Writing type " << nh.main_type << "/" << nh.minor_type
            << " message to NetPacket: " << filename << "; title: " << title;

  ParsedNetPacketText ppt{nh.main_type};
  ppt.set_date(nh.daten);
  ppt.set_title(title);
  ppt.set_sender(byname);
  ppt.set_text(text);

  return write_wwivnet_packet(FilePath(network.dir, filename), NetPacket(nh, list, ppt));
}

bool delete_packet(File& f, NetPacket& packet) {
  if (packet.source() == NetPacketSource::DISK) {
    f.Seek(packet.offset(), File::Whence::begin);
    packet.nh.main_type = 0xFFFF; // 65535
    f.Write(&packet.nh, sizeof(net_header_rec));
    // restore current position.
    f.Seek(packet.end_offset(), File::Whence::begin);
    return true;
  }
  return false;
}

std::tuple<NetPacket, ReadNetPacketResponse> read_packet(File& f, bool process_de) {
  NetPacket packet{};
  // Since this packet is read from disk, mark it as such, and note the current position
  // as the packet offset.
  packet.set_source(NetPacketSource::DISK);
  packet.set_offset(f.current_position());
  auto num_read = f.Read(&packet.nh, sizeof(net_header_rec));
  if (num_read == 0) {
    // at the end of the NetPacket.
    return std::make_tuple(packet, ReadNetPacketResponse::END_OF_FILE);
  }

  if (num_read != sizeof(net_header_rec)) {
    LOG(INFO) << "error reading header, got short read of size: " << num_read
              << "; expected: " << sizeof(net_header_rec);
    return std::make_tuple(packet, ReadNetPacketResponse::ERROR);
  }

  if (packet.nh.method > 0) {
    LOG(INFO) << "compression: de" << packet.nh.method;
  }

  if (packet.nh.list_len > 0) {
    // skip list of addresses.
    packet.list.resize(packet.nh.list_len);
    f.Read(&packet.list[0], sizeof(uint16_t) * packet.nh.list_len);
  }

  if (packet.nh.length != 0) {
    const auto length = packet.nh.length;

    if (length > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
      LOG(INFO) << "error reading header, got length too big (underflow?): " << length;
      return std::make_tuple(packet, ReadNetPacketResponse::ERROR);
    }

    if (packet.nh.method == 1  && process_de &&
        packet.nh.length > 146 /* Make sure we have enough for a header */) {
      // HACK - this should do this in a shim DE 146 is the sizeof EN/DE header for de1.
      packet.nh.length -= 146;
      char header[147];
      f.Read(header, 146);
      LOG(INFO) << header;
    }
    std::string read_text;
    read_text.resize(length);
    num_read = f.Read(&read_text[0], packet.nh.length);
    read_text.resize(num_read);
    packet.set_text(std::move(read_text));
  }
  packet.set_end_offset(f.current_position());
  return std::make_tuple(packet, ReadNetPacketResponse::OK);
}

bool write_deadnet_packet(const std::filesystem::path& dir, NetPacket& packet) {
  const auto path = dir / DEAD_NET;

  packet.nh.list_len = 0;
  packet.list.clear();

  return write_wwivnet_packet(path, packet);
}

bool write_wwivnet_packet(const std::filesystem::path& path, const NetPacket& p) {
  VLOG(2) << "write_wwivnet_packet: " << path.string();
  LOG(INFO) << "write_wwivnet_packet: Writing type " << p.nh.main_type << "/" << p.nh.minor_type
            << " message to NetPacket: " << path.string();
  if (p.nh.length != p.text().size()) {
    LOG(ERROR) << "Error while writing NetPacket: " << path.string();
    LOG(ERROR) << "Mismatched text and p.nh.length.  text =" << p.text().size()
               << " nh.length = " << p.nh.length;
    return false;
  }
  File file(path);
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    LOG(ERROR) << "Error while writing NetPacket: " << path.string() << "Unable to open file.";
    return false;
  }
  file.Seek(0L, File::Whence::end);
  const auto num = file.Write(&p.nh, sizeof(net_header_rec));
  if (num != sizeof(net_header_rec)) {
    // Let's fail now since we didn't write this right.
    LOG(ERROR) << "Error while writing NetPacket: " << path.string() << " num written (" << num
               << ") != net_header_rec size.";
    return false;
  }
  if (p.nh.list_len != p.list.size()) {
    LOG(WARNING) << "p.nh.list_len [" << p.nh.list_len << "] != p.list.size() [" << p.list.size()
                 << "]";
  }
  VLOG(4) << "p.nh.list_len: " << p.nh.list_len;
  if (p.nh.list_len) {
    file.Write(&p.list[0], sizeof(uint16_t) * (p.nh.list_len));
  }
  file.Write(p.text());
  file.Close();
  return true;
}

static std::string NetInfoFileName(uint16_t type) {
  switch (type) {
  case net_info_bbslist:
    return BBSLIST_NET;
  case net_info_connect:
    return CONNECT_NET;
  case net_info_sub_lst:
    return SUBS_LST;
  case net_info_wwivnews:
  case net_info_more_wwivnews:
    return "wwivnews.net";
  case net_info_categ_net:
    return CATEG_NET;
  case net_info_network_lst:
    return "networks.lst";
  case net_info_file:
    return "";
  case net_info_binkp:
    return BINKP_NET;
  default:
    return {};
  }
}

NetInfoFileInfo GetNetInfoFileInfo(NetPacket& p) {
  NetInfoFileInfo info{};
  if (p.nh.main_type != main_type_net_info && p.nh.minor_type != net_info_file) {
    // Everything  here should be a main_type_net_info or net_info_file
    LOG(ERROR) << "GetNetInfoFileInfo can't handle type: " << main_type_name(p.nh.main_type) << " ("
               << p.nh.main_type << ")";
    info.valid = false;
    return info;
  }

  if (p.nh.minor_type != net_info_file) {
    // Handle the file types we know about using minor_type
    info.filename = NetInfoFileName(p.nh.minor_type);
    info.data = p.text();
    info.valid = true;
    info.overwrite = true;
    return info;
  }
  auto text = p.text();
  if (text.size() < 4) {
    return info;
  }
  const uint16_t flags = (text.at(1) << 8) | text.at(0);
  VLOG(2) << "flags: " << flags;
  const char* fntemp = &text[2];
  std::string fn(fntemp);
  if (fn.empty() || fn.size() > 8) {
    // still BAD.
    LOG(ERROR) << "filename length not right; must be at [0,8]; was: " << fn.size();
    return info;
  }
  VLOG(2) << "fn: '" << fn << "'; "
          << "len: " << fn.size();
  const auto pos = fn.size() + sizeof(uint16_t) + 1;
  if (text.size() < pos) {
    // still bad
    LOG(ERROR) << "text length too short; must be at least: " << pos;
    return info;
  }
  info.data = text.substr(pos);
  // Since Windows is case sensitive, this is fine.  Since we want lower
  // case filenames on Linux, this is also fine.
  StringLowerCase(&fn);
  const auto zip = (flags & 0x02) != 0;
  if (zip) {
    info.filename = StrCat(fn, ".zip");
  } else {
    info.filename = StrCat(fn, ".net");
  }
  info.overwrite = (flags & 1) != 0;
  info.valid = true;
  return info;
}

static bool need_to_update_routing(uint16_t main_type) {
  switch (main_type) {
  case main_type_email:
  case main_type_post:
  case main_type_pre_post:
  case main_type_email_name:
  case main_type_file:
  case main_type_new_post:
    return true;
  default:
    return false;
  }
}

static int number_of_header_lines(uint16_t main_type) {
  // either 3 or 4.
  switch (main_type) {
  case main_type_email:
  case main_type_post:
  case main_type_pre_post:
    return 3;
  case main_type_email_name:
  case main_type_file:
  case main_type_new_post:
    return 4;
  default:
    return 0;
  }
}

NetPacket::NetPacket(const net_header_rec& h, std::vector<uint16_t> l, std::string t)
    : nh(h), list(std::move(l)), text_(std::move(t)) {
  if (nh.list_len != list.size()) {
    LOG(ERROR) << "ERROR: Malformed NetPacket: list_len [" << nh.list_len << "] != list.size() ["
               << list.size() << "]";
  }
  if (!list.empty() && nh.tosys != 0) {
    LOG(ERROR) << "ERROR: Malformed NetPacket: list is not empty and nh.tosys != 0";
  }
  update_header();
}

NetPacket::NetPacket(const net_header_rec& h, const std::vector<uint16_t>& l, const ParsedNetPacketText& t)
    : NetPacket(h, l, ParsedNetPacketText::ToPacketText(t)) {}

NetPacket::NetPacket() = default;

bool NetPacket::UpdateRouting(const Network& net) {
  if (!need_to_update_routing(nh.main_type)) {
    return false;
  }

  std::ostringstream ss;
  ss << "\004"
     << "0R " << wwiv_network_compatible_version() << " - " << date() << " " << times() << " "
     << net.name << " ->" << net.sysnum << "\r\n";

  const auto routing_information = ss.str();

  if (nh.length + routing_information.size() >= (32 * 1024)) {
    LOG(INFO) << "Can't updating routing information, already have 32k of message.";
    return false;
  }
  nh.length += size_uint32(routing_information);

  // Need to skip over either 3 or 4 lines 1st depending on the NetPacket type.
  const auto lines = number_of_header_lines(nh.main_type);
  auto iter = text_.begin();
  for (auto i = 0; i < lines; i++) {
    // Skip over this line
    [[maybe_unused]] auto _ = get_message_field(text_, iter, {'\0', '\r', '\n'}, 80);
  }

  const auto pos = std::distance(text_.begin(), iter);
  text_.insert(pos, routing_information);
  return true;
}

void NetPacket::set_text(const std::string& text) {
  text_ = text;
  update_header();
}

void NetPacket::set_text(std::string&& text) {
  text_ = std::move(text);
  update_header();
}

void NetPacket::update_header() {
  nh.length = stl::size_uint32(text_);
  nh.list_len = static_cast<uint16_t>(list.size());
}

int NetPacket::length() const {
  return nh.length;
}

/////////////////////////////////////////////////////////////////////////////
// NetMailFile

NetMailFile::NetMailFile(const std::filesystem::path& path, bool process_de, bool allow_write)
    : file_(path), process_de_(process_de) {
  const int mode = File::modeBinary | ((allow_write) ? File::modeReadWrite : File::modeReadOnly);
  open_ = file_.Open(mode);
  if (!open_) {
    LOG(ERROR) << "Unable to open file: " << path.string() << "; error: " << file_.last_error();
  }
}


NetMailFile::~NetMailFile() { Close(); }

void NetMailFile::Close() noexcept {
  if (open_) {
    file_.Close();
    open_ = false;
  }
}


std::tuple<NetPacket, ReadNetPacketResponse> NetMailFile::Read() {
  auto t = read_packet(file_, process_de_);
  // Update the the last response.
  last_read_response_ = std::get<1>(t);
  return t;
}


NetMailFile::iterator NetMailFile::begin() {
  file_.Seek(0, File::Whence::begin);
  return iterator(*this); 
}

NetMailFile::iterator::iterator(NetMailFile& f)
    : NetMailFile::iterator::iterator(f, ReadNetPacketResponse::NOT_OPENED) {}

NetMailFile::iterator::iterator(NetMailFile& f, ReadNetPacketResponse response)
    : f_(f), response_(response) {
  if (response == ReadNetPacketResponse::NOT_OPENED && f_.file_.current_position() == 0) {
    // start of file that is not yet opened (or unknown).
    std::tie(packet_, response_) = f_.Read();
  }
}


uint16_t get_forsys(const wwiv::sdk::BbsListNet& b, uint16_t node) {
  VLOG(2) << "get_forsys (forward to systen number) for node: " << node;

  if (node == 0) {
    VLOG(2) << "get_forsys: zero node. ";
    return 0;
  }

  auto n = b.node_config_for(node);
  if (!n) {
    VLOG(2) << "get_forsys: no route to node: " << node;
    return WWIVNET_NO_NODE;
  }
  if (n->forsys == WWIVNET_NO_NODE) {
    VLOG(2) << "get_forsys: no route to node: " << node;
    return WWIVNET_NO_NODE;
  }
  VLOG(2) << "get_forsys: route to node: " << node << "; is through node: " << n->forsys;
  return n->forsys;
}

// static
std::filesystem::path NetPacket::wwivnet_packet_path(const Network& net, uint16_t node) {
  if (node == net.sysnum || node == 0) {
    // Messages to us to into local.net.
    return FilePath(net.dir, LOCAL_NET);
  }
  if (node == WWIVNET_NO_NODE) {
    return FilePath(net.dir, DEAD_NET);
  }
  return FilePath(net.dir, fmt::format("s{}.net", node));
}

ParsedNetPacketText::ParsedNetPacketText(uint16_t typ) : main_type_(typ) {}

void ParsedNetPacketText::set_date(daten_t d) { date_ = daten_to_wwivnet_time(d); }
void ParsedNetPacketText::set_date(const std::string& d) { date_ = d; }
void ParsedNetPacketText::set_subtype(const std::string& s) { subtype_or_email_to_ = s; }
void ParsedNetPacketText::set_to(const std::string& s) { subtype_or_email_to_ = s; }
void ParsedNetPacketText::set_title(const std::string& t) { title_ = t; }
void ParsedNetPacketText::set_sender(const std::string& s) { sender_ = s; }
void ParsedNetPacketText::set_text(const std::string& t) { text_ = t; }
void ParsedNetPacketText::set_main_type(uint16_t t) { main_type_ = t; }
uint16_t ParsedNetPacketText::main_type() const noexcept { return main_type_; }

const std::string& ParsedNetPacketText::subtype() const noexcept { return subtype_or_email_to_; }
const std::string& ParsedNetPacketText::subtype_or_email_to() const noexcept {
  return subtype_or_email_to_;
}
const std::string& ParsedNetPacketText::to() const noexcept { return subtype_or_email_to_; }
const std::string& ParsedNetPacketText::title() const noexcept { return title_; }
const std::string& ParsedNetPacketText::sender() const noexcept { return sender_; }
const std::string& ParsedNetPacketText::date() const noexcept { return date_; }
const std::string& ParsedNetPacketText::text() const noexcept { return text_; }

// static
ParsedNetPacketText ParsedNetPacketText::FromText(uint16_t typ, const std::string& raw) {
  auto iter = std::begin(raw);
  ParsedNetPacketText p{typ};
  p.subtype_or_email_to_ = get_message_field(raw, iter, {'\0', '\r', '\n'}, 80);
  p.title_ = get_message_field(raw, iter, {'\0', '\r', '\n'}, 80);
  p.sender_ = get_message_field(raw, iter, {'\0', '\r', '\n'}, 80);
  p.date_ = get_message_field(raw, iter, {'\0', '\r', '\n'}, 80);

  // This is the message body including any control lines (^D0 or ^A)
  // that are part of it.
  p.text_ = std::string(iter, std::end(raw));
  return p;
}

// static
ParsedNetPacketText ParsedNetPacketText::FromNetPacket(const NetPacket& p) {
  return FromText(p.nh.main_type, p.text());
}

// static
std::string ParsedNetPacketText::ToPacketText(const ParsedNetPacketText& ppt) {
  std::string text;
  if (ppt.main_type() == main_type_new_post || ppt.main_type() == main_type_email_name) {
    // These types put the subtype or to address 1st
    text.append(ppt.subtype_or_email_to());
    text.push_back(0);
  }
  text.append(ppt.title());
  text.push_back(0);
  const std::string crlf{"\r\n"};
  text.append(ppt.sender()).append(crlf);
  text.append(ppt.date()).append(crlf);
  text.append(ppt.text());
  return text;
}

void rename_pend(const std::filesystem::path& directory, const std::string& filename, char network_app_id) {
  const auto pend_filename(FilePath(directory, filename));
  if (!File::Exists(pend_filename)) {
    LOG(INFO) << " pending file does not exist: " << pend_filename;
    return;
  }
  const auto num = filename.substr(1);
  const char prefix = (to_number<int>(num)) ? '1' : '0';

  for (int i = 0; i < 1000; i++) {
    const auto new_basename = fmt::format("p{}-{}-{}.net", prefix, network_app_id, i);
    const auto new_filename = FilePath(directory, new_basename);
    if (File::Rename(pend_filename, new_filename)) {
      LOG(INFO) << "renamed file: '" << pend_filename << "' to: '" << new_filename << "'";
      return;
    }
  }
  LOG(ERROR) << "all attempts failed to rename_wwivnet_pend";
}

std::string create_pend(const std::filesystem::path& directory, bool local, char network_app_id) {
  const uint8_t prefix = (local) ? 0 : 1;
  for (auto i = 0; i < 1000; i++) {
    auto filename = fmt::format("p{}-{}-{}.net", prefix, network_app_id, i);
    const auto pend_fn = FilePath(directory, filename);
    if (File::Exists(pend_fn)) {
      continue;
    }
    File f(pend_fn);
    if (f.Open(File::modeCreateFile | File::modeReadWrite | File::modeExclusive)) {
      LOG(INFO) << "Created pending file: " << filename;
      return filename;
    }
  }
  LOG(ERROR) << "all attempts failed to create_pend";
  return "";
}

std::string main_type_name(uint16_t typ) {
  switch (typ) {
  case main_type_net_info:
    return "main_type_net_info";
  case main_type_email:
    return "main_type_email";
  case main_type_post:
    return "main_type_post";
  case main_type_file:
    return "main_type_file";
  case main_type_pre_post:
    return "main_type_pre_post";
  case main_type_external:
    return "main_type_external";
  case main_type_email_name:
    return "main_type_email_name";
  case main_type_net_edit:
    return "main_type_net_edit";
  case main_type_sub_list:
    return "main_type_sub_list";
  case main_type_extra_data:
    return "main_type_extra_data";
  case main_type_group_bbslist:
    return "main_type_group_bbslist";
  case main_type_group_connect:
    return "main_type_group_connect";
  case main_type_group_binkp:
    return "main_type_group_binkp";
  case main_type_group_info:
    return "main_type_group_info";
  case main_type_ssm:
    return "main_type_ssm";
  case main_type_sub_add_req:
    return "main_type_sub_add_req";
  case main_type_sub_drop_req:
    return "main_type_sub_drop_req";
  case main_type_sub_add_resp:
    return "main_type_sub_add_resp";
  case main_type_sub_drop_resp:
    return "main_type_sub_drop_resp";
  case main_type_sub_list_info:
    return "main_type_sub_list_info";
  case main_type_new_post:
    return "main_type_new_post";
  case main_type_new_external:
    return "main_type_new_external";
  case main_type_game_pack:
    return "main_type_game_pack";
  default:
    return fmt::format("UNKNOWN type #{}", typ);
  }
}

std::string net_info_minor_type_name(uint16_t typ) {
  switch (typ) {
  case net_info_general_message:
    return "net_info_general_message";
  case net_info_bbslist:
    return "net_info_bbslist";
  case net_info_connect:
    return "net_info_connect";
  case net_info_sub_lst:
    return "net_info_sub_lst";
  case net_info_wwivnews:
    return "net_info_wwivnews";
  case net_info_fbackhdr:
    return "net_info_fbackhdr";
  case net_info_more_wwivnews:
    return "net_info_more_wwivnews";
  case net_info_categ_net:
    return "net_info_categ_net";
  case net_info_network_lst:
    return "net_info_network_lst";
  case net_info_file:
    return "net_info_file";
  case net_info_binkp:
    return "net_info_binkp";
  default:
    return fmt::format("UNKNOWN type #{}", typ);
  }
}

std::string get_subtype_from_packet_text(const std::string& text) {
  auto iter = text.begin();
  return get_message_field(text, iter, {'\0', '\r', '\n'}, 80);
}

/** Creates an outbound NetPacket to be sent */
NetPacket create_packet_from_wwiv_message(const wwiv::sdk::msgapi::Message& m,
                                       const std::string& subtype, std::set<uint16_t> receipients) {

  std::vector<uint16_t> list;
  net_header_rec nh{};
  const auto& h = m.header();
  nh.daten = h.daten();
  nh.fromsys = h.from_system();
  nh.fromuser = h.from_usernum();
  nh.list_len = 0;
  nh.tosys = 0;
  if (receipients.size() == 1) {
    nh.tosys = *receipients.begin();
  } else {
    nh.list_len = static_cast<uint16_t>(receipients.size());
    for (auto r : receipients) {
      list.push_back(r);
    }
  }

  nh.main_type = main_type_new_post;
  nh.method = 0;
  nh.minor_type = 0;
  nh.touser = 0;

  // TODO(rushfan): Use ParsedNetPacketText here?
  // text is subtype<0>title<0>sender<CRLF>date<crlf>body
  auto text = subtype;
  text.push_back(0);
  text.append(h.title());
  text.push_back(0);
  text.append(h.from());
  text.append("\r\n");
  text.append(daten_to_wwivnet_time(nh.daten));
  text.append("\r\n");
  text.append(m.text().string());

  nh.length = stl::size_uint32(text);

  NetPacket p(nh, list, text);
  return p;
}

static std::string change_subtype_to(const std::string& org_text, const std::string& new_subtype) {
  auto iter = org_text.begin();
  auto subtype = get_message_field(org_text, iter, {'\0', '\r', '\n'}, 80);
  auto result = new_subtype;
  result.push_back(0); // Add NULL
  result += std::string(iter, std::end(org_text));
  return result;
}

bool write_wwivnet_packet_or_log(const Network& net, char network_app_id,
                                 const NetPacket& p) {
  const auto fn = create_pend(net.dir, false, network_app_id);
  if (!write_wwivnet_packet(FilePath(net.dir, fn), p)) {
    LOG(ERROR) << "Error writing NetPacket: " << net.dir << " " << fn;
    return false;
  }
  VLOG(1) << "Wrote NetPacket: " << fn;
  return true;
}

/**
 * Sends the post out via WWIVnet or other networks to the other parties if needed.
 *
 * N.B. If this post originates on this system, use -1 for the original_net_num.
 */
bool send_post_to_subscribers(const std::vector<Network>& nets, int original_net_num,
                              const std::string& original_subtype, const subboard_t& sub,
                              NetPacket& template_packet, const std::set<uint16_t>& subscribers_to_skip,
                              const subscribers_send_to_t& send_to) {
  VLOG(1) << "DEBUG: send_post_to_subscribers; original subtype: " << original_subtype;

  // TODO(rushfan): Replace '2' with a network_app_id passed in
  const auto network_app_id = '2';
  for (const auto& subnet : sub.nets) {
    auto h = template_packet.nh;
    VLOG(1) << "DEBUG: Current network subtype: " << subnet.stype;
    VLOG(1) << "DEBUG: Current network is: " << nets[subnet.net_num].name;
    // if subnet.host == 0, we are the host.
    // if subnet.net_num != context.network_number then we are
    // gating the sub to another network.
    auto are_we_hosting = subnet.host == 0;
    auto are_we_gating = subnet.net_num != original_net_num;
    const auto& current_net = nets[subnet.net_num];
    VLOG(1) << "DEBUG: are_we_hosting: " << std::boolalpha << are_we_hosting;
    VLOG(1) << "DEBUG: are_we_gating:  " << std::boolalpha << are_we_gating;

    if (!are_we_hosting && !are_we_gating &&
        send_to == subscribers_send_to_t::hosted_and_gated_only) {
      // Nothing to do here, so move on to the next subnet in the list
      VLOG(2) << "!hosting and !gating on: " << current_net.name;
      continue;
    }
    if (are_we_gating) {
      // update fromsys
      h.fromsys = current_net.sysnum;
      h.fromuser = 0;
    }
    // This is what the BBS does. Do this in all cases in send_net_post
    if (!h.fromsys) {
      h.fromsys = current_net.sysnum;
    }
    // If the subtype has changed, then change the subtype in the
    // NetPacket text.
    const auto text = subnet.stype == original_subtype
                        ? template_packet.text()
                        : change_subtype_to(template_packet.text(), subnet.stype);
    if (subnet.stype != original_subtype) {
      // we also have to update the nh.length to reflect this change.
      // TODO(rushfan): Really need higher level interface to manipulating
      // WWIVnet NetPackets...
      h.length -= stl::size_uint32(original_subtype);
      h.length += stl::size_uint32(subnet.stype);
    }
    if (current_net.type == network_type_t::ftn) {
      h.tosys = FTN_FAKE_OUTBOUND_NODE;
      VLOG(1) << "current network is FTN";
      h.list_len = 0;
      write_wwivnet_packet_or_log(current_net, network_app_id, NetPacket(h, {}, text));
    } else if (current_net.type == network_type_t::wwivnet) {
      if (subnet.host == 0) {
        // We are the host.
        auto subscribers = ReadSubcriberFile(FilePath(current_net.dir, StrCat("n", subnet.stype, ".net")));
        // Remove the original sender from the set of systems that we will resend this to.
        subscribers.erase(template_packet.nh.fromsys);
        for (const auto& skip : subscribers_to_skip) {
          // Skip the subscribers to skip too.
          subscribers.erase(skip);
        }
        VLOG(1) << "Removing subscriber (sender): " << template_packet.nh.fromsys;
        VLOG(1) << "Read subscribers #: " << subscribers.size();
        VLOG(1) << "Creating wwivnet NetPacket to: ";
        for (const auto x : subscribers) {
          VLOG(1) << "        @" << x;
        }

        if (subscribers.empty()) {
          VLOG(1) << "No subscribers left, skipping sending this NetPacket";
        }
        h.list_len = static_cast<uint16_t>(subscribers.size());
        h.tosys = 0;
        write_wwivnet_packet_or_log(
            current_net, network_app_id,
            NetPacket(h, std::vector<uint16_t>(subscribers.begin(), subscribers.end()), text));
      } else {
        // We are not the host.  Send message to host.
        h.tosys = subnet.host;
        h.list_len = 0;
        write_wwivnet_packet_or_log(current_net, network_app_id, NetPacket(h, {}, text));
      }
    }
  }
  VLOG(1) << "DEBUG: send_post_to_subscribers exiting with true";
  return true;
}

} // namespace wwiv
