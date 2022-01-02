/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2020-2022, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_KEY_H
#define INCLUDED_SDK_KEY_H

#include <iostream>

namespace wwiv::sdk {

class key_t final {
public:
  key_t() = default;
  explicit key_t(char key) : key_(key) {}

  void key(char k) noexcept { key_ = k; }
  [[nodiscard]] char key() const noexcept { return key_; }

  char key_;
};

inline bool operator< (const key_t &c, const key_t & c2) { return c.key_ < c2.key_; }
inline bool operator< (const char &c, const key_t & c2) { return c < c2.key_; }
inline bool operator< (const key_t &c, const char & c2) { return c.key_ < c2; }
inline bool operator== (const key_t &c, const key_t & c2) { return c.key_ == c2.key_; }
inline bool operator== (const char &c, const key_t & c2) { return c == c2.key_; }
inline bool operator== (const key_t &c, const char & c2) { return c.key_ == c2; }

inline std::ostream& operator<<(std::ostream& os, const key_t& c) {
  os << c.key();
  return os;
}


}

#endif
