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
#ifndef INCLUDED_BBS_BASIC_NEXT_BBS_MESSAGE_BASE_H
#define INCLUDED_BBS_BASIC_NEXT_BBS_MESSAGE_BASE_H

#include "bbs/basic/message_base.h"

class Application;

namespace wwiv::bbs::basic {

// The BBS's message base for wwiv.msg: the user's subs (usub), opened
// through the BBS's message APIs, with the same rights as the BBS's reader
// (read_post in bbs/read_message.cpp).
class BbsMessageBase final : public MessageBase {
public:
  explicit BbsMessageBase(Application& app) : app_(app) {}

  [[nodiscard]] std::vector<UserSub> user_subs() const override;
  [[nodiscard]] std::optional<UserSub> current_user_sub() const override;
  [[nodiscard]] const sdk::Subs& subs() const override;
  [[nodiscard]] std::unique_ptr<sdk::msgapi::MessageArea> open(int subnum) override;
  [[nodiscard]] bool is_cosysop(int subnum) const override;
  [[nodiscard]] bool can_read_anonymous(int subnum) const override;
  [[nodiscard]] Poster poster(int subnum) const override;
  [[nodiscard]] bool can_post(int subnum) const override;
  [[nodiscard]] std::optional<EditedPost> edit_post(int subnum, const std::string& title,
                                                    const std::string& irt, const std::string& to,
                                                    uint8_t anonymous) override;
  void posted(int subnum, const std::string& title) override;
  void deleted(int subnum, const std::string& title, int owneruser, uint32_t daten) override;

private:
  Application& app_;
};

} // namespace wwiv::bbs::basic

#endif
