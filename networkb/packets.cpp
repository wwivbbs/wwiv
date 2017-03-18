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

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "networkb/net_util.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv {
namespace net {

bool send_local_email(
    const net_networks_rec& network, net_header_rec& nh,
    const std::string& text, const std::string& byname, const std::string& title) {
  return send_network_email(LOCAL_NET, network, nh, {}, text, byname, title);
}

bool send_network_email(const std::string& filename,
  const net_networks_rec& network, net_header_rec& nh,
  std::vector<uint16_t> list,
  const std::string& text, const std::string& byname, const std::string& title) {

  LOG(INFO) << "Writing type " << nh.main_type << "/" << nh.minor_type << " message to packet: " << filename << "; title: " << title;

  File file(network.dir, filename);
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    return false;
  }
  file.Seek(0L, File::Whence::end);
  nh.list_len = static_cast<uint16_t>(list.size());

  string date = wwiv::sdk::daten_to_wwivnet_time(nh.daten);
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
    int length = packet.nh.length;
    if (packet.nh.method > 0 && process_de) {
      // HACK - this should do this in a shim DE
      // 146 is the sizeof EN/DE header.
      packet.nh.length -= 146;
      char header[147];
      f.Read(header, 146);
    }
    packet.text.resize(length);
    num_read = f.Read(&packet.text[0], packet.nh.length);
    packet.text.resize(num_read);
  }
  return ReadPacketResponse::OK;
}

//bool write_wwivnet_packet(
//  const std::string& filename,
//  const net_networks_rec& net,
//  const net_header_rec& nh, const std::set<uint16_t>& list, const std::string& text) {
//  std::vector<uint16_t> v(list.begin(), list.end());
//  return write_wwivnet_packet(filename, net, nh, v, text);
//}
//
bool write_wwivnet_packet(
  const string& filename,
  const net_networks_rec& net, const Packet& p) {

  LOG(INFO) << "Writing type " << p.nh.main_type << "/" << p.nh.minor_type << " message to packet: " << filename;
  if (p.nh.length != p.text.size()) {
    LOG(ERROR) << "Error while writing packet: " << net.dir << filename;
    LOG(ERROR) << "Mismatched text and p.nh.length.  text =" << p.text.size()
      << " nh.length = " << p.nh.length;
    return false;
  }
  File file(net.dir, filename);
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    return false;
  }
  file.Seek(0L, File::Whence::end);
  auto num = file.Write(&p.nh, sizeof(net_header_rec));
  if (num != sizeof(net_header_rec)) {
    // Let's fail now since we didn't write this right.
    return false;
  }
  if (p.nh.list_len) {
    file.Write(&p.list[0], sizeof(uint16_t) * (p.nh.list_len));
  }
  file.Write(p.text);
  file.Close();
  return true;
}

static string NetInfoFileName(uint16_t type) {
  switch (type) {
  case net_info_bbslist: return BBSLIST_NET;
  case net_info_connect: return CONNECT_NET;
  case net_info_sub_lst: return SUBS_LST;
  case net_info_wwivnews: return "wwivnews.net";
  case net_info_more_wwivnews: return "wwivnews.net";
  case net_info_categ_net: return CATEG_NET;
  case net_info_network_lst: return "networks.lst";
  case net_info_file: return "";
  case net_info_binkp: return BINKP_NET;
  }
  return "";
}

NetInfoFileInfo GetNetInfoFileInfo(Packet& p) {
  NetInfoFileInfo info{};
  if (p.nh.main_type != main_type_net_info) {
    // Everything else here should be a main_type_net_info
    LOG(ERROR) << "GetNetInfoFileInfo can't handle type: "
      << main_type_name(p.nh.main_type) << " (" << p.nh.main_type << ")";
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
  VLOG(2) << "fn: '" << fn << "'; " << "len: " << fn.size();
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
  }
  else {
    info.filename = StrCat(fn, ".net");
  }
  info.overwrite = (flags & 1) != 0;
  info.valid = true;
  return info;
}

}  // namespace net
}  // namespace wwiv

