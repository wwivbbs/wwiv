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

#include "bbs/acs.h"
#include "bbs/application.h"
#include "bbs/bbsutl.h"
#include "bbs/sysoplog.h"
#include "core/datetime.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/config.h"
#include "sdk/msgapi/message_api.h"
#include "sdk/names.h"
#include "sdk/net/networks.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"

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

MessageBase::Poster BbsMessageBase::poster(int subnum) const {
  const auto& u = *app_.user();
  const auto& sl = app_.config()->sl(app_.sess().effective_sl());
  // As inmsg signs posts: with the system number of the sub's network (its
  // first), or of the current network for local subs.
  auto sysnum = app_.current_net().sysnum;
  const auto& subs = std::as_const(app_).subs();
  if (subnum >= 0 && subnum < subs.size()) {
    if (const auto& nets = subs.sub(subnum).nets; !nets.empty()) {
      const auto net_num = nets.front().net_num;
      if (net_num >= 0 && net_num < stl::size_int(app_.nets())) {
        sysnum = app_.nets()[net_num].sysnum;
      }
    }
  }
  Poster p;
  p.user_num = app_.sess().user_num();
  if (const auto* names = app_.names(); names) {
    p.name = names->UserName(p.user_num, sysnum);
  }
  if (p.name.empty()) {
    // As Names::UserName.
    p.name = fmt::format("{} #{}", ::properize(u.name()), p.user_num);
  }
  p.real_name = u.real_name();
  p.restrict_post = u.restrict_post();
  p.restrict_net = u.restrict_net();
  p.restrict_anony = u.restrict_anony();
  p.restrict_validate = u.restrict_validate();
  p.posts_today = u.posts_today();
  p.max_posts_today = sl.posts;
  p.sl_post_anony = (sl.ability & ability_post_anony) != 0;
  return p;
}

bool BbsMessageBase::can_post(int subnum) const {
  const auto& subs = std::as_const(app_).subs();
  if (subnum < 0 || subnum >= subs.size()) {
    return false;
  }
  return check_acs(subs.sub(subnum).post_acs);
}

// The BBS's post() also adds one to today's posts; for a script's post the
// SDK's AddMessage has done that.
void BbsMessageBase::posted(int subnum, const std::string& title) {
  auto& u = *app_.user();
  u.messages_posted(u.messages_posted() + 1);
  u.posts_today(u.posts_today() + 1);
  const auto& sub = std::as_const(app_).subs().sub(subnum);
  if (!sub.nets.empty()) {
    u.posts_net(u.posts_net() + 1);
  }
  // AddMessage already counted it in today's posts (msgposttoday), when it
  // gave the post its qscan number.
  app_.status_manager()->Run([](sdk::Status& s) { s.IncrementNumLocalPosts(); });
  sysoplog(fmt::format("+ '{}' posted on {} (from a script)", title, sub.name));
}

void BbsMessageBase::deleted(int subnum, const std::string& title, int owneruser, uint32_t daten) {
  // As remove_post: take back the credit for the user's own post, unless it
  // was made before this account was (a reused user number).
  if (owneruser == app_.sess().user_num()) {
    auto& u = *app_.user();
    if (core::date_to_daten(u.firston()) < daten && u.messages_posted() > 0) {
      u.messages_posted(u.messages_posted() - 1);
    }
  }
  const auto& sub = std::as_const(app_).subs().sub(subnum);
  sysoplog(fmt::format("- '{}' removed from {} (by a script)", title, sub.name));
}

} // namespace wwiv::bbs::basic
