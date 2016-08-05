/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2016 WWIV Software Services                 */
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

  LOG(INFO) << "Writing type " << nh.main_type << "/" << nh.minor_type << " message to packet: " << filename;

  File file(network.dir, filename);
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    return false;
  }
  file.Seek(0L, File::seekEnd);
  nh.list_len = static_cast<uint16_t>(list.size());

  string date = wwiv::sdk::daten_to_humantime(nh.daten);
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

bool write_packet(
  const std::string& filename,
  const net_networks_rec& net,
  const net_header_rec& nh, const std::set<uint16_t>& list, const std::string& text) {
  std::vector<uint16_t> v(list.begin(), list.end());
  return write_packet(filename, net, nh, v, text);
}

bool write_packet(
  const string& filename,
  const net_networks_rec& net,
  const net_header_rec& nh, const std::vector<uint16_t>& list, const string& text) {

  LOG(INFO) << "Writing type " << nh.main_type << "/" << nh.minor_type << " message to packet: " << filename;
  if (nh.length != text.size()) {
    LOG(ERROR) << "Error while writing packet: " << net.dir << filename;
    LOG(ERROR) << "Mismatched text and nh.length.  text =" << text.size()
      << " nh.length = " << nh.length;
    return false;
  }
  File file(net.dir, filename);
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    return false;
  }
  file.Seek(0L, File::seekEnd);
  file.Write(&nh, sizeof(net_header_rec));
  if (nh.list_len) {
    file.Write(&list[0], sizeof(uint16_t) * (nh.list_len));
  }
  file.Write(text);
  file.Close();
  return true;
}


}  // namespace net
}  // namespace wwiv

