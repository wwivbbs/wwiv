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
#ifndef INCLUDED_BBS_BASIC_MESSAGE_BASE_H
#define INCLUDED_BBS_BASIC_MESSAGE_BASE_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wwiv::sdk {
class Subs;
namespace msgapi {
class MessageArea;
}
} // namespace wwiv::sdk

namespace wwiv::bbs::basic {

// What wwiv.msg reads: the message areas the current user can read and the
// user's rights in them. BbsMessageBase is the BBS's; tests use their own.
class MessageBase {
public:
  // A message area the user can read: an index into subs(), and the key the
  // user types to choose it.
  struct UserSub {
    int subnum;
    std::string key;
  };

  virtual ~MessageBase() = default;

  // The areas the user can read, in the order the BBS lists them (the
  // current conference's subs that pass their read ACS).
  [[nodiscard]] virtual std::vector<UserSub> user_subs() const = 0;
  // The index into user_subs() of the area the user is in, if any.
  [[nodiscard]] virtual std::optional<int> current_user_sub() const = 0;
  [[nodiscard]] virtual const sdk::Subs& subs() const = 0;
  // Opens a sub's messages, or returns nullptr if it can't be opened.
  [[nodiscard]] virtual std::unique_ptr<sdk::msgapi::MessageArea> open(int subnum) = 0;
  // May the user see unvalidated and deleted posts in the sub (lcs())?
  [[nodiscard]] virtual bool is_cosysop(int subnum) const = 0;
  // May the user see who wrote anonymous posts in the sub?
  [[nodiscard]] virtual bool can_read_anonymous(int subnum) const = 0;
};

} // namespace wwiv::bbs::basic

#endif
