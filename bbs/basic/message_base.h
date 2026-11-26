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

#include <chrono>
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
  // The post a reply is to, for quoting it and (on FTN subs) threading the
  // reply under it.
  struct ReplyTo {
    // Its author, as the user sees them.
    std::string name;
    // Its text as sdk/msgapi reads it (control lines such as MSGID, then the
    // text), and when it was posted.
    std::string text;
    uint32_t daten{0};
  };

  // A post the user wrote in the BBS's message editor.
  struct EditedPost {
    std::string title;
    // How the editor signed it (the user's name, or their real name).
    std::string from;
    // What follows the editor's author and date lines: its control lines
    // (PID, FidoAddr), RE:/BY: lines and the user's text.
    std::string text;
    // anony_sender etc., as the user chose (or the sub forces).
    uint8_t anony{0};
    // How long the user spent writing it.
    std::chrono::seconds writing_time{0};
  };
  // Opens the user's message editor, as the BBS's post() does, to write a
  // post in the sub. title is used as given; if it's empty the editor asks,
  // offering "Re: " + irt if irt isn't empty. to is who it's to ("" for
  // everyone; on FTN subs the editor asks). anonymous is the user's
  // anonymous setting in the sub (0, anony_enable_anony...), which decides
  // whether the editor asks. reply, if set, is the post this replies to:
  // its text can be quoted, and on FTN subs the reply gets its MSGID's
  // REPLY line. nullopt if the user aborts.
  [[nodiscard]] virtual std::optional<EditedPost>
  edit_post(int subnum, const std::string& title, const std::string& irt, const std::string& to,
            uint8_t anonymous, const std::optional<ReplyTo>& reply) = 0;
  // Called once the user's post titled title is added to the sub (through
  // sdk/msgapi, which counts it in today's posts): counts it for the user
  // and in the system's local posts, and logs it. writing_time is how long
  // the user spent in the editor (0 for a script's own text), given back
  // if the sysop has the BBS do that.
  virtual void posted(int subnum, const std::string& title, std::chrono::seconds writing_time) = 0;
  // Called once a post is deleted from the sub. If it was the user's own
  // (owneruser, posted at daten), takes back its credit; logs it.
  virtual void deleted(int subnum, const std::string& title, int owneruser, uint32_t daten) = 0;
};

} // namespace wwiv::bbs::basic

#endif
