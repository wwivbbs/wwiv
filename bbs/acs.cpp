/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2020-2022, WWIV Software Services            */
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

#include "sdk/acs/acs.h"
#include "bbs/application.h"
#include "bbs/bbs.h"
#include "common/input.h"
#include "common/value/bbsvalueprovider.h"
#include "common/value/uservalueprovider.h"
#include "core/stl.h"
#include <string>

using namespace wwiv::common::value;
using namespace wwiv::stl;
using namespace wwiv::sdk::acs;
using namespace wwiv::sdk::value;
using namespace wwiv::strings;

namespace wwiv::bbs {

bool check_acs(const std::string& expression, acs_debug_t debug) {
  if (StringTrim(expression).empty()) {
    // Empty expression is always allowed.
    return true;
  }

  const UserValueProvider user_provider(a()->context());
  const BbsValueProvider bbs_provider(*a()->config(), a()->sess());

  auto [result, debug_info] =
      sdk::acs::check_acs(*a()->config(), expression, &user_provider, &bbs_provider);
  for (const auto& l : debug_info) {
    if (debug == acs_debug_t::local) {
      LOG(INFO) << l;
    } else if (debug == acs_debug_t::remote) {
      bout.pl(l);
    }
  }

  return result;
}

bool validate_acs(const std::string& expression, acs_debug_t debug) {
  const UserValueProvider up(a()->context());
  const BbsValueProvider bbsp(*a()->config(), a()->sess());
  auto [result, ex_what, debug_info] = 
    sdk::acs::validate_acs(expression, &up, &bbsp);
  if (result) {
    return true;
  }
  if (debug == acs_debug_t::local) {
    LOG(INFO) << ex_what;
  } else if (debug == acs_debug_t::remote) {
    bout.pl(ex_what);
  }
  for (const auto& l : debug_info) {
    if (debug == acs_debug_t::local) {
      LOG(INFO) << l;
    } else if (debug == acs_debug_t::remote) {
      bout.pl(l);
    }
  }
  return false;
}

std::string input_acs(common::Input& in, common::Output& out, const std::string& prompt, 
                      const std::string& orig_text, int max_length) {
  if (!prompt.empty()) {
    out.pl("|#5 WWIV ACS Help");
    out.pl("|#9 ACS expressions are of the form: |#1attribute OP value");
    out.pl("|#9 attributes are: |#1user.sl, user.dsl, user.ar, user.dar, user.name, etc ");
    out.pl("|#9 OP (operators) are: |#1>, <, >=. <=, ==, !=");
    out.nl();
    out.pl("|#9 Expressions can be grouped with |#1() |#9and combined with "
           "|#1|| |#9(or) or |#1&& |#9(and)");
    out.nl();
    out.pl("|#9 Examples: |#2user.sl >= 20");
    out.pl("|#9           |#2user.sl > 100 || user.ar == 'A'");
    out.nl();
    out.pl("|#9 For full documentation see:|#7 http://docs.wwivbbs.org/en/latest/cfg/acs/\r\n");
    out.nl();
    out.print("|#7{}\r\n|#9:", prompt);
  }

  auto s = in.input_text(orig_text, max_length);

  if (!validate_acs(s, acs_debug_t::remote)) {
    out.pausescr();
    return orig_text;
  }
  return s;
}

} // namespace wwiv::bbs
