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
#include "networkb/packets.h"

#include <string>

#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/version.h"
#include "networkb/net_util.h"
#include "core/datetime.h"
#include "sdk/filenames.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv {
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
    f.Read(&packet.list[0], 2 * packet.nh.list_len);
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
    packet.text.resize(length);
    num_read = f.Read(&packet.text[0], packet.nh.length);
    packet.text.resize(num_read);
  }
  return ReadPacketResponse::OK;
}

bool write_wwivnet_packet(const string& filename, const net_networks_rec& net, const Packet& p) {
  VLOG(2) << "write_wwivnet_packet: " << filename;
  LOG(INFO) << "write_wwivnet_packet: Writing type " << p.nh.main_type << "/" << p.nh.minor_type
            << " message to packet: " << filename;
  if (p.nh.length != p.text.size()) {
    LOG(ERROR) << "Error while writing packet: " << net.dir << filename;
    LOG(ERROR) << "Mismatched text and p.nh.length.  text =" << p.text.size()
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
  file.Write(p.text);
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
    info.data = p.text;
    info.valid = true;
    info.overwrite = true;
    return info;
  }
  auto text = p.text;
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
    : nh(h), list(l), text(t) {
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
  auto iter = text.begin();
  for (auto i = 0; i < lines; i++) {
    // Skip over this line
    get_message_field(text, iter, {'\0', '\r', '\n'}, 80);
  }

  auto pos = std::distance(text.begin(), iter);
  text.insert(pos, routing_information);
  return true;
}

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

} // namespace net
} // namespace wwiv
