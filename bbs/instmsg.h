/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
/*                                                                        */
/**************************************************************************/
#ifndef INCLUDED_INSTMSG_H
#define INCLUDED_INSTMSG_H

#include <chrono>
#include <optional>
#include <string>
#include "sdk/instance.h"

constexpr int INST_MSG_STRING = 1;  // A string to print out to the user
constexpr int UNUSED_INST_MSG_SHUTDOWN = 2;  // Hangs up, ends BBS execution
constexpr int INST_MSG_SYSMSG = 3;  // Message from the system, not a user
constexpr int UNUSED_INST_MSG_CLEANNET = 4;  // should call cleanup_net
constexpr int INST_MSG_CHAT = 6;  // External chat request

/****************************************************************************/




void send_inst_str(int whichinst, const std::string& send_string);
void broadcast(const std::string& message);
void process_inst_msgs();
int  num_instances();
std::optional<int> user_online(int user_number);
void write_inst(int loc, int subloc = 0, int flags = wwiv::sdk::INST_FLAGS_NONE);
bool inst_msg_waiting();
std::chrono::milliseconds setiia(std::chrono::milliseconds poll_time);
void toggle_invis();
void toggle_avail();
bool is_chat_invis();

#endif

