/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#ifndef INCLUDED_BBS_MSGBASE_H
#define INCLUDED_BBS_MSGBASE_H

#include <memory>
#include <string>
#include "common/message_editor_data.h"
#include "core/file.h"
#include "sdk/vardec.h"

class EmailData {
public:
  explicit EmailData(const wwiv::common::MessageEditorData& msged)
      : title(msged.title), silent_mode(msged.silent_mode) {}
  EmailData() = default;
  ~EmailData() = default;

  std::string title;
  messagerec * msg{nullptr};
  uint8_t anony{0};
  uint16_t user_number{0};
  uint16_t system_number{0};
  bool an{false};
  uint16_t from_user{0};
  uint16_t from_system{0};
  int forwarded_code{0};
  int from_network_number{0};

  void set_from_user(int u) { from_user = static_cast<uint16_t>(u); }
  void set_from_system(int u) { from_system = static_cast<uint16_t>(u); }
  void set_user_number(int u) { user_number = static_cast<uint16_t>(u); }

  bool silent_mode;     // Used for ASV and new emails.  No questions, etc.
};

bool ForwardMessage(uint16_t* user_number, uint16_t* system_number);
[[nodiscard]] std::unique_ptr<wwiv::core::File> OpenEmailFile(bool allow_write);
void sendout_email(::EmailData& data);
[[nodiscard]] bool ok_to_mail(uint16_t user_number, uint16_t system_number, bool force_it);
void email(const std::string& title, uint16_t user_number, uint16_t system_number, bool force_it,
           int anony, bool allow_fsed = true);
void imail(const std::string& title, uint16_t user_number, uint16_t system_number);
void delmail(wwiv::core::File& pFile, size_t loc);

[[nodiscard]] std::string fixup_user_entered_email(const std::string& user_input);

#endif
