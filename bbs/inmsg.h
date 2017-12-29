/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_INMSG_H__
#define __INCLUDED_BBS_INMSG_H__

#include <string>

#include "bbs/external_edit.h"
#include "sdk/vardec.h"

enum class FsedFlags {
  NOFSED, FSED, WORKSPACE
};

class MessageEditorData {
public:
  MessageEditorData() {}
  ~MessageEditorData() {}

  // Title to use. If set it will be used without prompting the user.
  std::string title;
  std::string to_name;  // szDestination (to or sub name)

  bool need_title = true;
  uint8_t anonymous_flag = 0;   // an
  int msged_flags = MSGED_FLAG_NONE;      // used to be flags
  FsedFlags fsed_flags = FsedFlags::NOFSED;       // fsed
  bool silent_mode = false;     // Used for ASV and newemail emails.  No questions, etc.

  // legacy filename, used to see if it's email or not.
  std::string aux;

  // output onlu
  std::string text;
};

bool inmsg(MessageEditorData& data);

#endif  // __INCLUDED_BBS_INMSG_H__