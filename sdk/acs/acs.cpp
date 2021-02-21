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
#include "sdk/acs/uservalueprovider.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

using std::string;
using std::unique_ptr;
using namespace wwiv::stl;
using namespace wwiv::sdk::acs;
using namespace wwiv::strings;

namespace wwiv::sdk::acs {

static std::unique_ptr<Eval> make_eval(const Config& config, const User& user, int eff_sl,
                                       const std::string& expression) {
  auto eval = std::make_unique<Eval>(expression);

  const auto& eslrec = config.sl(eff_sl);
  eval->add("user", std::make_unique<UserValueProvider>(config, user, eff_sl, eslrec));
  return eval;
}

std::tuple<bool, std::vector<std::string>> check_acs(const Config& config, const User& user, int eff_sl,
                                                     const std::string& expression,
                                                     acs_debug_t debug) {
  if (StringTrim(expression).empty()) {
    // Empty expression is always allowed.
    std::vector<std::string> debug_lines;
    return std::make_tuple(true, debug_lines);
  }

  auto eval = make_eval(config, user, eff_sl, expression);
  const auto result = eval->eval();
  return std::make_tuple(result, eval->debug_info());
}

std::tuple<bool, std::string, std::vector<std::string>>
validate_acs(const Config& config, const User& user, int eff_sl, const std::string& expression) {
  auto eval = make_eval(config, user, eff_sl, expression);

  try {
    eval->eval_throws();
    std::vector<std::string> debug_lines;
    return std::make_tuple(true, "", debug_lines);
  } catch (const eval_error& e) {
    return std::make_tuple(false, e.what(), eval->debug_info());
  }
}

} // namespace wwiv::sdk::acs
