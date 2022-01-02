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
#include  "bbs/basic/wwiv_file.h"

#include "bbs/basic/util.h"
#include "common/input.h"
#include "common/output.h"
#include "deps/my_basic/core/my_basic.h"
#include <string>

namespace wwiv::bbs::basic {

bool RegisterNamespaceWWIVFILE(mb_interpreter_t* basi) {  
  mb_begin_module(basi, "WWIV.IO.FILE");

  mb_register_func(basi, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_empty_function(bas, l));
    return mb_push_string(bas, l, BasicStrDup("wwiv.io.file"));
  });

  mb_register_func(basi, "OPEN_OPTIONS", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));

    auto loc = wwiv_mb_pop_string(bas, l);
    MB_FUNC_ERR_IF_ABSENT_MSG(loc, "OPEN_OPTIONS: Expected Location");
    const auto loc_enum = to_file_location_t(loc.value());
    if (loc_enum != file_location_t::GFILES && loc_enum != file_location_t::MENUS) {
      LOG(ERROR) << "Invalid location specified: " << loc.value();
      return MB_FUNC_ERR;
    }
    mb_check(mb_attempt_close_bracket(bas, l));

    // N.B. We don't use bytes since it's easier to use integer for
    // an integer key.
    return wwiv_mb_push_handle(bas, l, static_cast<int>(loc_enum));
  });
 
  mb_register_func(basi, "OPEN", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    auto arg = wwiv_mb_pop_usertype(bas, l);
    MB_FUNC_ERR_IF_ABSENT(arg);
    auto loc = static_cast<file_location_t>(arg.value().value.integer);

    auto arg_name = wwiv_mb_pop_string(bas, l);
    MB_FUNC_ERR_IF_ABSENT(arg_name);

    auto* sd = get_wwiv_script_userdata(bas);
    const auto gfdir = sd->ctx->session_context().dirs().gfiles_directory();
    const std::filesystem::path file_part{arg_name.value()};
    const auto filename = file_part.filename();
    const auto path = core::FilePath(gfdir, filename);

    auto arg_mode = wwiv_mb_pop_string(bas, l);
    MB_FUNC_ERR_IF_ABSENT(arg_name);
    std::string mode = strings::iequals("W", arg_mode.value()) ? "wt" : "rt";

    const auto handle = sd->allocate_handle();
    sd->files[handle].loc = loc;
    sd->files[handle].file = std::make_unique<TextFile>(path, mode);
    mb_check(mb_attempt_close_bracket(bas, l));
    return wwiv_mb_push_handle(bas, l, static_cast<int>(handle));
  });

  mb_register_func(basi, "READINTOSTRING", [](struct mb_interpreter_t* bas, void** l) -> int {
    auto* sd = get_wwiv_script_userdata(bas);
    mb_check(mb_attempt_open_bracket(bas, l));

    const auto handle = wwiv_mb_pop_handle(bas, l);
    MB_FUNC_ERR_IF_ABSENT(handle);

    auto& f = stl::at(sd->files, handle.value());
    mb_check(mb_attempt_close_bracket(bas, l));
    const auto s = f.file->ReadFileIntoString();
    mb_check(mb_push_string(bas, l, BasicStrDup(s)));

    return MB_FUNC_OK;
  });

  // Read lines of a file into collection.
  mb_register_func(basi, "READLINES", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));

    const auto handle = wwiv_mb_pop_handle(bas, l);
    MB_FUNC_ERR_IF_ABSENT(handle);
    auto* sd = get_wwiv_script_userdata(bas);
    auto& f = stl::at(sd->files, handle.value());
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

    const auto handle = wwiv_mb_pop_handle(bas, l);
    MB_FUNC_ERR_IF_ABSENT(handle);

    auto* sd = get_wwiv_script_userdata(bas);
    auto& f = stl::at(sd->files, handle.value());
    const auto t = static_cast<int>(core::File::last_write_time(f.file->full_pathname()));
    
    mb_check(mb_attempt_close_bracket(bas, l));
    mb_push_int(bas, l, t);
    return MB_FUNC_OK;
  });

  return mb_end_module(basi) == MB_FUNC_OK;
}

}