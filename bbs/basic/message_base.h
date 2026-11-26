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

#include <cstdint>
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

// What wwiv.msg reads and writes: the message areas the current user can
// read, and the user's rights in them. BbsMessageBase is the BBS's; tests use
// their own.
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
  // The area the user is in (one of user_subs()), if any.
  [[nodiscard]] virtual std::optional<UserSub> current_user_sub() const = 0;
  [[nodiscard]] virtual const sdk::Subs& subs() const = 0;
  // Opens a sub's messages, or returns nullptr if it can't be opened.
  [[nodiscard]] virtual std::unique_ptr<sdk::msgapi::MessageArea> open(int subnum) = 0;
  // May the user see unvalidated and deleted posts in the sub (lcs())?
  [[nodiscard]] virtual bool is_cosysop(int subnum) const = 0;
  // May the user see who wrote anonymous posts in the sub?
  [[nodiscard]] virtual bool can_read_anonymous(int subnum) const = 0;

  // The user, as the BBS's post() sees them.
  struct Poster {
    int user_num{0};
    // How the user's posts in the sub are signed ("NAME #1 @1234"), and the
    // user's real name, for subs that sign posts with it.
    std::string name;
    std::string real_name;
    // The user's restrictions: may not post at all, may not post on
    // networked subs, may not post anonymously, posts need validating.
    bool restrict_post{false};
    bool restrict_net{false};
    bool restrict_anony{false};
    bool restrict_validate{false};
    // Posts today, and how many the user's SL allows a day.
    int posts_today{0};
    int max_posts_today{0};
    // The user's SL may post anonymously in subs that don't say otherwise.
    bool sl_post_anony{false};
  };
  [[nodiscard]] virtual Poster poster(int subnum) const = 0;
  // Does the user pass the sub's post ACS?
  [[nodiscard]] virtual bool can_post(int subnum) const = 0;
  // Called once the user's post titled title is added to the sub: counts it
  // for the user and the system, and logs it.
  virtual void posted(int subnum, const std::string& title) = 0;
  // Called once a post is deleted from the sub. If it was the user's own
  // (owneruser, posted at daten), takes back its credit; logs it.
  virtual void deleted(int subnum, const std::string& title, int owneruser, uint32_t daten) = 0;
};

} // namespace wwiv::bbs::basic

#endif
