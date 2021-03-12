/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2020-2021, WWIV Software Services           */
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
#ifndef INCLUDED_CORE_IP_ADDRESS_H
#define INCLUDED_CORE_IP_ADDRESS_H

#include <cstring>
#include <optional>
#include <string>

namespace wwiv::core {

class ip_address final {
public:
  explicit ip_address(char* data);
  ip_address() = default;
  ~ip_address() = default;


  [[nodiscard]] std::string to_string() const;

  /** True if this IP Address is an empty address (i.e. 0.0.0.0 or ::) */
  [[nodiscard]] bool empty() const;
  [[nodiscard]] static std::optional<ip_address> from_string(const std::string&);
  friend inline bool operator==(const ip_address& lhs, const ip_address& rhs);
  friend inline bool operator!=(const ip_address& lhs, const ip_address& rhs);
  friend std::ostream& operator<<(std::ostream& os, const ip_address& u);
  
private:
  char data_[16];
};

inline bool operator==(const ip_address& lhs, const ip_address& rhs) {
  return memcmp(lhs.data_, rhs.data_, sizeof(lhs.data_)) == 0;
}
inline bool operator!=(const ip_address& lhs, const ip_address& rhs) {
  return memcmp(lhs.data_, rhs.data_, sizeof(lhs.data_)) != 0;
}

static_assert(sizeof(wwiv::core::ip_address) == 16, "wwiv::core::ip_address == 16");

}

#endif
