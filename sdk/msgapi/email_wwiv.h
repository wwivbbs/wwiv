/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2015-2022, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_EMAIL_WWIV_H
#define INCLUDED_SDK_EMAIL_WWIV_H

#include "core/datafile.h"
#include "sdk/config.h"
#include "sdk/msgapi/message.h"
#include "sdk/msgapi/type2_text.h"
#include <cstdint>
#include <string>

namespace wwiv::sdk::msgapi {

class WWIVMessageApi;

class EmailData {
public:
  EmailData() = default;

  /** The title for this email */
  std::string title;
  /** 
   * The full text of this email 
   * Expects data.text is of the form:
   * SENDER_NAME<cr/lf>DATE_STRING<cr/lf>MESSAGE_TEXT.
   */
  std::string text;
  /** Anonymous settings for this email. 0 for normal */
  uint8_t anony = 0;
  /** The user this email should be sent to (to username) */
  uint16_t user_number = 0;
  /** The system to send this email. 0 for local. (to system) */
  uint16_t system_number = 0;
  /** The user this email should be sent from. */
  uint16_t from_user = 0;
  /** The system that sent this email. 0 for local. */
  uint16_t from_system = 0;
  /** The network number that sent this email.*/
  int from_network_number = 0;
  /** The time stamp for this email. */
  daten_t daten = 0;
};

class WWIVEmail : private Type2Text {
public:
  WWIVEmail(const wwiv::sdk::Config& config, const std::filesystem::path& data_filename,
            const std::filesystem::path& text_filename, int max_net_num);

  bool Close();

  bool AddMessage(EmailData& data);

  /** Total number of active email messages in the system. */
  [[nodiscard]] int number_of_messages();
  /** Total number of email records in the system. This includes any deleted messages. */
  [[nodiscard]] int number_of_email_records() const;

  /** Temporary API to read the header from an email message. */
  bool read_email_header(int email_number, mailrec& m);
  /** Temporary API to read the header and text from an email message. */
  bool read_email_header_and_text(int email_number, mailrec& m, std::string& text);
  /** Deletes an email by number */
  bool DeleteMessage(int email_number);
  /** Delete all email to a specified user */
  bool DeleteAllMailToOrFrom(int user_number);

private:
  bool add_email(const mailrec& m);
  const Config& config_;
  const std::filesystem::path data_filename_;
  core::DataFile<mailrec> mail_file_;
  bool open_{false};
  const int max_net_num_{-1};

  static constexpr uint8_t STORAGE_TYPE = 2;
};

}  // namespace

#endif
