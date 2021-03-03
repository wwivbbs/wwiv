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
#include "common/macro_context.h"

#include "core/stl.h"
#include "core/strings.h"

using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::common {

template <typename K>
static bool in(K const& k, std::initializer_list<K> list) {
  for (const auto& l : list) {
    if (l == k) {
      return true;
    }
  }
  return false;
}

Interpreted MacroContext::interpret(std::string::const_iterator& it,
    std::string::const_iterator end) const {  // NOLINT(performance-unnecessary-value-param)
  Interpreted res{};
  const auto code = *it++;
  std::string text;
  text.push_back(code);
  switch (code) {
  case '[': { // movement
    while (it != end) {
      if (std::isdigit(*it) || *it == ';') {
        text.push_back(*it);
      } else if (in(*it, {'A', 'B', 'C', 'D', 'H', 'J', 'K'})) {
        text.push_back(*it++);
        return interpret_string(text);
      } else {
        return text;
      }
      ++it;
    }
    return text;
  }
  case '{': { // long form expression
    if (!context_ || !context_->mci_enabled()) {
      res.text = "|{";
      return res;
    }

    while (it != end) {
      const auto ch = *it++;
      text.push_back(ch);
      if (ch == '}') {
        // TODO(rushfan): Maybe here is where we should handle cases where the expressions
        // return Pipe codes (like {if "system.use_realnames", "|@N", "|@n"}
        return evaluate_expression(text);
      }
    }
    return text;
  }
  case '@': { // macro
    res.text = interpret_macro_char(*it++);
    return res;
  }
  case '|': { // pipe char
    res.text = "|";
    return res;
  }
  default:
    res.text = code;
    return res;
  }
}

}

