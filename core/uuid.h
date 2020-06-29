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
#ifndef __INCLUDED_WWIV_CORE_UUID_H__
#define __INCLUDED_WWIV_CORE_UUID_H__

#include <array>
#include <optional>
#include <random>
#include <string>

class uuid final {
public:
  uuid();
  ~uuid() = default;
  explicit uuid(std::array<uint8_t, 16> s);
  uuid(const uuid&);
  uuid& operator=(const uuid&);

  [[nodiscard]] bool empty() const { return empty_; }
  [[nodiscard]] std::string to_string() const;
  [[nodiscard]] static std::optional<uuid> from_string(const std::string&);
  [[nodiscard]] int version() const;
  [[nodiscard]] int variant() const;
  [[nodiscard]] std::array<uint8_t, 16> bytes() const { return bytes_; }
  friend inline bool operator==(const uuid& lhs, const uuid& rhs);
  friend inline bool operator!=(const uuid& lhs, const uuid& rhs);

private:
  std::array<uint8_t, 16> bytes_{};
  bool empty_{true};
};

inline bool operator==(const uuid& lhs, const uuid& rhs) {
  return lhs.bytes_ == rhs.bytes_;
}
inline bool operator!=(const uuid& lhs, const uuid& rhs) {
  return lhs.bytes_ != rhs.bytes_;
}

class uuid_generator {
public:
  explicit uuid_generator(std::random_device& rd) :rd_(rd) {}
  [[nodiscard]] uuid generate() const;
private:
  std::uniform_int_distribution<uint32_t>  distribution_;
  std::random_device& rd_;
};


#endif