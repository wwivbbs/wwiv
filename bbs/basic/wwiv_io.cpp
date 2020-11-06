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

#include "bbs/basic/util.h"
#include "common/input.h"
#include "common/output.h"
#include "deps/my_basic/core/my_basic.h"
#include <string>

namespace wwiv::bbs::basic {


bool RegisterNamespaceWWIVIO(mb_interpreter_t* basi) {
  mb_begin_module(basi, "WWIV.IO");

  mb_register_func(basi, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_empty_function(bas, l));
    return mb_push_string(bas, l, BasicStrDup("wwiv.io"));
  });

  mb_register_func(basi, "PUTS", [](struct mb_interpreter_t* bas, void** l) -> int {
    const auto* sd = get_wwiv_script_userdata(bas);
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      sd->out->bputs(arg);
    }
    return mb_attempt_close_bracket(bas, l);
  });

  mb_register_func(basi, "PL", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    const auto* sd = get_wwiv_script_userdata(bas);
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      sd->out->bputs(arg);
      sd->out->nl();
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(basi, "PRINTFILE", [](struct mb_interpreter_t* bas, void** l) -> int {
    const auto* sd = get_wwiv_script_userdata(bas);
    mb_check(mb_attempt_open_bracket(bas, l));
    if (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      sd->out->printfile(arg);
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(basi, "GETS", [](struct mb_interpreter_t* bas, void** l) -> int {
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

  mb_register_func(basi, "GETKEY", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_empty_function(bas, l));
    const auto* sd = get_wwiv_script_userdata(bas);
    const auto ch = sd->in->getkey();
    char s[2] = {ch, 0};
    return mb_push_string(bas, l, BasicStrDup(s));
  });

  mb_register_func(basi, "CLS", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_empty_function(bas, l));
    const auto* sd = get_wwiv_script_userdata(bas);
    sd->out->cls();
    return MB_FUNC_OK;
  });

  mb_register_func(basi, "NL", [](struct mb_interpreter_t* bas, void** l) -> int {
    const auto* sd = get_wwiv_script_userdata(bas);
    mb_check(mb_attempt_open_bracket(bas, l));
    int num_lines = 1;
    if (mb_has_arg(bas, l)) {
      mb_check(mb_pop_int(bas, l, &num_lines));
    }
    sd->out->nl(num_lines);
    return mb_attempt_close_bracket(bas, l);
  });

  mb_register_func(basi, "YN", [](struct mb_interpreter_t* bas, void** l) -> int {
    const auto* sd = get_wwiv_script_userdata(bas);
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      sd->out->bputs(arg);
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    const auto ret = bin.yesno();

    return mb_push_int(bas, l, ret ? 1 : 0);
  });

  mb_register_func(basi, "NY", [](struct mb_interpreter_t* bas, void** l) -> int {
    const auto* sd = get_wwiv_script_userdata(bas);
    mb_check(mb_attempt_open_bracket(bas, l));
    while (mb_has_arg(bas, l)) {
      char* arg = nullptr;
      mb_check(mb_pop_string(bas, l, &arg));
      sd->out->bputs(arg);
    }
    mb_check(mb_attempt_close_bracket(bas, l));
    const auto ret = bin.noyes();

    return mb_push_int(bas, l, ret ? 1 : 0);
  });

  mb_register_func(basi, "PAUSE", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_empty_function(bas, l));
    bout.pausescr();
    return MB_FUNC_OK;
  });

  return mb_end_module(basi) == MB_FUNC_OK;
}

}