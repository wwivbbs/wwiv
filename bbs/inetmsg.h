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
#ifndef __INCLUDED_BBS_INETMSG_H__
#define __INCLUDED_BBS_INETMSG_H__

#include <string>

void get_user_ppp_addr();
void send_inet_email();
bool check_inet_addr(const std::string& inetaddr);
void read_inet_addr(std::string& internet_address, int user_number);
void write_inet_addr(const std::string internet_address, int user_number);
void send_inst_sysstr(int whichinst, const char *send_string);

#endif  // __INCLUDED_BBS_INETMSG_H__