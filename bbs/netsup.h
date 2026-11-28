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
#ifndef INCLUDED_BBS_NETSUP_H
#define INCLUDED_BBS_NETSUP_H

#include <cstdint>
#include <string>
#include <vector>

struct net_header_rec;
namespace wwiv::sdk::net {
class Network;
}

void cleanup_net();

/**
 * Builds the "from" name line written when gating a message from from_net to
 * the network whose system number is to_sysnum, and updates nh.fromsys and
 * nh.fromuser to match. nm is the message's original name line, without its
 * "\r\n". from_this_system is true when the message originated on this system;
 * from_net is then not used. The result ends in "\r\n", except for a name
 * from an internet or news gateway.
 */
std::string gate_msg_from_name(net_header_rec& nh, std::string nm, bool from_this_system,
                               const wwiv::sdk::net::Network& from_net, uint16_t to_sysnum);
void print_pending_list();
void gate_msg(net_header_rec* nh, char* messageText, int net_number, const std::string& author_name,
              std::vector<uint16_t> list, int nFromNetworkNumber);
void force_callout();

#endif
