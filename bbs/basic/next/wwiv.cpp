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

#include "bbs/acs.h"
#include "bbs/interpret.h"
#include "bbs/menus/menucommands.h"
#include "common/output.h"
#include "common/pipe_expr.h"
#include "core/version.h"
#include "wwivbasic/interpreter.h"

#include <string>
#include <vector>

namespace wwiv::bbs::basic {

using wwivbasic::BasicError;
using wwivbasic::Value;
using Args = std::vector<Value>;

// The wwiv module (bbs/basic/wwiv.cpp).
void RegisterWwiv(wwivbasic::Interpreter& interp, const WwivBindings& b) {
  auto& m = interp.add_module("wwiv");

  m.native_function("VERSION", []() { return core::short_version(); });

  // Like the legacy engine, this one also prints its name.
  m.native_function("MODULE_NAME", [b]() {
    b.out.outstr("wwiv\r\n");
    return std::string("wwiv");
  });

  // COMMAND(cmd [, data]) runs a menu command.
  m.native_functionl("COMMAND", [](const Args& args) {
    if (args.empty() || args.size() > 2) {
      throw BasicError(
          fmt::format("COMMAND: expected 1 or 2 argument(s), got {}", args.size()));
    }
    const auto cmd = wwivbasic::from_value<std::string>(args[0]);
    const auto data = args.size() > 1 ? wwivbasic::from_value<std::string>(args[1]) : "";
    menus::interpret_command(nullptr, cmd, data);
    return Value();
  });

  // EVAL([acs]) is 1 if the current user meets the ACS expression, else 0.
  m.native_functionl("EVAL", [](const Args& args) {
    if (args.size() > 1) {
      throw BasicError(fmt::format("EVAL: expected 0 or 1 argument(s), got {}", args.size()));
    }
    if (args.empty()) {
      return Value(0);
    }
    return Value(check_acs(wwivbasic::from_value<std::string>(args[0])) ? 1 : 0);
  });

  // INTERPRET(s) expands the macro named by the first character of s.
  m.native_function("INTERPRET", [b](std::string s) {
    if (!b.ctx) {
      return std::string();
    }
    common::PipeEval pipe_eval(*b.ctx);
    const BbsMacroContext ctx(b.ctx, pipe_eval);
    return ctx.interpret_macro_char(s.empty() ? '\0' : s.front());
  });
}

} // namespace wwiv::bbs::basic
