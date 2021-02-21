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
#include "sdk/acs/uservalueprovider.h"

#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/acs/eval_error.h"
#include "sdk/user.h"
#include <optional>
#include <string>

using namespace wwiv::core;
using namespace parser;
using namespace wwiv::strings;

namespace wwiv::sdk::acs {


/** Shorthand to create an optional Value */
template <typename T> static std::optional<Value> val(T&& v) {
  return std::make_optional<Value>(std::forward<T>(v));
}

std::optional<Value> UserValueProvider::value(const std::string& name) {
  if (iequals(name, "sl")) {
    return val(user_->sl());
  }
  if (iequals(name, "dsl")) {
    return val(user_->dsl());
  }
  if (iequals(name, "age")) {
    return val(user_->age());
  }
  if (iequals(name, "ar")) {
    return val(Ar(user_->ar_int(), true));
  }
  if (iequals(name, "dar")) {
    return val(Ar(user_->dar_int(), true));
  }
  if (iequals(name, "name")) {
    return val(user_->name());
  }
  if (iequals(name, "regnum")) {
    return val(user_->wwiv_regnum() != 0);
  }
  if (iequals(name, "sysop")) {
    return val(user_->sl() == 255);
  }
  if (iequals(name, "cosysop")) {
    const auto so = user_->sl() == 255;
    const auto cs = (sl_.ability & ability_cosysop) != 0;
    return val(so || cs);
  }
  if (iequals(name, "guest")) {
    return val(user_->guest_user());
  }
  throw eval_error(fmt::format("No user attribute named 'user.{}' exists.", name));
}

}