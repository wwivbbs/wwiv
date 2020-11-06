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
  mb_begin_module(basi, "WWIV.IO.FILE");

  mb_register_func(basi, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_empty_function(bas, l));
    return mb_push_string(bas, l, BasicStrDup("wwiv.io.file"));
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
    mb_value_t val;
    val.type = MB_DT_USERTYPE;
    const auto loc_enum = to_file_location_t(loc);
    val.value.integer = static_cast<int>(loc_enum);

    mb_check(mb_attempt_close_bracket(bas, l));
    // N.B. We don't use bytes since it's easier to use integer for
    // an integer key.
    return mb_push_value(bas, l, val);
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
    const std::filesystem::path file_part{arg_name};
    const auto filename = file_part.filename();
    const auto path = core::FilePath(gfdir, filename);

    char* arg_mode{nullptr};
    mb_check(mb_pop_string(bas, l, &arg_mode));
    std::string mode = strings::iequals("W", arg_mode) ? "wt" : "rt";

    sd->files[handle].file = std::make_unique<TextFile>(path, mode);

    mb_check(mb_attempt_close_bracket(bas, l));
    return mb_push_usertype(bas, l, reinterpret_cast<void*>(handle));
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

    auto& f = stl::at(sd->files, handle);
    mb_check(mb_attempt_close_bracket(bas, l));
    const auto s = f.file->ReadFileIntoString();
    mb_check(mb_push_string(bas, l, BasicStrDup(s)));

    return MB_FUNC_OK;
  });

  // Read lines of a file into collection.
  mb_register_func(basi, "READLINES", [](struct mb_interpreter_t* bas, void** l) -> int {
    auto* sd = get_wwiv_script_userdata(bas);
    mb_check(mb_attempt_open_bracket(bas, l));

    mb_value_t arg_handle;
    mb_make_nil(arg_handle);
    mb_check(mb_pop_value(bas, l, &arg_handle));
    if (arg_handle.type != MB_DT_USERTYPE) {
      return MB_FUNC_ERR;
    }
    const auto handle = arg_handle.value.integer;
    auto& f = stl::at(sd->files, handle);
    auto lines = f.file->ReadFileIntoVector();

    mb_value_t arg;
    mb_make_nil(arg);
    mb_check(mb_pop_value(bas, l, &arg));
    // arg should be coll.
    if (arg.type != MB_DT_LIST) {
      *sd->out << "|#6Error: Only saving a LIST is currently supported. (not DICT)\r\n";
      return MB_FUNC_WARNING;
    }

    auto current_count = 0;
    mb_check(mb_count_coll(bas, l, arg, &current_count));
    for (const auto& line : lines) {
      const auto val = wwiv_mb_make_string(line);
      const auto idx = wwiv_mb_make_int(current_count++);
      const auto ret = mb_set_coll(bas, l, arg, idx, val);
      if (ret != MB_FUNC_OK) {
        sd->out->bputs("[oops] ");
      }
    }

    mb_check(mb_attempt_close_bracket(bas, l));
    return MB_FUNC_OK;
  });

  mb_register_func(basi, "LAST_MODIFIED", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));

    mb_value_t arg_handle;
    mb_make_nil(arg_handle);
    mb_check(mb_pop_value(bas, l, &arg_handle));
    if (arg_handle.type != MB_DT_USERTYPE) {
      return MB_FUNC_ERR;
    }

    auto* sd = get_wwiv_script_userdata(bas);
    const auto handle = arg_handle.value.integer;
    auto& f = stl::at(sd->files, handle);
    const auto t = static_cast<int>(core::File::last_write_time(f.file->full_pathname()));
    
    mb_check(mb_attempt_close_bracket(bas, l));
    mb_push_int(bas, l, t);
    return MB_FUNC_OK;
  });

  return mb_end_module(basi) == MB_FUNC_OK;
}

}