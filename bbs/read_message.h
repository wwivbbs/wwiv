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
#ifndef INCLUDED_BBS_READ_MESSAGE_H
#define INCLUDED_BBS_READ_MESSAGE_H

#include <set>
#include <string>

struct messagerec;

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
  // Parsed out message text, no header information.
  std::string message_text;
  // Raw text as it came from readfile
  std::string raw_message_text;
  std::string from_sys_name;
  std::string from_sys_loc;

  std::string title;
  std::set<MessageFlags> flags;
  int message_number{0};
  int total_messages{0};
  std::string message_area;

  // mailrec.anony flag.
  uint8_t message_anony{0};
  // Any special flags from the subboard. So far only anony.no_fulscreen
  uint8_t subboard_flags{0};
  bool use_msg_command_handler{true};
  bool email{false};
};

Type2MessageData read_type2_message(messagerec* msg, uint8_t an, bool readit,
                                    const std::string& file_name, int from_sys_num, int from_user);


enum class ReadMessageOption {
  NONE,
  // Opens the Jump to Message Dialog
  JUMP_TO_MSG,
  NEXT_MSG,
  PREV_MSG,
  LIST_TITLES,
  COMMAND,
  READ_MESSAGE
};

struct ReadMessageResult {
  ReadMessageOption option = ReadMessageOption::NONE;
  char command = 0;
  std::string data;
  int lines_start = 0;
  int lines_end = 20;
  // only used with GOTO_MSG
  int msgnum{0};
};

ReadMessageResult display_type2_message(int& msgnum, Type2MessageData& msg, bool* next);

ReadMessageResult read_post(int& msgnum, bool *next, int *val);

#endif
