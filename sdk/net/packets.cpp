/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*            Copyright (C)2016-2017, WWIV Software Services              */
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

#include <string>

#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/version.h"
#include "networkb/net_util.h"
#include "sdk/filenames.h"
#include "sdk/subscribers.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {
namespace net {

bool send_local_email(const net_networks_rec& network, net_header_rec& nh, const std::string& text,
                      const std::string& byname, const std::string& title) {
  return send_network_email(LOCAL_NET, network, nh, {}, text, byname, title);
}

bool send_network_email(const std::string& filename, const net_networks_rec& network,
                        net_header_rec& nh, std::vector<uint16_t> list, const std::string& text,
                        const std::string& byname, const std::string& title) {

  LOG(INFO) << "send_network_email: Writing type " << nh.main_type << "/" << nh.minor_type
            << " message to packet: " << filename << "; title: " << title;

  File file(network.dir, filename);
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    return false;
  }
  file.Seek(0L, File::Whence::end);
  nh.list_len = static_cast<uint16_t>(list.size());

  string date = daten_to_wwivnet_time(nh.daten);
  nh.length = (text.size() + 1 + byname.size() + date.size() + 4 + title.size());
  file.Write(&nh, sizeof(net_header_rec));
  if (nh.list_len) {
    file.Write(&list[0], sizeof(uint16_t) * (nh.list_len));
  }
  char nul[1] = {0};
  if (!title.empty()) {
    // We want the null byte at the end too.
    file.Write(title);
  }
  file.Write(nul, 1);
  if (!byname.empty()) {
    // We want the null byte at the end too.
    file.Write(byname);
  }
  file.Write("\r\n");
  file.Write(date);
  file.Write("\r\n");
  file.Write(text);
  file.Close();
  return true;
}

ReadPacketResponse read_packet(File& f, Packet& packet, bool process_de) {
  auto num_read = f.Read(&packet.nh, sizeof(net_header_rec));
  if (num_read == 0) {
    // at the end of the packet.
    return ReadPacketResponse::END_OF_FILE;
  }

  if (num_read != sizeof(net_header_rec)) {
    LOG(INFO) << "error reading header, got short read of size: " << num_read
              << "; expected: " << sizeof(net_header_rec);
    return ReadPacketResponse::ERROR;
  }

  if (packet.nh.method > 0) {
    LOG(INFO) << "compression: de" << packet.nh.method;
  }

  if (packet.nh.list_len > 0) {
    // skip list of addresses.
    packet.list.resize(packet.nh.list_len);
    f.Read(&packet.list[0], sizeof(uint16_t) * packet.nh.list_len);
  }

  if (packet.nh.length > 0) {
    auto length = packet.nh.length;

    if (length > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) || length < 0) {
      LOG(INFO) << "error reading header, got length too big (underflow?): " << length;
      return ReadPacketResponse::ERROR;
    }

    if (packet.nh.method > 0 && process_de &&
        packet.nh.length > 146 /* Make sure we have enough for a header */) {
      // HACK - this should do this in a shim DE
      // 146 is the sizeof EN/DE header.
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
  return ReadPacketResponse::OK;
}

bool write_wwivnet_packet(const string& filename, const net_networks_rec& net, const Packet& p) {
  VLOG(2) << "write_wwivnet_packet: " << filename;
  LOG(INFO) << "write_wwivnet_packet: Writing type " << p.nh.main_type << "/" << p.nh.minor_type
            << " message to packet: " << filename;
  if (p.nh.length != p.text().size()) {
    LOG(ERROR) << "Error while writing packet: " << net.dir << filename;
    LOG(ERROR) << "Mismatched text and p.nh.length.  text =" << p.text().size()
               << " nh.length = " << p.nh.length;
    return false;
  }
  File file(net.dir, filename);
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    LOG(ERROR) << "Error while writing packet: " << net.dir << filename << "Unable to open file.";
    return false;
  }
  file.Seek(0L, File::Whence::end);
  auto num = file.Write(&p.nh, sizeof(net_header_rec));
  if (num != sizeof(net_header_rec)) {
    // Let's fail now since we didn't write this right.
    LOG(ERROR) << "Error while writing packet: " << net.dir << filename << " num written (" << num
               << ") != net_header_rec size.";
    return false;
  }
  if (p.nh.list_len != p.list.size()) {
    LOG(WARNING) << "p.nh.list_len [" << p.nh.list_len << "] != p.list.size() [" << p.list.size()
                 << "]";
  }
  VLOG(2) << "p.nh.list_len: " << p.nh.list_len;
  if (p.nh.list_len) {
    file.Write(&p.list[0], sizeof(uint16_t) * (p.nh.list_len));
  }
  file.Write(p.text());
  file.Close();
  return true;
}

static string NetInfoFileName(uint16_t type) {
  switch (type) {
  case net_info_bbslist:
    return BBSLIST_NET;
  case net_info_connect:
    return CONNECT_NET;
  case net_info_sub_lst:
    return SUBS_LST;
  case net_info_wwivnews:
    return "wwivnews.net";
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
    return "";
  }
}

NetInfoFileInfo GetNetInfoFileInfo(Packet& p) {
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
  uint16_t flags = (text.at(1) << 8) | text.at(0);
  VLOG(2) << "flags: " << flags;
  const char* fntemp = &text[2];
  string fn(fntemp);
  if (fn.size() == 0 || fn.size() > 8) {
    // still BAD.
    LOG(ERROR) << "filename length not right; must be at [0,8]; was: " << fn.size();
    return info;
  }
  VLOG(2) << "fn: '" << fn << "'; "
          << "len: " << fn.size();
  auto pos = fn.size() + sizeof(uint16_t) + 1;
  if (text.size() < pos) {
    // still bad
    LOG(ERROR) << "text length too short; must be at least: " << pos;
    return info;
  }
  info.data = text.substr(pos);
  // Since Windows is case sensitive, this is fine.  Since we want lower
  // case filenames on Linux, this is also fine.
  StringLowerCase(&fn);
  bool zip = (flags & 0x02) != 0;
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
  }
  return false;
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
  }
  return 0;
}
Packet::Packet(const net_header_rec& h, const std::vector<uint16_t>& l, const std::string& t)
    : nh(h), list(l), text_(t) {
  if (nh.list_len != list.size()) {
    LOG(ERROR) << "ERROR: Malformed packet: list_len [" << nh.list_len << "] != list.size() ["
               << list.size() << "]";
  }
  if (!list.empty() && nh.tosys != 0) {
    LOG(ERROR) << "ERROR: Malformed packet: list is not empty and nh.tosys != 0";
  }
}

bool Packet::UpdateRouting(const net_networks_rec& net) {
  if (!need_to_update_routing(nh.main_type)) {
    return false;
  }

  std::ostringstream ss;
  ss << "\004"
     << "0R " << wwiv_net_version << " - " << date() << " " << times() << " " << net.name << " ->"
     << net.sysnum << "\r\n";

  const auto routing_information = ss.str();

  if (nh.length + routing_information.size() >= (32 * 1024)) {
    LOG(INFO) << "Can't updating routing information, already have 32k of message.";
    return false;
  }
  nh.length += routing_information.size();

  // Need to skip over either 3 or 4 lines 1st depending on the packet type.
  auto lines = number_of_header_lines(nh.main_type);
  auto iter = text_.begin();
  for (auto i = 0; i < lines; i++) {
    // Skip over this line
    get_message_field(text_, iter, {'\0', '\r', '\n'}, 80);
  }

  auto pos = std::distance(text_.begin(), iter);
  text_.insert(pos, routing_information);
  return true;
}

// TODO(rushfan): Invalidate any cached parsed text.
void Packet::set_text(const std::string& text) { text_ = text; }

void Packet::set_text(std::string&& text) { text_ = std::move(text); }

uint16_t get_forsys(const wwiv::sdk::BbsListNet& b, uint16_t node) {
  VLOG(2) << "get_forsys (forward to systen number) for node: " << node;
  auto n = b.node_config_for(node);
  if (node == 0) {
    return 0;
  }
  if (n == nullptr || n->forsys == WWIVNET_NO_NODE) {
    VLOG(2) << "get_forsys: no route to node: " << node;
    return WWIVNET_NO_NODE;
  }
  VLOG(2) << "get_forsys: route to node: " << node << "; is through node: " << n->forsys;
  return n->forsys;
}

// static
std::string Packet::wwivnet_packet_name(const net_networks_rec& net, uint16_t node) {
  if (node == net.sysnum || node == 0) {
    // Messages to us to into local.net.
    return LOCAL_NET;
  }
  if (node == WWIVNET_NO_NODE) {
    return DEAD_NET;
  }
  return StringPrintf("s%u.net", node);
}

ParsedPacketText::ParsedPacketText(uint16_t typ) : main_type_(typ) {}

// static
ParsedPacketText ParsedPacketText::FromPacketText(uint16_t typ, const std::string& raw) {
  auto iter = std::begin(raw);
  ParsedPacketText p{typ};
  p.subtype_or_email_to_ = get_message_field(raw, iter, {'\0', '\r', '\n'}, 80);
  p.title = get_message_field(raw, iter, {'\0', '\r', '\n'}, 80);
  p.sender = get_message_field(raw, iter, {'\0', '\r', '\n'}, 80);
  p.date = get_message_field(raw, iter, {'\0', '\r', '\n'}, 80);

  // This is the message body including any control lines (^D0 or ^A)
  // that are part of it.
  p.text = string(iter, std::end(raw));
  return p;
}

// static
ParsedPacketText ParsedPacketText::FromPacket(const Packet& p){
  return FromPacketText(p.nh.main_type, p.text());
}

// static
std::string ParsedPacketText::ToPacketText(const ParsedPacketText& ppt) {
  std::string text = ppt.subtype_or_email_to_;
  text.push_back(0);
  text.append(ppt.title);
  text.push_back(0);
  const auto crlf = StringPrintf("\r\n");
  text.append(ppt.sender).append(crlf);
  text.append(ppt.date).append(crlf);
  text.append(ppt.text);
  return text;
}

void rename_pend(const string& directory, const string& filename, char network_app_num) {
  File pend_file(directory, filename);
  if (!pend_file.Exists()) {
    LOG(INFO) << " pending file does not exist: " << pend_file;
    return;
  }
  const auto pend_filename(pend_file.full_pathname());
  const auto num = filename.substr(1);
  const char prefix = (to_number<int>(num)) ? '1' : '0';

  for (int i = 0; i < 1000; i++) {
    const auto new_filename =
        StringPrintf("%sp%c-%c-%u.net", directory.c_str(), prefix, network_app_num, i);
    if (File::Rename(pend_filename, new_filename)) {
      LOG(INFO) << "renamed file: '" << pend_filename << "' to: '" << new_filename << "'";
      return;
    }
  }
  LOG(ERROR) << "all attempts failed to rename_wwivnet_pend";
}

std::string create_pend(const string& directory, bool local, char network_app_id) {
  const uint8_t prefix = (local) ? 0 : 1;
  for (auto i = 0; i < 1000; i++) {
    const auto filename = StringPrintf("p%u-%c-%d.net", prefix, network_app_id, i);
    File f(directory, filename);
    if (f.Exists()) {
      continue;
    }
    if (f.Open(File::modeCreateFile | File::modeReadWrite | File::modeExclusive)) {
      LOG(INFO) << "Created pending file: " << filename;
      return filename;
    }
  }
  LOG(ERROR) << "all attempts failed to create_pend";
  return "";
}

string main_type_name(uint16_t typ) {
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
    return StringPrintf("UNKNOWN type #%d", typ);
  }
}

string net_info_minor_type_name(uint16_t typ) {
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
    return StringPrintf("UNKNOWN type #%d", typ);
  }
}

std::string get_subtype_from_packet_text(const std::string& text) {
  auto iter = text.begin();
  return get_message_field(text, iter, {'\0', '\r', '\n'}, 80);
}

/** Creates an outbound packet to be sent */
Packet create_packet_from_wwiv_message(const wwiv::sdk::msgapi::WWIVMessage& m,
                                       const std::string& subtype, std::set<uint16_t> receipients) {

  std::vector<uint16_t> list;
  net_header_rec nh{};
  const auto& h = m.header();
  nh.daten = h.daten();
  nh.fromsys = h.from_system();
  nh.fromuser = h.from_usernum();
  nh.list_len = 0;
  nh.tosys = 0;
  uint16_t receipient = 0;
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

  // text is subtype<0>title<0>sender<cflr>date<crlf>body
  string text = subtype;
  text.push_back(0);
  text.append(h.title());
  text.push_back(0);
  text.append(h.from());
  text.append("\r\n");
  text.append(daten_to_wwivnet_time(nh.daten));
  text.append("\r\n");
  text.append(m.text().text());

  nh.length = text.size();

  Packet p(nh, list, text);
  return p;
}

static std::string change_subtype_to(const std::string& org_text, const std::string& new_subtype) {
  auto iter = org_text.begin();
  auto subtype = get_message_field(org_text, iter, {'\0', '\r', '\n'}, 80);
  std::string result = new_subtype;
  result.push_back(0); // Add NULL
  result += string(iter, std::end(org_text));
  // Should implicitly move by RVO
  return result;
}

// TODO(rushfan): Need to pass in the name of the pending network file to make
// or at least pass in the network character to use in the filename.
bool write_wwivnet_packet_or_log(const net_networks_rec& net, const net_header_rec& h,
                                 std::vector<uint16_t> list, const std::string& text) {
  Packet gp(h, list, text);
  const auto fn = create_pend(net.dir, false, '2');
  if (!write_wwivnet_packet(fn, net, gp)) {
    LOG(ERROR) << "Error writing packet: " << net.dir << " " << fn;
    return false;
  } else {
    VLOG(1) << "Wrote packet: " << fn;
    return true;
  }
}

/**
 * Sends the post out via wwivnet or other networks to the other parties if needed.
 *
 * N.B. If this post originates on this system, use -1 for the original_net_num.
 */
bool send_post_to_subscribers(const std::vector<net_networks_rec>& nets, int original_net_num,
                              const std::string& original_subtype, const subboard_t& sub,
                              Packet& template_packet, std::set<uint16_t> subscribers_to_skip,
                              const subscribers_send_to_t& send_to) {
  VLOG(1) << "DEBUG: send_post_to_subscribers; original subtype: " << original_subtype;

  for (const auto& subnet : sub.nets) {
    auto h = template_packet.nh;
    VLOG(1) << "DEBUG: Current network subtype: " << subnet.stype;
    VLOG(1) << "DEBUG: Current network is: " << nets[subnet.net_num].name;
    // if subnet.host == 0, we are the host.
    // if subnet.net_num != context.network_number then we are
    // gating the sub to another network.
    bool are_we_hosting = subnet.host == 0;
    bool are_we_gating = subnet.net_num != original_net_num;
    const auto& current_net = nets[subnet.net_num];
    VLOG(1) << "DEBUG: are_we_hosting: " << std::boolalpha << are_we_hosting;
    VLOG(1) << "DEBUG: are_we_gating:  " << std::boolalpha << are_we_gating;

    if (!are_we_hosting && !are_we_gating &&
        send_to == subscribers_send_to_t::hosted_and_gated_only) {
      // Nothing to do here, so move on to the next subnet in the list
      continue;
      VLOG(2) << "!hosting and !gating on: " << current_net.name;
    }
    if (are_we_gating) {
      // update fromsys
      h.fromsys = current_net.sysnum;
      h.fromuser = 0;
    }
    // If the subtype has changed, then change the subtype in the
    // packet text.
    const auto text = (subnet.stype == original_subtype)
                          ? template_packet.text()
                          : change_subtype_to(template_packet.text(), subnet.stype);
    if (subnet.stype != original_subtype) {
      // we also have to update the nh.length to reflect this change.
      // TODO(rushfan): Really need higher level interface to manipulating
      // WWIVnet packets...
      h.length -= original_subtype.size();
      h.length += subnet.stype.size();
    }
    if (current_net.type == network_type_t::ftn) {
      h.tosys = FTN_FAKE_OUTBOUND_NODE;
      VLOG(1) << "current network is FTN";
      h.list_len = 0;
      write_wwivnet_packet_or_log(current_net, h, {}, text);
    } else if (current_net.type == network_type_t::wwivnet) {
      if (subnet.host == 0) {
        // We are the host.
        std::set<uint16_t> subscribers;
        bool subscribers_read =
            ReadSubcriberFile(current_net.dir, StrCat("n", subnet.stype, ".net"), subscribers);
        if (subscribers_read) {
          // Remove the original sender from the set of systems
          // that we will resend this to.
          subscribers.erase(template_packet.nh.fromsys);
          VLOG(1) << "Removing subscriber (sender): " << template_packet.nh.fromsys;
          VLOG(1) << "Read subscribers #: " << subscribers.size();
          VLOG(1) << "Creating wwivnet packet to: ";
          for (const auto x : subscribers) {
            VLOG(1) << "        @" << x;
          }
          h.list_len = static_cast<uint16_t>(subscribers.size());
          h.tosys = 0;
          write_wwivnet_packet_or_log(
              current_net, h, std::vector<uint16_t>(subscribers.begin(), subscribers.end()), text);
        } else {
          LOG(ERROR) << "Unable to read subscribers for " << current_net.dir << " " << subnet.stype;
        }
      } else {
        // We are not the host.  Send message to host.
        h.tosys = subnet.host;
        h.list_len = 0;
        write_wwivnet_packet_or_log(current_net, h, {}, text);
      }
    }
  }
  LOG(INFO) << "DEBUG: send_post_to_subscribers"
            << "exiting with true";
  return true;
}

} // namespace net
} // namespace sdk
} // namespace wwiv
