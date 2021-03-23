/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2020-2021, WWIV Software Services             */
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
/*                                                                        */
/**************************************************************************/
#include "common/menus/menu_data_util.h"

#include "core/strings.h"

namespace wwiv::common::menus {


menu_data_and_options_t::menu_data_and_options_t(const std::string& raw) {
  const auto idx = raw.find(' ');
  if (idx == std::string::npos) {
    data_ = raw;
    return;
  }
  data_ = raw.substr(0, idx);
  const auto o = raw.substr(idx + 1);
  const auto v = strings::SplitString(o, " ");
  for (const auto& f : v) {
    if (const auto fidx = f.find('='); fidx != std::string::npos) {
      auto key = strings::ToStringLowerCase(strings::StringTrim(f.substr(0, fidx))); 
      auto val = strings::ToStringLowerCase(strings::StringTrim(f.substr(fidx + 1))); 
      opts_.emplace(key, val);
    }
  }
}

std::set<std::string> menu_data_and_options_t::opts(const std::string& key) const {
  std::set<std::string> r;
  for (auto [s, e] = opts_.equal_range(key); s != e; ++s) {
    r.insert(s->second);
  }
  return r;
}

const std::string& menu_data_and_options_t::data() const {
  return data_;
}

}
