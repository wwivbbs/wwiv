/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2026, WWIV Software Services                  */
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
#ifndef INCLUDED_BBS_BASIC_USER_BASE_H
#define INCLUDED_BBS_BASIC_USER_BASE_H

#include <optional>
#include <string>

namespace wwiv::sdk {
class User;
}

namespace wwiv::bbs::basic {

// What wwiv.user reads: the caller and the BBS's users. BbsUserBase is the
// BBS's; tests use their own.
class UserBase {
public:
  virtual ~UserBase() = default;

  // The caller's user record and number.
  [[nodiscard]] virtual const sdk::User& current() const = 0;
  [[nodiscard]] virtual int current_number() const = 0;
  // Is the caller a co-sysop, as the BBS's user.cosysop expression says: SL
  // 255, or an effective SL with the co-sysop ability?
  [[nodiscard]] virtual bool is_cosysop() const = 0;
  // User user_number, or nothing if there's no such user or it's deleted or
  // inactive.
  [[nodiscard]] virtual std::optional<sdk::User> read(int user_number) const = 0;
  // The number of the user named name (case doesn't matter), or 0.
  [[nodiscard]] virtual int find(const std::string& name) const = 0;
};

} // namespace wwiv::bbs::basic

#endif
