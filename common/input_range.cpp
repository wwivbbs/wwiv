/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                  Copyright (C)2020, WWIV Software Services             */
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
#include "common/input_range.h"

#include "core/stl.h"
#include "core/strings.h"

using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::common {

std::vector<std::string> split_distinct_ranges(std::string s) {
  s.erase(std::remove(std::begin(s), std::end(s), ' '), std::end(s));
  return SplitString(s, ",", true);
}

bool is_consecutive_range(const std::string& text) {
  if (text.empty()) {
    return false;
  }

  const auto idx = text.find('-');
  return idx != std::string::npos && idx != 0 && idx != text.size() - 1;
}

} // namespace wwiv::common
