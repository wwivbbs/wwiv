/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2020-2022, WWIV Software Services           */
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
#ifndef INCLUDED_CORE_UUID_H
#define INCLUDED_CORE_UUID_H

#include <array>
#include <optional>
#include <random>
#include <string>

namespace wwiv::core {

class uuid_t final {
public:
  uuid_t();
  ~uuid_t() = default;
  explicit uuid_t(std::array<uint8_t, 16> s);
  uuid_t(const uuid_t&);
  uuid_t& operator=(const uuid_t&);

  [[nodiscard]] bool empty() const { return empty_; }
  [[nodiscard]] std::string to_string() const;
  [[nodiscard]] static std::optional<uuid_t> from_string(const std::string&);
  [[nodiscard]] int version() const;
  [[nodiscard]] int variant() const;
  [[nodiscard]] std::array<uint8_t, 16> bytes() const { return bytes_; }
  friend inline bool operator==(const uuid_t& lhs, const uuid_t& rhs);
  friend inline bool operator!=(const uuid_t& lhs, const uuid_t& rhs);
  friend std::ostream& operator<<(std::ostream& os, const uuid_t& u);

  
private:
  std::array<uint8_t, 16> bytes_{};
  bool empty_{true};
};

inline bool operator==(const uuid_t& lhs, const uuid_t& rhs) {
  return lhs.bytes_ == rhs.bytes_;
}
inline bool operator!=(const uuid_t& lhs, const uuid_t& rhs) {
  return lhs.bytes_ != rhs.bytes_;
}

class uuid_generator {
public:
  explicit uuid_generator(std::random_device& rd) :rd_(rd) {}
  [[nodiscard]] uuid_t generate();
private:
  std::uniform_int_distribution<uint32_t>  distribution_;
  std::random_device& rd_;
};

}


#endif
