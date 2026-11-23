/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2026, WWIV Software Services                  */
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
#include "bbs/basic/next/wwiv_bindings.h"

#include "bbs/execexternal.h"
#include "bbs/stuffin.h"
#include "core/log.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/vardec.h"
#include "wwivbasic/interpreter.h"
#include "wwivbasic/object.h"

#include <cstdint>
#include <string>

using namespace wwiv::strings;

namespace wwiv::bbs::basic {

using wwivbasic::BasicError;
using wwivbasic::Value;

namespace {

enum class exec_chain_t { door32, stdio, fossil, netfoss };
enum class exec_dir_t { gfiles, menus, temp, bbs };

// What EXEC_OPTIONS returns. The dropfile type is accepted but, as in the
// legacy engine, not used.
struct ExecOptions {
  exec_chain_t chain{exec_chain_t::door32};
  exec_dir_t dir{exec_dir_t::temp};
  std::string dropfile;
};

// The same names as the legacy engine (bbs/basic/util.cpp); anything else is
// the default.
exec_chain_t to_exec_chain(const std::string& s) {
  if (iequals(s, "FOSSIL")) {
    return exec_chain_t::fossil;
  }
  if (iequals(s, "NETFOSS")) {
    return exec_chain_t::netfoss;
  }
  if (iequals(s, "STDIO")) {
    return exec_chain_t::stdio;
  }
  return exec_chain_t::door32;
}

exec_dir_t to_exec_dir(const std::string& s) {
  if (iequals(s, "MENUS")) {
    return exec_dir_t::menus;
  }
  if (iequals(s, "TEMP")) {
    return exec_dir_t::temp;
  }
  if (iequals(s, "BBS")) {
    return exec_dir_t::bbs;
  }
  return exec_dir_t::gfiles;
}

// As chain_type_to_flags in bbs/basic/wwiv_os.cpp.
uint32_t exec_flags(const ExecOptions& o) {
  uint32_t flags = 0;
  switch (o.chain) {
  case exec_chain_t::fossil:
    flags |= EFLAG_SYNC_FOSSIL;
    break;
  case exec_chain_t::stdio:
    flags |= EFLAG_STDIO;
    break;
  case exec_chain_t::netfoss:
    flags |= EFLAG_NETFOSS;
    break;
  case exec_chain_t::door32:
    break;
  }
  if (o.dir != exec_dir_t::bbs) {
    flags |= EFLAG_TEMP_DIR;
  }
  return flags;
}

} // namespace

// The wwiv.os module (bbs/basic/wwiv_os.cpp), registered only when the sysop
// enables it: runs external programs.
void RegisterWwivOs(wwivbasic::Interpreter& interp, const WwivBindings& b) {
  auto& m = interp.add_module("wwiv.os");
  auto& options_class = m.class_<ExecOptions>("exec_options");

  m.native_function("MODULE_NAME", []() { return std::string("wwiv.os"); });

  // EXEC_OPTIONS(door_type, dir, dropfile): door_type is DOOR32, STDIO,
  // FOSSIL or NETFOSS; dir is BBS to run in the BBS directory, anything else
  // to run in the temp directory.
  m.native_function("EXEC_OPTIONS", [&options_class](std::string door_type, std::string dir,
                                                     std::string dropfile) {
    return options_class.wrap(ExecOptions{to_exec_chain(door_type), to_exec_dir(dir), dropfile});
  });

  // EXEC(options, command) runs command; returns 1 if it exited with 0, else 0.
  m.native_function("EXEC", [b, &options_class](Value options, std::string cmd) {
    const auto opts = options_class.unwrap(options);
    if (!opts) {
      throw BasicError(fmt::format("EXEC: expected options from wwiv.os.exec_options, got {}",
                                   options.a_type_name()));
    }
    if (!b.ctx) {
      throw BasicError("EXEC: no session to run the program in");
    }
    LOG(INFO) << "Execute Command: " << cmd;
    CommandLine cl(cmd);
    return ExecuteExternalProgram(cl, exec_flags(*opts)) == 0 ? 1 : 0;
  });
}

} // namespace wwiv::bbs::basic
