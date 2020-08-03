/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                    Copyright (C)2020, WWIV Software Services           */
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
#include "core/ip_address.h"

#include "core/strings.h"
#include "core/stl.h"
#include <array>
#include <iomanip>
#include <ios>
#include <random>
#include <sstream>
#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#include "WS2tcpip.h"
#include <ws2def.h>
#else
#include <arpa/inet.h>
#include <sys/types.h>
#endif // _WIN32


namespace wwiv::core {

using namespace wwiv::strings;

// Guarantee the size is 16 bytes and we're trivially copyable
// i.e. memcmp works on the class and also standard layout
// so that we can write to disk and back with write/read.
static_assert(sizeof(ip_address) == 16, "ip_address == 16");
static_assert(std::is_trivially_copyable<ip_address>::value == true,
              "ip_address !is_trivially_copyable");
static_assert(std::is_standard_layout<ip_address>::value == true,
              "ip_address !is_trivially_copyable");

std::ostream& operator<<(std::ostream& os, const ip_address& a) {
  os << a.to_string();
  return os;
}

ip_address::ip_address(char* data) { 
  memcpy(data_, data, 16); 
}

std::string ip_address::to_string() const {
  char buf[255];
  std::string s = inet_ntop(AF_INET6, &data_, buf, sizeof(buf));
  if (starts_with(s, "::ffff:") && s.find('.') != std::string::npos) {
    // Even though we store it IPv6 internally, display as IPv4
    return s.substr(7);
  }
  return StrCat("[", s, "]");
}

std::optional<ip_address> ip_address::from_string(const std::string& s) { 
  char d[16];
  auto addr{s};
  if (s.find(':') == std::string::npos) {
    //make this into a ipv6 address for an ipv4 address
    addr = wwiv::strings::StrCat("0:0:0:0:0:FFFF:", s);
  }
  const auto ret = inet_pton(AF_INET6, addr.c_str(), &d);
  if (ret != 1) {
    LOG(INFO) << "result code: " << ret;
    return std::nullopt;
  }
  return {ip_address(d)};
}

} // namespace wwiv::core 