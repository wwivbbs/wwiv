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
#include  "bbs/basic/wwiv_file.h"

#include "bbs/basic/util.h"
#include "common/input.h"
#include "common/output.h"
#include "deps/my_basic/core/my_basic.h"
#include <string>

namespace wwiv::bbs::basic {

static file_location_t to_file_location_t(const std::string& s) {
  if (strings::iequals(s, "MENUS")) {
    return file_location_t::MENUS;
  }
  return file_location_t::GFILES;
}

bool RegisterNamespaceWWIVFILE(mb_interpreter_t* basi) {
  // usertype
    mb_register_func(basi, "LOC", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    void* ud_raw;
    mb_check(mb_pop_usertype(bas, l, &ud_raw));
    const auto handle = *static_cast<int*>(ud_raw);
    char* loc_raw = nullptr;
    mb_check(mb_pop_string(bas, l, &loc_raw));
    const auto loc = wwiv::strings::ToStringUpperCase(loc_raw);
    mb_check(mb_attempt_close_bracket(bas, l));
    const auto* sd = get_wwiv_script_userdata(bas);
    sd->out->format("Setting LOC '{}' on handle '{}'", loc, handle);
    // TODO(rushfan): Get Handle and set location.
    return MB_FUNC_OK;
  });

  
  mb_begin_module(basi, "WWIV.file");

  mb_register_func(basi, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    const auto* sd = get_wwiv_script_userdata(bas);
    mb_check(mb_empty_function(bas, l));
    *sd->out << "wwiv.FILE\r\n";
    mb_check(mb_push_string(bas, l, BasicStrDup("wwiv.file")));
    return MB_FUNC_OK;
  });

  mb_register_func(basi, "OPEN_OPTIONS", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    char* arg{nullptr};
    mb_check(mb_pop_string(bas, l, &arg));
    if (!arg) {
      LOG(WARNING) << "OPEN_OPTIONS: Expected Location";
      return MB_FUNC_ERR;
    }
    std::string loc{arg};
    mb_check(mb_attempt_close_bracket(bas, l));
    // We could also just do mb_push_usertype(bas, l, filehandle);
    mb_value_t val;
    val.type = MB_DT_USERTYPE;
    const auto loc_enum = to_file_location_t(loc);
    val.value.integer = static_cast<int>(loc_enum);
    // N.B. We don't use bytes since it's easier to use integer for
    // an integer key.
    mb_push_value(bas, l, val);
    return MB_FUNC_OK;
  });
 
  mb_register_func(basi, "OPEN", [](struct mb_interpreter_t* bas, void** l) -> int {
    auto* sd = get_wwiv_script_userdata(bas);
    mb_check(mb_attempt_open_bracket(bas, l));

    mb_value_t arg;
    mb_make_nil(arg);
    mb_check(mb_pop_value(bas, l, &arg));
    if (arg.type != MB_DT_USERTYPE) {
      return MB_FUNC_ERR;
    }
    const auto handle = ++sd->max_file;
    sd->files[handle].loc = static_cast<file_location_t>(arg.value.integer);

    char* arg_name;
    mb_check(mb_pop_string(bas, l, &arg_name));
    const auto gfdir = sd->ctx->session_context().dirs().gfiles_directory();
    auto path = core::FilePath(gfdir, arg_name);

    char* arg_mode;
    mb_check(mb_pop_string(bas, l, &arg_mode));
    std::string mode = strings::iequals("W", arg_mode) ? "wt" : "rt";

    sd->files[handle].file = std::make_unique<TextFile>(path, mode);

    mb_check(mb_attempt_close_bracket(bas, l));
    mb_check(mb_push_usertype(bas, l, reinterpret_cast<void*>(handle)));

    return MB_FUNC_OK;
  });

  mb_register_func(basi, "READINTOSTRING", [](struct mb_interpreter_t* bas, void** l) -> int {
    auto* sd = get_wwiv_script_userdata(bas);
    mb_check(mb_attempt_open_bracket(bas, l));

    mb_value_t arg;
    mb_make_nil(arg);
    mb_check(mb_pop_value(bas, l, &arg));
    if (arg.type != MB_DT_USERTYPE) {
      return MB_FUNC_ERR;
    }
    const auto handle = arg.value.integer;

    auto& f = wwiv::stl::at(sd->files, handle);
    mb_check(mb_attempt_close_bracket(bas, l));
    const auto s = f.file->ReadFileIntoString();
    mb_check(mb_push_string(bas, l, BasicStrDup(s)));

    return MB_FUNC_OK;
  });
  return mb_end_module(basi) == MB_FUNC_OK;
}

}