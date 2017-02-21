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
#ifndef __INCLUDED_BBS_NETSUP_H__
#define __INCLUDED_BBS_NETSUP_H__

#include <chrono>
#include <cstdint>
#include <string>
#include "sdk/net.h"

void cleanup_net();
void do_callout(uint16_t sn);
void print_pending_list();
void gate_msg(
  net_header_rec* nh, char *messageText, int net_number,
  const std::string& author_name, std::vector<uint16_t> list,
  int nFromNetworkNumber);
void force_callout();
void run_exp();
bool attempt_callout();

std::chrono::steady_clock::time_point last_network_attempt();


#endif  // __INCLUDED_BBS_NETSUP_H__
