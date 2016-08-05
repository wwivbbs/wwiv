/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*              Copyright (C)2016 WWIV Software Services                  */
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
#ifndef __INCLUDED_NETWORKB_PACKETS_H__
#define __INCLUDED_NETWORKB_PACKETS_H__

#include <set>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/file.h"
#include "sdk/config.h"
#include "sdk/networks.h"
#include "sdk/net.h"

namespace wwiv {
namespace net {


bool write_packet(
  const std::string& filename,
  const net_networks_rec& net,
  const net_header_rec& nh, const std::set<uint16_t>& list, const std::string& text);

bool write_packet(
  const std::string& filename,
  const net_networks_rec& net,
  const net_header_rec& nh, const std::vector<uint16_t>& list, const std::string& text);

bool send_local_email(
  const net_networks_rec& network, net_header_rec& nh,
  const std::string& text, const std::string& byname, const std::string& title);

bool send_network_email(
  const std::string& filename,
  const net_networks_rec& network, net_header_rec& nh,
  std::vector<uint16_t> list, const std::string& text, const std::string& byname, const std::string& title);



}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_PACKETS_H__
