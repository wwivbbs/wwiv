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
#include "bbs/basic/user_base.h"
#include "bbs/basic/next/wwiv_bindings.h"

#include "fmt/format.h"
#include "sdk/arword.h"
#include "sdk/user.h"
#include "wwivbasic/interpreter.h"
#include "wwivbasic/object.h"

#include <string>


namespace wwiv::bbs::basic {

using wwivbasic::BasicError;

namespace {

// The caller, as scripts see them. The names match the BBS's user.*
// expressions (common/value/uservalueprovider.cpp) where there is one.
struct ScriptUser {
  int number{0};
  std::string name;
  std::string real_name;
  int sl{0};
  int dsl{0};
  std::string ar;
  std::string dar;
  bool sysop{false};
  bool cosysop{false};
  int logons{0};
  int posts{0};
  int uploads{0};
  int downloads{0};
  daten_t last_on{0};
};

// Another user: only what any caller may see of them.
struct ScriptUserInfo {
  int number{0};
  std::string name;
  daten_t last_on{0};
};

ScriptUser to_script_user(const UserBase& users) {
  const auto& u = users.current();
  ScriptUser s;
  s.number = users.current_number();
  s.name = u.name();
  s.real_name = u.real_name_or_empty();
  s.sl = u.sl();
  s.dsl = u.dsl();
  s.ar = word_to_arstr_nopadding(u.ar_int(), "");
  s.dar = word_to_arstr_nopadding(u.dar_int(), "");
  s.sysop = u.sl() == 255;
  s.cosysop = users.is_cosysop();
  s.logons = u.logons();
  s.posts = u.messages_posted();
  s.uploads = u.uploaded();
  s.downloads = u.downloaded();
  s.last_on = u.last_daten();
  return s;
}

} // namespace

// The wwiv.user module (new engine only): the caller, and the public details
// of other users.
void RegisterWwivUser(wwivbasic::Interpreter& interp, const WwivBindings& b) {
  if (!b.users) {
    return;
  }
  auto* users = b.users;
  auto& m = interp.add_module("wwiv.user");
  auto& user_class = m.class_<ScriptUser>("user");
  auto& info_class = m.class_<ScriptUserInfo>("user_info");

  user_class.property("number", &ScriptUser::number)
      .property("name", &ScriptUser::name)
      .property("real_name", &ScriptUser::real_name)
      .property("sl", &ScriptUser::sl)
      .property("dsl", &ScriptUser::dsl)
      .property("ar", &ScriptUser::ar)
      .property("dar", &ScriptUser::dar)
      .property("sysop", &ScriptUser::sysop)
      .property("cosysop", &ScriptUser::cosysop)
      .property("logons", &ScriptUser::logons)
      .property("posts", &ScriptUser::posts)
      .property("uploads", &ScriptUser::uploads)
      .property("downloads", &ScriptUser::downloads)
      .property("last_on", &ScriptUser::last_on);

  info_class.property("number", &ScriptUserInfo::number)
      .property("name", &ScriptUserInfo::name)
      .property("last_on", &ScriptUserInfo::last_on);

  m.native_function("MODULE_NAME", []() { return std::string("wwiv.user"); });

  // CURRENT() is the caller, read when it's called.
  m.native_function("CURRENT", [users, &user_class]() {
    return user_class.wrap(to_script_user(*users));
  });

  // FIND(name) is the number of the user named name (case doesn't matter),
  // or 0 if there's none.
  m.native_function("FIND", [users](std::string name) { return users->find(name); });

  // USER(n) is the public details of user n; an error if there's no such
  // user, or it's deleted or inactive.
  m.native_function("USER", [users, &info_class](int number) {
    const auto u = users->read(number);
    if (!u) {
      throw BasicError(fmt::format("USER: there is no user {}", number));
    }
    return info_class.wrap(ScriptUserInfo{number, u->name(), u->last_daten()});
  });
}

} // namespace wwiv::bbs::basic
