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
#ifndef INCLUDED_BBS_BBSUTL1_H
#define INCLUDED_BBS_BBSUTL1_H

#include "sdk/net/net.h"

#include <string>
#include <tuple>
#include <vector>

/**
 * Finds user_num and system number from emailAddress and sets the
 * network number as appropriate.
 *
 * @param email_address The text of the email address.
 * @return tuple of {un User Number, System Number}
 */
std::tuple<uint16_t, uint16_t> parse_email_info(const std::string& email_address);

/**
 * @verbatim
 * Creates string of form (#un | user_name) @sn[.network_name].
 * example: Rushfan @1.rushfan or #1 @1.rushnet or #1 @1
 */
std::string username_system_net_as_string(uint16_t un, const std::string& user_name,
                                          uint16_t sn, const std::string& network_name);
std::string username_system_net_as_string(uint16_t un, const std::string& user_name,
                                          uint16_t sn);

bool ValidateSysopPassword();
void hang_it_up();
bool play_sdf(const std::string& soundFileName, bool abortable);
std::string describe_area_code(int nAreaCode);
std::string describe_area_code_prefix(int nAreaCode, int town);

/**
 * Represents a network and also system name pair.
 */
struct NetworkAndName {
  NetworkAndName(wwiv::sdk::net::Network n, std::string s)
      : net(std::move(n)), system_name(std::move(s)), n_(n) {}
  wwiv::sdk::net::Network net;
  std::string system_name;
  const wwiv::sdk::net::Network& n_;
};

/**
 * Filters networks by viability.
 *
 * A viable network is:
 *   (1) has system_num reachable.
 *   (2) If the network is FTN, then tries to match zone from the network.
 *
 * Returns a vector of networks and the system names identified by the email address
 * either from system_num for WWIVnet networks, or from parsing the FidoNet Address
 * out of the email address for FTN networks.
 */
std::vector<NetworkAndName> filter_networks(std::vector<wwiv::sdk::net::Network>& nets,
                                            const std::string& email, int system_num);

#endif
