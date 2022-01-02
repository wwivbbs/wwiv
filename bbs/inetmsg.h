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
/**************************************************************************/
#ifndef INCLUDED_BBS_INETMSG_H
#define INCLUDED_BBS_INETMSG_H

#include <string>

/**
 * Minimal checks for email address validity. 
 */
bool check_inet_addr(const std::string& inetaddr);

/**
 * Gets the email address from the user at user_number or an empty string
 * if the email address does not exist or is invalid.
 */
std::string read_inet_addr(int user_number);
void send_inst_sysstr(int whichinst, const std::string& send_string);

#endif  // INCLUDED_BBS_INETMSG_H