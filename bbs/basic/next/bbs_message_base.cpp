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
#include "bbs/bbsutl.h"
#include "core/log.h"
#include "core/stl.h"
#include "sdk/msgapi/message_api.h"
#include "sdk/subxtr.h"

#include <exception>
#include <utility>

namespace wwiv::bbs::basic {

std::vector<MessageBase::UserSub> BbsMessageBase::user_subs() const {
  std::vector<UserSub> out;
  out.reserve(app_.usub.size());
  const auto num_subs = app_.subs().size();
  for (const auto& u : app_.usub) {
    // usub should only hold real subs; skip any that aren't, so every area a
    // script sees has a sub.
    if (u.subnum >= 0 && u.subnum < num_subs) {
      out.push_back(UserSub{u.subnum, u.keys});
    }
  }
  return out;
}

std::optional<MessageBase::UserSub> BbsMessageBase::current_user_sub() const {
  // current_user_sub_num() is a position in usub, which user_subs() filters,
  // so look the entry up here rather than in user_subs().
  const int n = app_.current_user_sub_num();
  if (n < 0 || n >= stl::size_int(app_.usub)) {
    return std::nullopt;
  }
  const auto& u = app_.usub[n];
  if (u.subnum < 0 || u.subnum >= app_.subs().size()) {
    return std::nullopt;
  }
  return UserSub{u.subnum, u.keys};
}

const sdk::Subs& BbsMessageBase::subs() const { return app_.subs(); }

std::unique_ptr<sdk::msgapi::MessageArea> BbsMessageBase::open(int subnum) {
  // The non-const Subs::sub() doesn't check its index.
  const auto& subs = std::as_const(app_).subs();
  if (subnum < 0 || subnum >= subs.size()) {
    return nullptr;
  }
  const auto& sub = subs.sub(subnum);
  // As Application::msgapi(): type 0 is read as type 2, the only type there is
  // an API for. (Asking it for another type is fatal in a debug build.)
  if (sub.storage_type != 0 && sub.storage_type != 2) {
    return nullptr;
  }
  try {
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

bool BbsMessageBase::is_cosysop(int subnum) const { return lcs(app_.context(), subnum); }

bool BbsMessageBase::can_read_anonymous(int subnum) const {
  return can_read_anonymous_posts(app_.context(), subnum);
}

} // namespace wwiv::bbs::basic
