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
#ifndef __INCLUDED_NETORKB_NET_UTIL_H__
#define __INCLUDED_NETORKB_NET_UTIL_H__

#include <set>
#include <string>
#include <vector>
#include "sdk/networks.h"
#include "sdk/net.h"

namespace wwiv {
namespace net {

void rename_pend(const std::string& directory, const std::string& filename);

bool write_packet(
  const std::string& filename,
  const net_networks_rec& net,
  const net_header_rec& nh, const std::vector<uint16_t>& list, const std::string& text);

bool send_network(
  const std::string& filename,
  const net_networks_rec& network, net_header_rec& nh,
  std::vector<uint16_t> list, const std::string& text, const std::string& byname, const std::string& title);

bool send_local(
  const net_networks_rec& network, net_header_rec& nh,
  const std::string& text, const std::string& byname, const std::string& title);

std::string main_type_name(int typ);
std::string net_info_minor_type_name(int typ);

template <typename C, typename I>
static std::string get_message_field(C& c, I& iter, std::set<char> stop, std::size_t max) {
  const auto begin = iter;

  int count = 0;
  while (stop.find(*iter) == std::end(stop) && ++count < 80 && iter != c.end()) {
    iter++;
  }
  string result(begin, iter);

  // Stop over stop chars
  while (stop.find(*iter) != std::end(stop)
    && iter != std::end(c)) {
    iter++;
  }

  return result;
}




}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETORKB_NET_UTIL_H__
