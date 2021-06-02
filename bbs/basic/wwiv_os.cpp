/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include  "bbs/basic/wwiv_os.h"


#include "bbs/execexternal.h"
#include "bbs/basic/util.h"
#include "core/strings.h"
#include "common/input.h"
#include "common/output.h"
#include "deps/my_basic/core/my_basic.h"
#include <string>

using namespace wwiv::strings;

namespace wwiv::bbs::basic {

static uint32_t chain_type_to_flags(chain_type_t c, file_location_t loc) {
  uint32_t flags = 0;
  if (c == chain_type_t::FOSSIL) {
    flags |= EFLAG_SYNC_FOSSIL;
  } else if (c == chain_type_t::STDIO) {
    flags |= EFLAG_STDIO;
  } else if (c == chain_type_t::NETFOSS) {
    flags |= EFLAG_NETFOSS;
  }

  if (loc != file_location_t::BBS) {
    flags |= EFLAG_TEMP_DIR;
  }
  return flags;
}

bool RegisterNamespaceWWIVOS(mb_interpreter_t* basi) {

  mb_begin_module(basi, "WWIV.OS");

  mb_register_func(basi, "MODULE_NAME", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_empty_function(bas, l));
    return mb_push_string(bas, l, BasicStrDup("wwiv.os"));
  });

  mb_register_func(basi, "EXEC_OPTIONS", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    const auto arg_chaintype = wwiv_mb_pop_string(bas, l);
    MB_FUNC_ERR_IF_ABSENT_MSG(arg_chaintype, "EXEC_OPTIONS: Expected DOORTYPE");
    const auto chain_type =  to_chain_type_t(arg_chaintype.value());

    const auto arg_dir = wwiv_mb_pop_string(bas, l);
    MB_FUNC_ERR_IF_ABSENT_MSG(arg_dir, "EXEC_OPTIONS: Expected dir");
    const auto dir = to_file_location_t(arg_dir.value());

    const auto arg_dropfile = wwiv_mb_pop_string(bas, l);
    MB_FUNC_ERR_IF_ABSENT_MSG(arg_dropfile, "EXEC_OPTIONS: Expected dropfile type");
    const auto dropfile_type = to_dropfile_type_t(arg_dropfile.value());

    auto* sd = get_wwiv_script_userdata(bas);
    const auto handle = sd->emplace_exec_options(chain_type, dir, dropfile_type);
    mb_check(mb_attempt_close_bracket(bas, l));

    return wwiv_mb_push_handle(bas, l, handle);
  });

  mb_register_func(basi, "EXEC", [](struct mb_interpreter_t* bas, void** l) -> int {
    mb_check(mb_attempt_open_bracket(bas, l));
    const auto handle = wwiv_mb_pop_handle(bas, l);
    MB_FUNC_ERR_IF_ABSENT(handle);
    const auto cmd = wwiv_mb_pop_string(bas, l);
    MB_FUNC_ERR_IF_ABSENT(cmd);
    mb_check(mb_attempt_close_bracket(bas, l));

    auto* sd = get_wwiv_script_userdata(bas);
    if (!stl::contains(sd->exec_options, handle.value())) {
      LOG(ERROR) << "Unable to find exec options handle: " << handle.value();
      return MB_FUNC_ERR;
    }
    const auto& opt = stl::at(sd->exec_options, handle.value());

    LOG(INFO) << "Execute Command: " << cmd.value();
    const auto ret = ExecuteExternalProgram(wwiv::bbs::CommandLine(cmd.value()),
                                            chain_type_to_flags(opt.type, opt.loc));

    return mb_push_int(bas, l, ret == 0 ? 1 : 0);
  });

  return mb_end_module(basi) == MB_FUNC_OK;
}

}