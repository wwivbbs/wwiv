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
#include "bbs/basic/next/bbs_user_base.h"

#include "bbs/application.h"
#include "common/context.h"
#include "sdk/config.h"
#include "sdk/names.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"

namespace wwiv::bbs::basic {

const sdk::User& BbsUserBase::current() const { return *app_.user(); }

int BbsUserBase::current_number() const { return app_.sess().user_num(); }

// As the user.cosysop expression (common/value/uservalueprovider.cpp),
// including its use of the stored SL before the session's SL is set.
bool BbsUserBase::is_cosysop() const {
  const auto& u = *app_.user();
  if (u.sl() == 255) {
    return true;
  }
  const auto effective_sl = app_.sess().effective_sl();
  const auto sl = effective_sl != 0 ? effective_sl : u.sl();
  return (app_.config()->sl(sl).ability & ability_cosysop) != 0;
}

// Deleted and inactive users aren't shown, as names.lst (rebuilt from active
// users) doesn't list them. Record 0 isn't a user; readuser already returns
// nothing past the end.
std::optional<sdk::User> BbsUserBase::read(int user_number) const {
  const auto* users = app_.users();
  if (!users || user_number < 1) {
    return std::nullopt;
  }
  return users->readuser(user_number, sdk::UserManager::mask::active);
}

int BbsUserBase::find(const std::string& name) const {
  auto* names = app_.names();
  return names ? names->FindUser(name) : 0;
}

} // namespace wwiv::bbs::basic
