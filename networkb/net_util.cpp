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
#include "networkb/net_util.h"

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

void rename_pend(const string& directory, const string& filename, uint8_t network_app_num) {
  File pend_file(directory, filename);
  if (!pend_file.Exists()) {
    LOG(INFO) << " pending file does not exist: " << pend_file;
    return;
  }
  const string pend_filename(pend_file.full_pathname());
  const string num = filename.substr(1);
  const string prefix = (atoi(num.c_str())) ? "1" : "0";

  for (int i = 0; i < 1000; i++) {
    const string new_filename =
      StringPrintf("%sp%s-%u-%u.net", directory.c_str(), prefix.c_str(), network_app_num, i);
    if (File::Rename(pend_filename, new_filename)) {
      LOG(INFO) << "renamed file to: " << new_filename;
      return;
    }
  }
  LOG(ERROR) << "all attempts failed to rename_pend";
}

std::string create_pend(const string& directory, bool local, uint8_t network_app_num) {
  uint8_t prefix = (local) ? 0 : 1;
  for (int i = 0; i < 1000; i++) {
    const string filename =
      StringPrintf("p%u-%u-%u.net", prefix, network_app_num, i);
    File f(directory, filename);
    if (f.Exists()) {
      continue;
    }
    if (f.Open(File::modeCreateFile | File::modeReadWrite | File::modeExclusive)) {
      LOG(INFO) << "Created pending file: " << filename;
    }
    return filename;
  }
  LOG(ERROR) << "all attempts failed to create_pend";
  return "";
}

bool send_network(const std::string& filename,
  const net_networks_rec& network, net_header_rec& nh,
  std::vector<uint16_t> list,
  const std::string& text, const std::string& byname, const std::string& title) {

  LOG(INFO) << "Writing type " << nh.main_type << " message to packet: " << filename;

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

  LOG(INFO) << "Writing type " << nh.main_type << " message to packet: " << filename;
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

bool send_local(
    const net_networks_rec& network, net_header_rec& nh,
    const std::string& text, const std::string& byname, const std::string& title) {
  return send_network(LOCAL_NET, network, nh, {}, text, byname, title);
}

string main_type_name(int typ) {
  switch (typ) {
  case main_type_net_info: return "main_type_net_info";
  case main_type_email: return "main_type_email";
  case main_type_post: return "main_type_post";
  case main_type_file: return "main_type_file";
  case main_type_pre_post: return "main_type_pre_post";
  case main_type_external: return "main_type_external";
  case main_type_email_name: return "main_type_email_name";
  case main_type_net_edit: return "main_type_net_edit";
  case main_type_sub_list: return "main_type_sub_list";
  case main_type_extra_data: return "main_type_extra_data";
  case main_type_group_bbslist: return "main_type_group_bbslist";
  case main_type_group_connect: return "main_type_group_connect";
  case main_type_group_binkp: return "main_type_group_binkp";
  case main_type_group_info: return "main_type_group_info";
  case main_type_ssm: return "main_type_ssm";
  case main_type_sub_add_req: return "main_type_sub_add_req";
  case main_type_sub_drop_req: return "main_type_sub_drop_req";
  case main_type_sub_add_resp: return "main_type_sub_add_resp";
  case main_type_sub_drop_resp: return "main_type_sub_drop_resp";
  case main_type_sub_list_info: return "main_type_sub_list_info";
  case main_type_new_post: return "main_type_new_post";
  case main_type_new_external: return "main_type_new_external";
  case main_type_game_pack: return "main_type_game_pack";
  default: return StringPrintf("UNKNOWN type #%d", typ);
  }
}

string net_info_minor_type_name(int typ) {
  switch (typ) {
  case net_info_general_message: return "net_info_general_message";
  case net_info_bbslist: return "net_info_bbslist";
  case net_info_connect: return "net_info_connect";
  case net_info_sub_lst: return "net_info_sub_lst";
  case net_info_wwivnews: return "net_info_wwivnews";
  case net_info_fbackhdr: return "net_info_fbackhdr";
  case net_info_more_wwivnews: return "net_info_more_wwivnews";
  case net_info_categ_net: return "net_info_categ_net";
  case net_info_network_lst: return "net_info_network_lst";
  case net_info_file: return "net_info_file";
  case net_info_binkp: return "net_info_binkp";
  default: return StringPrintf("UNKNOWN type #%d", typ);
  }
}

void AddStandardNetworkArgs(wwiv::core::CommandLine& cmdline, const std::string& current_directory) {
  cmdline.add_argument({"network_number", "Network number to use. (Deprecated, use --net).", "0"});
  cmdline.add_argument({"net", "Network number to use (i.e. 0).", "0"});
  cmdline.add_argument({"bbsdir", "(optional) BBS directory if other than current directory", current_directory});
  cmdline.add_argument(BooleanCommandLineArgument("skip_net", "Skip invoking network1/network2/network3"));
}


NetworkCommandLine::NetworkCommandLine(wwiv::core::CommandLine& cmdline)
  : bbsdir_(cmdline.arg("bbsdir").as_string()),
    config_(bbsdir_), networks_(config_) {
  if (!config_.IsInitialized()) {
    LOG(ERROR) << "Unable to load CONFIG.DAT.";
    initialized_ = false;
  }
  if (!networks_.IsInitialized()) {
    LOG(ERROR) << "Unable to load networks.";
    initialized_ = false;
  }
  network_number_ = cmdline.arg("network_number").as_int();
  auto net_num = cmdline.arg("net").as_int();
  if (network_number_ == 0 && net_num > 0) {
    // Temporarily allow --net as an alias for network_number
    network_number_ = net_num;
  }
  network_name_ = networks_[network_number_].name;
  StringLowerCase(&network_name_);

  network_ = networks_.networks()[network_number_];
}


}  // namespace net
}  // namespace wwiv

