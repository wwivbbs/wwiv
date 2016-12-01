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
#ifndef __INCLUDED_BBS_READ_MESSAGE_H__
#define __INCLUDED_BBS_READ_MESSAGE_H__

#include <string>
#include <set>

#include "sdk/vardec.h"

enum class MessageFlags {
  NOT_VALIDATED,
  NOT_NETWORK_VALIDATED,
  FORCED,
  PERMANENT,
  LOCAL,
  PRIVATE,
  FTN,
  WWIVNET
};

struct Type2MessageData {
  std::string to_user_name;
  std::string from_user_name;
  std::string date;
  std::string message_text;
  std::string from_sys_name;
  std::string from_sys_loc;

  std::string title;
  std::set<MessageFlags> flags;
  int message_number = 0;
  int total_messages = 0;
  std::string message_area;
};

Type2MessageData read_type2_message(
  messagerec* msg, char an, bool readit, const char* file_name,
  int from_sys_num, int from_user);


enum class ReadMessageOption {
  NONE,
  JUMP_TO_MSG,
  NEXT_MSG,
  PREV_MSG,
  NEXT_SUB,
  PREV_SUB,
  COMMAND
};

struct ReadMessageResult {
  ReadMessageOption option = ReadMessageOption::NONE;
  char command = 0;
  std::string data;
};

ReadMessageResult display_type2_message(Type2MessageData& msg, char an, bool* next);

ReadMessageResult read_post(int n, bool *next, int *val);

#endif  // __INCLUDED_BBS_READ_MESSAGE_H__