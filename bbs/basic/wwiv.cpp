/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include  "bbs/basic/wwiv.h"

#include "bbs/basic/util.h"
#include "bbs/interpret.h"
#include "bbs/menu.h"
#include "bbs/acs.h"
#include "core/version.h"
#include "deps/my_basic/core/my_basic.h"

#include <string>

namespace wwiv::bbs::basic {

static int _version(struct mb_interpreter_t* bas, void** l) {
  mb_check(mb_attempt_open_bracket(bas, l));
  mb_check(mb_attempt_close_bracket(bas, l));
  mb_push_string(bas, l, BasicStrDup(wwiv::core::short_version()));
  return MB_FUNC_OK;
}

bool RegisterNamespaceWWIV(mb_interpreter_t* bas) {
  mb_begin_module(bas, "WWIV");
  mb_register_func(bas, "VERSION", _version);

  mb_register_func(bas, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    mb_check(mb_attempt_close_bracket(bas, l));
    script_out() << "wwiv\r\n";
    mb_check(mb_push_string(bas, l, BasicStrDup("wwiv")));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "COMMAND", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    char* arg = nullptr;
    if (mb_has_arg(bas, l)) {
      mb_check(mb_pop_string(bas, l, &arg));
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    if (arg) {
      wwiv::menus::InterpretCommand(nullptr, arg);
    }
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "EVAL", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    char* arg = nullptr;
    if (mb_has_arg(bas, l)) {
      mb_check(mb_pop_string(bas, l, &arg));
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    if (arg) {
      auto ret = wwiv::bbs::check_acs(arg);
      mb_push_int(bas, l, ret ? 1 : 0);
    } else {
      mb_push_int(bas, l, 0);
    }
    return MB_FUNC_OK;
  });

  // Crappy, awful API way to get data out of WWIVs' macros.
  mb_register_func(bas, "INTERPRET", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    if (!mb_has_arg(bas, l)) {
      return MB_FUNC_ERR;
    }
    char* arg = nullptr;
    mb_check(mb_pop_string(bas, l, &arg));

    const auto* d = get_wwiv_script_userdata(bas);
    MacroContext ctx(d->ctx);
    const auto s = ctx.interpret(*arg);
    mb_check(mb_attempt_close_bracket(bas, l));
    mb_push_string(bas, l, BasicStrDup(s));
    return MB_FUNC_OK;
  });

  return mb_end_module(bas) == MB_FUNC_OK;
}


}