/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*                Copyright (C)2020, WWIV Software Services               */
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

#include "core/log.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/user.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::core::parser;
using namespace wwiv::strings;

namespace wwiv::sdk::acs {

// TODO(rushfan): Move this to sdk, this is from bbs/arword.cpp
static std::string word_to_arstr(int ar) {
  if (!ar) {
    return {};
  }
  std::string arstr;
  for (int i = 0; i < 16; i++) {
    if ((1 << i) & ar) {
      arstr.push_back(static_cast<char>('A' + i));
    }
  }
  return arstr;
}

/** Shorthand to create an optional Value */
template <typename T> static std::optional<Value> val(T&& v) {
  return std::make_optional<Value>(std::forward<T>(v));
}

std::optional<Value> UserValueProvider::value(const std::string& name) {
  if (iequals(name, "sl")) {
    return val(user_->GetSl());
  } else if (iequals(name, "dsl")) {
    return val(user_->GetDsl());
  } else if (iequals(name, "age")) {
    return val(user_->age());
  } else if (iequals(name, "ar")) {
    return val(word_to_arstr(user_->GetAr()));
  } else if (iequals(name, "dar")) {
    return val(word_to_arstr(user_->GetDar()));
  } else if (iequals(name, "name")) {
    return val(user_->GetName());
  }
  return std::nullopt;
}

}