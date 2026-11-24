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
#include "bbs/basic/next/bbs_message_base.h"

#include "bbs/application.h"
#include "common/context.h"
#include "core/log.h"
#include "core/stl.h"
#include "sdk/config.h"
#include "sdk/msgapi/message_api.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"

#include <exception>

namespace wwiv::bbs::basic {

std::vector<MessageBase::UserSub> BbsMessageBase::user_subs() const {
  std::vector<UserSub> out;
  out.reserve(app_.usub.size());
  for (const auto& u : app_.usub) {
    out.push_back(UserSub{u.subnum, u.keys});
  }
  return out;
}

std::optional<int> BbsMessageBase::current_user_sub() const {
  const int n = app_.current_user_sub_num();
  if (n < 0 || n >= stl::size_int(app_.usub)) {
    return std::nullopt;
  }
  return n;
}

const sdk::Subs& BbsMessageBase::subs() const { return app_.subs(); }

std::unique_ptr<sdk::msgapi::MessageArea> BbsMessageBase::open(int subnum) {
  try {
    const auto& sub = app_.subs().sub(subnum);
    auto* api = app_.msgapi(sub.storage_type);
    if (!api || !api->Exist(sub)) {
      return nullptr;
    }
    return api->Open(sub, subnum);
  } catch (const std::exception& e) {
    LOG(ERROR) << "wwiv.msg: unable to open sub #" << subnum << ": " << e.what();
    return nullptr;
  }
}

// As lcs() in bbsutl.cpp, but for the given sub rather than the current one.
bool BbsMessageBase::is_cosysop(int subnum) const {
  const auto sl = app_.sess().effective_sl();
  if (sl == 255) {
    return true;
  }
  const auto ability = app_.config()->sl(sl).ability;
  if (ability & ability_cosysop) {
    return true;
  }
  if (ability & ability_limited_cosysop) {
    // A limited co-sysop's sub is the first qscan entry; 999 means all subs.
    const auto* qsc = app_.sess().qsc;
    return qsc && (*qsc == 999 || *qsc == static_cast<uint32_t>(subnum));
  }
  return false;
}

// As read_post in read_message.cpp.
bool BbsMessageBase::can_read_anonymous(int subnum) const {
  return is_cosysop(subnum) ||
         (app_.config()->sl(app_.sess().effective_sl()).ability & ability_read_post_anony);
}

} // namespace wwiv::bbs::basic
