/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*           Copyright (C)2020-2022, WWIV Software Services               */
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
#include "sdk/acs/expr.h"

#include "core/log.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::sdk::acs {

std::string AcsExpr::get() { 
  std::vector<std::string> expr;
  if (min_age_ > 0 && min_age_ != 255) {
    expr.emplace_back(fmt::format("user.age >= {}", min_age_));
  }
  if (max_age_ < 255 && max_age_ != 0) {
    expr.emplace_back(fmt::format("user.age <= {}", max_age_));
  }

  if (ar_ >= 'A' && ar_ <= 'P') {
    expr.emplace_back(fmt::format("user.ar == '{}'", ar_));
  }
  if (min_sl_ > 0) {
    expr.emplace_back(fmt::format("user.sl >= {}", min_sl_));
  }
  if (max_sl_ > 0 && max_sl_ != 255) {
    expr.emplace_back(fmt::format("user.sl <= {}", max_sl_));
  }

  if (dar_ >= 'A' && dar_ <= 'P') {
    expr.emplace_back(fmt::format("user.dar == '{}'", dar_));
  }
  if (min_dsl_ > 0) {
    expr.emplace_back(fmt::format("user.dsl >= {}", min_dsl_));
  }
  if (max_dsl_ > 0 && max_dsl_ != 255) {
    expr.emplace_back(fmt::format("user.dsl <= {}", max_dsl_));
  }

  if (regnum_) {
    expr.emplace_back("user.regnum == true");
  }

  if (sysop_) {
    expr.emplace_back("user.sysop == true");
  }

  if (cosysop_) {
    expr.emplace_back("user.cosysop == true");
  }

  bool first{true};
  std::ostringstream ss;
  for (const auto& e : expr) {
    if (!first) {
      ss << " && ";
    }
    ss << e;
    first = false;
  }
  return ss.str();
}

}