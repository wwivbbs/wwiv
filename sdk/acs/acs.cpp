/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2020-2021, WWIV Software Services            */
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
#include "sdk/acs/acs.h"

#include "core/stl.h"
#include "sdk/acs/eval.h"
#include "sdk/acs/eval_error.h"
#include "common/value/uservalueprovider.h"

#include <string>
#include <tuple>
#include <vector>

using namespace wwiv::stl;
using namespace wwiv::sdk::acs;
using namespace wwiv::sdk::value;
using namespace wwiv::strings;

namespace wwiv::sdk::acs {

std::tuple<bool, std::vector<std::string>>
check_acs(const Config&, const std::string& expression,
  const std::vector<const ValueProvider*>& providers) {
  if (StringTrim(expression).empty()) {
    // Empty expression is always allowed.
    std::vector<std::string> debug_lines;
    return std::make_tuple(true, debug_lines);
  }

  Eval eval(expression);
  for (const auto* p : providers) {
    eval.add(p);
  }
  const auto result = eval.eval();
  return std::make_tuple(result, eval.debug_info());  
}

std::tuple<bool, std::string, std::vector<std::string>>
validate_acs(const std::string& expression, const std::vector<const ValueProvider*>& providers) {
  Eval eval(expression);
  for (const auto* p : providers) {
    eval.add(p);
  }

  try {
    eval.eval_throws();
    std::vector<std::string> debug_lines;
    return std::make_tuple(true, "", debug_lines);
  } catch (const eval_error& e) {
    return std::make_tuple(false, e.what(), eval.debug_info());
  }
}

} // namespace wwiv::sdk::acs
