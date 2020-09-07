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
#include  "bbs/basic/wwiv_io.h"

#include "common/com.h"
#include "common/input.h"
#include "bbs/interpret.h"
#include "bbs/menu.h"
#include "common/output.h"
#include "common/pause.h"
#include "common/printfile.h"
#include "bbs/basic/util.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/strings.h"
#include "deps/my_basic/core/my_basic.h"
#include <cassert>
#include <string>

namespace wwiv::bbs::basic {


bool RegisterNamespaceWWIVIO(mb_interpreter_t* bas) {
  mb_begin_module(bas, "WWIV.IO");

  mb_register_func(bas, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    mb_check(mb_attempt_close_bracket(bas, l));
    script_out() << "wwiv.io\r\n";
    mb_check(mb_push_string(bas, l, BasicStrDup("wwiv.io")));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "PUTS", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      script_out().bputs(arg);
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "PL", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      script_out().bputs(arg);
      script_out().nl();
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "PRINTFILE", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    if (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      printfile(arg);
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "GETS", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    auto arg = 0;
    if (mb_has_arg(bas, l)) {
      mb_check(mb_pop_int(bas, l, &arg));
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    if (arg > 0) {
      const auto s = bin.input_text("", arg);
      mb_push_string(bas, l, BasicStrDup(s));
    }
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "GETKEY", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    mb_check(mb_attempt_close_bracket(bas, l));
    const auto ch = script_in().getkey();
    char s[2] = {ch, 0};
    mb_push_string(bas, l, BasicStrDup(s));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "CLS", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    script_out().cls();
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "NL", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    int num_lines = 1;
    if (mb_has_arg(bas, l)) {
      mb_check(mb_pop_int(bas, l, &num_lines));
    }
    script_out().nl(num_lines);
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "YN", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      script_out().bputs(arg);
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    const auto ret = bin.yesno();

    mb_push_int(bas, l, ret ? 1 : 0);
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "NY", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      script_out().bputs(arg);
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    const auto ret = bin.noyes();

    mb_push_int(bas, l, ret ? 1 : 0);
    return MB_FUNC_OK;
  });

  mb_register_func(bas, "PAUSE", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    mb_check(mb_attempt_close_bracket(bas, l));
    bout.pausescr();
    return MB_FUNC_OK;
  });

  return mb_end_module(bas) == MB_FUNC_OK;
}

}