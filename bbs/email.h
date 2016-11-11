/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_MSGBASE_H__
#define __INCLUDED_BBS_MSGBASE_H__

#include <memory>
#include <string>
#include "bbs/inmsg.h"
#include "core/file.h"
#include "sdk/vardec.h"

class EmailData {
public:
  EmailData(const MessageEditorData& msged) : title(msged.title), silent_mode(msged.silent_mode) {}
  explicit EmailData() {}
  ~EmailData() {}

  std::string title;
  messagerec * msg;
  int anony = 0;
  int user_number = 0;
  int system_number = 0;
  bool an = 0;
  int from_user = 0;
  int from_system = 0;
  int forwarded_code = 0;
  int from_network_number = 0;

  bool silent_mode;     // Used for ASV and newemail emails.  No questions, etc.
};

bool ForwardMessage(int *user_number, int *system_number);
std::unique_ptr<File> OpenEmailFile(bool allow_write);
void sendout_email(EmailData& data);
bool ok_to_mail(int user_number, int system_number, bool force_it);
void email(const std::string& title, int user_number, int system_number, bool force_it, int anony, bool allow_fsed = true);
void imail(int user_number, int system_number);
void delmail(File *pFile, size_t loc);

#endif  // __INCLUDED_BBS_MSGBASE_H__