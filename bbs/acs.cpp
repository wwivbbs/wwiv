/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                   Copyright (C)2020, WWIV Software Services            */
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
#include "bbs/acs.h"

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "common/input.h"
#include "core/stl.h"
#include "sdk/acs/eval.h"
#include "sdk/acs/eval_error.h"
#include "sdk/acs/uservalueprovider.h"
#include <memory>
#include <string>

using std::string;
using std::unique_ptr;
using namespace wwiv::stl;
using namespace wwiv::sdk::acs;
using namespace wwiv::strings;

namespace wwiv::bbs {

bool check_acs(const std::string& expression, acs_debug_t debug) {
  Eval eval(expression);

  eval.add("user", std::make_unique<UserValueProvider>(a()->user(), a()->sess().effective_sl()));

  const auto result = eval.eval();

  for (const auto& l : eval.debug_info()) {
    if (debug == acs_debug_t::local) {
      LOG(INFO) << l;
    } else if (debug == acs_debug_t::remote) {
      bout << l << wwiv::endl;
    }
  }

  return result;
}

bool validate_acs(const std::string& expression, acs_debug_t debug) {
  Eval eval(expression);

  eval.add("user", std::make_unique<UserValueProvider>(a()->user(), a()->sess().effective_sl()));

  try {
    eval.eval_throws();
    return true;
  } catch (const eval_error& e) {
    for (const auto& l : eval.debug_info()) {
      if (debug == acs_debug_t::local) {
        LOG(INFO) << l;
      } else if (debug == acs_debug_t::remote) {
        bout << l << wwiv::endl;
      }
    }
  }
  return false;
}

std::string input_acs(const std::string& orig_text, int max_length) {
  const auto s = bin.input_text(orig_text, max_length);

  if (!validate_acs(s, acs_debug_t::remote)) {
    bout.pausescr();
    return orig_text;
  }
  return s;
}

}
