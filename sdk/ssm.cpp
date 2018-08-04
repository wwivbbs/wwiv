/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2017, WWIV Software Services             */
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
#include "sdk/ssm.h"

#include <exception>
#include <stdexcept>
#include <string>

#include "core/datafile.h"
#include "core/file.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "core/datetime.h"
#include "sdk/filenames.h"
#include "sdk/net.h"
#include "sdk/vardec.h"

using std::endl;
using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

SSM::SSM(const wwiv::sdk::Config& config, wwiv::sdk::UserManager& user_manager)
  : data_directory_(config.datadir()), user_manager_(user_manager) {

}

SSM::~SSM() {}

bool SSM::send_remote(const net_networks_rec& net, uint16_t system_number, uint32_t from_user_number, uint32_t user_number, const std::string& t) {
  net_header_rec nh;
  nh.tosys = static_cast<uint16_t>(system_number);
  nh.touser = static_cast<uint16_t>(user_number);
  // This was user_number, but that seems really wrong.  Changing to net.sysnum.
  nh.fromsys = net.sysnum;
  nh.fromuser = static_cast<uint16_t>(from_user_number);
  nh.main_type = main_type_ssm;
  nh.minor_type = 0;
  nh.list_len = 0;
  string text(t);
  nh.daten = daten_t_now();
  if (text.size() > 80) {
    text.resize(80);
  }
  nh.length = text.size();
  nh.method = 0;
  const string packet_filename = StrCat(net.dir, "p0.net");
  File file(packet_filename);
  file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  file.Seek(0L, File::Whence::end);
  file.Write(&nh, sizeof(net_header_rec));
  file.Write(text.c_str(), nh.length);
  file.Close();
  return true;
}

bool SSM::send_local(uint32_t user_number, const std::string& text) {
  User user;
  user_manager_.readuser(&user, user_number);
  if (user.IsUserDeleted()) {
    return false;
  }
  File file(FilePath(data_directory_, SMW_DAT));
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    return false;
  }
  int size = static_cast<int>(file.length() / sizeof(shortmsgrec));
  int pos = size - 1;
  shortmsgrec sm;
  if (pos >= 0) {
    file.Seek(pos * sizeof(shortmsgrec), File::Whence::begin);
    file.Read(&sm, sizeof(shortmsgrec));
    while (sm.tosys == 0 && sm.touser == 0 && pos > 0) {
      --pos;
      file.Seek(pos * sizeof(shortmsgrec), File::Whence::begin);
      file.Read(&sm, sizeof(shortmsgrec));
    }
    if (sm.tosys != 0 || sm.touser != 0) {
      pos++;
    }
  } else {
    pos = 0;
  }
  sm.tosys = static_cast<uint16_t>(0);  // 0 means local
  sm.touser = static_cast<uint16_t>(user_number);
  strncpy(sm.message, text.c_str(), 80);
  sm.message[80] = '\0';
  file.Seek(pos * sizeof(shortmsgrec), File::Whence::begin);
  file.Write(&sm, sizeof(shortmsgrec));
  file.Close();
  user.SetStatusFlag(User::SMW);
  user_manager_.writeuser(&user, user_number);
  return true;
}

bool SSM::delete_local_to_user(uint32_t user_number) {
  DataFile<shortmsgrec> file(data_directory_, SMW_DAT, File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  if (!file) {
    return false;
  }

  int num_recs = file.number_of_records();
  for (int i = 0; i < num_recs; i++) {
    shortmsgrec sm{};
    if (!file.Read(i, &sm)) {
      break;
    }
    if (sm.tosys == 0 && sm.touser == user_number) {
      memset(&sm, 0, sizeof(sm));
      file.Write(i, &sm);
    }
  }
  return true;
}

}
}
