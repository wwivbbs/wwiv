/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include  "bbs/basic/wwiv_time.h"

#include "bbs/basic/util.h"
#include "common/input.h"
#include "common/output.h"
#include "deps/my_basic/core/my_basic.h"
#include <string>

namespace wwiv::bbs::basic {

bool RegisterNamespaceWWIVTIME(mb_interpreter_t* basi) {

  mb_begin_module(basi, "WWIV.TIME");

  mb_register_func(basi, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_empty_function(bas, l));
    return mb_push_string(bas, l, BasicStrDup("wwiv.time"));
  });

  mb_register_func(basi, "FORMAT", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    char* raw_arg{nullptr};
    int t{};
    mb_check(mb_pop_int(bas, l, &t));
    mb_check(mb_pop_string(bas, l, &raw_arg));
    if (!raw_arg) {
      LOG(WARNING) << "FORMAT: Expected format style";
      return MB_FUNC_ERR;
    }
    const auto s = core::DateTime::from_time_t(t).to_string(raw_arg);
    const auto ret = wwiv_mb_make_string(s);
    mb_check(mb_attempt_close_bracket(bas, l));

    return mb_push_value(bas, l, ret);
  });

  return mb_end_module(basi) == MB_FUNC_OK;
}

}