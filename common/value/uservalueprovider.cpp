/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*           Copyright (C)2020-2021, WWIV Software Services               */
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
#include "common/value/uservalueprovider.h"

#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/user.h"
#include "sdk/acs/eval_error.h"

#include <optional>
#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::acs;
using namespace wwiv::sdk::value;
using namespace parser;
using namespace wwiv::strings;

namespace wwiv::common::value {


/** Shorthand to create an optional Value */
template <typename T> static std::optional<sdk::value::Value> val(T&& v) {
  return std::make_optional<Value>(std::forward<T>(v));
}

UserValueProvider::UserValueProvider(Context& c, int effective_sl)
  : UserValueProvider(c.config(), c.u(), effective_sl, c.config().sl(effective_sl)) {
  fns_.try_emplace("editorname", [&]() {
    const auto editor_num = user_.default_editor();
    if (editor_num == 0xff) {
      return val("Full Screen");
    }
    if (editor_num > 0 && editor_num <= stl::size_int(editors_)) {
      return val(editors_[editor_num - 1].description);
    }
    return val("Line");
  });
}

UserValueProvider::UserValueProvider(const sdk::Config& config, const sdk::User& user, int effective_sl,
                                     slrec sl)
  : ValueProvider("user"), config_(config), user_(user), effective_sl_(effective_sl), sl_(sl) {
  fns_.try_emplace("sl", [&]() { return val(user_.sl()); });
  fns_.try_emplace("dsl", [&]() { return val(user_.dsl()); });
  fns_.try_emplace("age", [&]() { return val(user_.age()); });
  fns_.try_emplace("ar", [&]() { return val(Ar(user_.ar_int(), true)); });
  fns_.try_emplace("dar", [&]() { return val(Ar(user_.dar_int(), true)); });
  fns_.try_emplace("name", [&]() { return val(user_.name()); });
  fns_.try_emplace("real_name", [&]() { return val(user_.real_name_or_empty()); });
  fns_.try_emplace("regnum", [&]() { return val(static_cast<int>(user_.wwiv_regnum())); });
  fns_.try_emplace("registered", [&]() { return val(user_.wwiv_regnum() != 0); });
  fns_.try_emplace("sysop", [&]() { return val(user_.sl() == 255); });
  fns_.try_emplace("cosysop", [&]() {
    const auto so = user_.sl() == 255;
    const auto cs = (sl_.ability & ability_cosysop) != 0;
    return val(so || cs);
  });
  fns_.try_emplace("guest", [&]() { return val(user_.guest_user()); });
  fns_.try_emplace("validated", [&]() { return val(effective_sl_ >= config_.validated_sl()); });
  fns_.try_emplace("screenlines", [&]() { return val(user_.screen_lines()); });
  fns_.try_emplace("screenwidth", [&]() { return val(user_.screen_width()); });
  fns_.try_emplace("ansi", [&]() { return val(user_.ansi()); });
  fns_.try_emplace("color", [&]() { return val(user_.color()); });
  fns_.try_emplace("ansistr", [&]() { return val(
    user_.ansi() ? (user_.color() ? "Color" : "Monochrome") : "No ANSI");
  });
  fns_.try_emplace("pause", [&]() { return val(user_.pause()); });
  fns_.try_emplace("mailbox_state", [&]() { return val(user_.mailbox_state()); });
  fns_.try_emplace("extcolors", [&]() { return val(user_.extra_color()); });
  fns_.try_emplace("optional_lines", [&]() { return val(user_.optional_val()); });
  fns_.try_emplace("conferencing", [&]() { return val(user_.use_conference()); });
  fns_.try_emplace("fs_reader", [&]()
  {
    return val(user_.has_flag(User::fullScreenReader));
  });
  fns_.try_emplace("email", [&]() { return val(user_.email_address()); });
  fns_.try_emplace("ignore_msgs", [&]() { return val(user_.ignore_msgs()); });
  fns_.try_emplace("clear_screen", [&]() { return val(user_.clear_screen()); });
  fns_.try_emplace("auto_quote", [&]() { return val(user_.auto_quote()); });
  fns_.try_emplace("protocol", [&]() { return val(user_.default_protocol()); });
  fns_.try_emplace("callsign", [&]() { return val(user_.callsign()); });
  fns_.try_emplace("street", [&]() { return val(user_.street()); });
  fns_.try_emplace("city", [&]() { return val(user_.city()); });
  fns_.try_emplace("state", [&]() { return val(user_.state()); });
  fns_.try_emplace("zip_code", [&]() { return val(user_.zip_code()); });
  fns_.try_emplace("last_ipaddress", [&]() { return val(user_.last_address().to_string()); });
  fns_.try_emplace("last_bps", [&]() { return val(user_.last_bps()); });
  fns_.try_emplace("laston", [&]() { return val(user_.laston()); });
  fns_.try_emplace("voice_phone", [&]() { return val(user_.voice_phone()); });
  fns_.try_emplace("data_phone", [&]() { return val(user_.data_phone()); });
  fns_.try_emplace("gender", [&]() { return val(user_.gender()); });
  fns_.try_emplace("menuset", [&]() { return val(user_.menu_set()); });
  fns_.try_emplace("birthday_mmddyy", [&]() { return val(user_.birthday_mmddyy()); });
  fns_.try_emplace("email_waiting", [&]() { return val(user_.email_waiting()); });
  fns_.try_emplace("messages_posted", [&]() { return val(user_.messages_posted()); });
  fns_.try_emplace("posts_today", [&]() { return val(user_.posts_today()); });
  fns_.try_emplace("posts_net", [&]() { return val(user_.posts_net()); });
  fns_.try_emplace("messages_read", [&]() { return val(user_.messages_read()); });
  fns_.try_emplace("email_today", [&]() { return val(user_.email_today()); });
  fns_.try_emplace("email_sent_local", [&]() { return val(user_.email_sent()); });
  fns_.try_emplace("feedback_sent", [&]() { return val(user_.feedback_sent()); });
  fns_.try_emplace("email_sent_net", [&]() { return val(user_.email_net()); });
  fns_.try_emplace("chains_run", [&]() { return val(user_.chains_run()); });
  fns_.try_emplace("uploaded", [&]() { return val(user_.uploaded()); });
  fns_.try_emplace("uk", [&]() { return val(user_.uk()); });
  fns_.try_emplace("downloaded", [&]() { return val(user_.downloaded()); });
  fns_.try_emplace("dk", [&]() { return val(user_.dk()); });
  fns_.try_emplace("show_controlcodes", [&]() { return val(user_.has_flag(User::msg_show_controlcodes)); });
  fns_.try_emplace("twentyfour_clock", [&]() { return val(user_.twentyfour_clock()); });

}

UserValueProvider::UserValueProvider(Context& c)
    : UserValueProvider(c, c.session_context().effective_sl() != 0
                               ? c.session_context().effective_sl()
                               : c.u().sl()) {}


std::optional<Value> UserValueProvider::value(const std::string& name) const {
  if (const auto it = fns_.find(name); it != std::end(fns_)) {
    return it->second();
  }
  throw eval_error(fmt::format("No user attribute named 'user.{}' exists.", name));
}

}