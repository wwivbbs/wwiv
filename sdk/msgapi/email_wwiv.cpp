/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "bbs/subacc.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/datetime.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
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
  const std::string& root_directory,
  const std::string& data_filename, const std::string& text_filename, int max_net_num)
  : Type2Text(text_filename), root_directory_(root_directory), data_filename_(data_filename),
    mail_file_(data_filename_, File::modeBinary | File::modeReadWrite, File::shareDenyReadWrite),
    max_net_num_(max_net_num) {
  open_ = mail_file_ && mail_file_.file().Exists();
}

bool WWIVEmail::Close() {
  open_ = false;
  return true;
}

static bool modify_email_waiting(const Config& config, uint16_t email_usernum, int delta) {
  UserManager um(config);
  User u{};
  if (!um.readuser(&u, email_usernum)) {
    return false;
  }
  int newval = u.GetNumMailWaiting();
  newval += delta;
  if (newval < 0) { newval = 0; }
  u.SetNumMailWaiting(newval);
  if (!um.writeuser(&u, email_usernum)) {
    return false;
  }
  return true;
}

static bool increment_email_counters(const string& root_directory, uint16_t email_usernum) {
  statusrec_t statusrec{};
  Config config(root_directory);
  if (!config.IsInitialized()) {
    LOG(ERROR) << "Unable to load CONFIG.DAT.";
    return false;
  }
  DataFile<statusrec_t> file(config.datadir(), STATUS_DAT,
    File::modeBinary | File::modeReadWrite);
  if (!file) {
    return false;
  }
  if (!file.Read(0, &statusrec)) {
    return false;
  }
  if (email_usernum == 1) {
    ++statusrec.fbacktoday;
  } else {
    ++statusrec.emailtoday;
  }
  if (!file.Write(0, &statusrec)) {
    return false;
  }

  return modify_email_waiting(config, email_usernum, 1);
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

  string text = data.text;
  // WWIV 4.x requires a control-Z to terminate the message, WWIV 5.x
  // does not, and removes it on read.
  if (text.back() != CZ) {
    text.push_back(CZ);
  }

  if (!savefile(text, &m.msg)) {
    return false;
  }
  increment_email_counters(root_directory_, m.touser);
  return add_email(m);
}

static bool is_mailrec_deleted(const mailrec& h) {
  return h.tosys == 0 && h.touser == 0 && h.daten == 0xffffffff;
}

/** Total number of email messages in the system. */
int WWIVEmail::number_of_messages() {
  auto num = mail_file_.number_of_records();
  if (num == 0) { return 0; }
  std::vector<mailrec> headers;
  mail_file_.Seek(0);
  if (!mail_file_.ReadVector(headers)) {
    // WTF
    return false;
  }
  int count = 0;
  for (const auto& h : headers) {
    if (!is_mailrec_deleted(h)) {
      ++count;
    }
  }
  return count;
}

int WWIVEmail::number_of_email_records() {
  if (!open_ || !mail_file_) {
    return 0;
  }
  return mail_file_.number_of_records();
}

/** Temporary API to read the header from an email message. */
bool WWIVEmail::read_email_header(int email_number, mailrec& m) {
  if (!open_) {
    return false;
  }
  if (mail_file_.number_of_records() == 0) {
    return false;
  }
  if (!mail_file_.Read(email_number, &m)) {
    return false;
  }
  if (is_mailrec_deleted(m)) {
    return false;
  }
  return true;
}

/** Temporary API to read the header and text from an email message. */
bool WWIVEmail::read_email_header_and_text(int email_number, mailrec& m, std::string& text) {
  if (!read_email_header(email_number, m)) {
    return false;
  }
  return readfile(&m.msg, &text);
}

/** Deletes an email by number */
bool WWIVEmail::DeleteMessage(int email_number) {
  if (!open_) { return false; }
  auto num_records = static_cast<decltype(email_number)>(mail_file_.number_of_records());
  if (num_records == 0 || email_number > num_records) {
    return false;
  }

  mailrec m{};
  if (!mail_file_.Read(email_number, &m)) {
    return false;
  }

  if (m.touser == 0 && m.tosys == 0) {
    // Already deleted.
    return true;
  }

  bool rm = true;
  if (m.status & status_multimail) {
    bool others_found = false;
    for (auto i = 0; i < num_records; i++) {
      if (i != email_number) {
        mailrec m1{};
        if (!mail_file_.Read(i, &m1)) { continue; }
        if ((m.msg.stored_as == m1.msg.stored_as) && (m.msg.storage_type == m1.msg.storage_type) && (m1.daten != 0xffffffff)) {
          others_found = true;
        }
      }
    }
    if (others_found) {
      rm = false;
    }
  }
  if (rm) {
    remove_link(m.msg);
  }

  if (m.tosys == 0) {
    Config config(root_directory_);
    if (config.IsInitialized()) {
      modify_email_waiting(config, m.touser, -1);
    }
  }

  // Clear out the email record and write it back to EMAIL.DAT
  // so the slot may be reused later.
  m.touser = 0;
  m.tosys = 0;
  m.daten = 0xffffffff;
  m.msg.storage_type = 0;
  m.msg.stored_as = 0xffffffff;
  return mail_file_.Write(email_number, &m);
}

bool WWIVEmail::DeleteAllMailToOrFrom(int user_number) {
  if (user_number <= 1) {
    // You can not take command.
    return false;
  }
  std::vector<mailrec> headers;
  mail_file_.Seek(0);
  if (!mail_file_.ReadVector(headers)) {
    // WTF
    return false;
  }
  for (auto i = 0; i < size_int(headers); i++) {
    const auto& m = headers.at(i);
    if ((m.tosys == 0 && m.touser == user_number) || (m.fromsys == 0 && m.fromuser == user_number)) {
      DeleteMessage(i);
    }
  }
  return true;
}

// Implementation Details

bool WWIVEmail::add_email(const mailrec& m) {
  if (!open_) {
    return false;
  }
  int recno = 0;
  int max_size = mail_file_.number_of_records();
  if (max_size > 0) {
    mailrec temprec{};
    recno = max_size - 1;
    mail_file_.Read(recno, &temprec);
    while (recno > 0 && temprec.tosys == 0 && temprec.touser == 0) {
      --recno;
      mail_file_.Read(recno, &temprec);
    }
    if (temprec.tosys || temprec.touser) {
      ++recno;
    }
  }

  return mail_file_.Write(recno, &m);
}

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv
