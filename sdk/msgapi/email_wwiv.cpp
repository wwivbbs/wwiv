/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2016, WWIV Software Services             */
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
#include "sdk/msgapi/email_wwiv.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/datafile.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "bbs/subacc.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/datetime.h"
#include "sdk/vardec.h"
#include "sdk/msgapi/message_api_wwiv.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

using std::string;
using std::make_unique;
using std::unique_ptr;
using std::vector;
using wwiv::core::DataFile;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

constexpr char CD = 4;
constexpr char CZ = 26;

WWIVEmail::WWIVEmail(
  const std::string& data_filename, const std::string& text_filename, int max_net_num)
  : Type2Text(text_filename), data_filename_(data_filename),
    max_net_num_(max_net_num) {
  DataFile<mailrec> data(data_filename_, File::modeBinary | File::modeReadOnly);
  open_ = data.file().Exists();
}

WWIVEmail::~WWIVEmail() {
  Close();
}

bool WWIVEmail::Close() {
  open_ = false;
  return true;
}


bool WWIVEmail::AddMessage(const EmailData& data) {
  mailrec m = {};
  strcpy(m.title, data.title.c_str());
  m.msg = { STORAGE_TYPE, 0xffffff };
  m.anony = static_cast<unsigned char>(data.anony);
  m.fromsys = static_cast<uint16_t>(data.from_system);
  m.fromuser = static_cast<uint16_t>(data.from_user);
  m.tosys = static_cast<uint16_t>(data.system_number);
  m.touser = static_cast<uint16_t>(data.user_number);
  m.status = 0;
  m.daten = data.daten;

  if (m.fromsys && max_net_num_ > 1) {
    // always trim to WWIV_MESSAGE_TITLE_LENGTH now.
    m.title[71] = '\0';
    m.status |= status_new_net;
    m.network.network_msg.net_number = static_cast<int8_t>(data.from_network_number);
  }

  savefile(data.text, &m.msg);
  return add_email(m);
}

// Implementation Details

bool WWIVEmail::add_email(const mailrec& m) {
  DataFile<mailrec> mail_file(data_filename_, File::modeBinary|File::modeReadWrite);
  if (!mail_file) {
    return false;
  }
  int recno = 0;
  int max_size = mail_file.number_of_records();
  if (max_size > 0) {
    mailrec temprec;
    recno = max_size - 1;
    mail_file.Seek(recno);
    mail_file.Read(recno, &temprec);
    while (recno > 0 && temprec.tosys == 0 && temprec.touser == 0) {
      --recno;
      mail_file.Read(recno, &temprec);
    }
    if (temprec.tosys || temprec.touser) {
      ++recno;
    }
  }

  return mail_file.Write(recno, &m);
}

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv
