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

#include "common/input.h"
#include "common/output.h"
#include "wwivbasic/console.h"
#include "wwivbasic/interpreter.h"

#include <string>
#include <vector>

namespace wwiv::bbs::basic {

using wwivbasic::BasicError;
using wwivbasic::Value;
using Args = std::vector<Value>;

namespace {

// Writes each argument with outstr, as the legacy PUTS, OUTSTR, YN and NY do.
void outstr_each(common::Output& out, const Args& args) {
  for (const auto& a : args) {
    out.outstr(wwivbasic::from_value<std::string>(a));
  }
}

} // namespace

// The wwiv.io module (bbs/basic/wwiv_io.cpp).
void RegisterWwivIo(wwivbasic::Interpreter& interp, const WwivBindings& b) {
  auto& m = interp.add_module("wwiv.io");

  m.native_function("MODULE_NAME", []() { return std::string("wwiv.io"); });

  // PUTS is the old name for OUTSTR. Both write each argument.
  m.native_functionl("PUTS", [b](const Args& args) {
    outstr_each(b.out, args);
    return Value();
  });
  m.native_functionl("OUTSTR", [b](const Args& args) {
    outstr_each(b.out, args);
    return Value();
  });

  // PL writes each argument followed by a newline.
  m.native_functionl("PL", [b](const Args& args) {
    for (const auto& a : args) {
      b.out.outstr(wwivbasic::from_value<std::string>(a));
      b.out.nl();
    }
    return Value();
  });

  m.native_functionl("PRINTFILE", [b](const Args& args) {
    if (args.size() > 1) {
      throw BasicError(
          fmt::format("PRINTFILE: expected 0 or 1 argument(s), got {}", args.size()));
    }
    if (!args.empty()) {
      b.out.printfile(wwivbasic::from_value<std::string>(args[0]));
    }
    return Value();
  });

  // GETS([max_length]) reads a line of at most max_length characters. With no
  // length (or one that isn't positive) it reads nothing and returns "".
  m.native_functionl("GETS", [b](const Args& args) {
    if (args.size() > 1) {
      throw BasicError(fmt::format("GETS: expected 0 or 1 argument(s), got {}", args.size()));
    }
    const auto max_length = args.empty() ? 0 : wwivbasic::from_value<int>(args[0]);
    if (max_length <= 0) {
      return Value();
    }
    return Value(b.console.read_line(max_length));
  });

  m.native_function("GETKEY", [b]() { return b.console.read_key(); });

  m.native_function("CLS", [b]() { b.out.cls(); });

  m.native_functionl("NL", [b](const Args& args) {
    if (args.size() > 1) {
      throw BasicError(fmt::format("NL: expected 0 or 1 argument(s), got {}", args.size()));
    }
    b.out.nl(args.empty() ? 1 : wwivbasic::from_value<int>(args[0]));
    return Value();
  });

  // YN(prompt...) defaults to No, NY(prompt...) to Yes; both return 1 or 0.
  m.native_functionl("YN", [b](const Args& args) {
    outstr_each(b.out, args);
    return Value(b.in.yesno() ? 1 : 0);
  });
  m.native_functionl("NY", [b](const Args& args) {
    outstr_each(b.out, args);
    return Value(b.in.noyes() ? 1 : 0);
  });

  m.native_function("PAUSE", [b]() { b.out.pausescr(); });
}

} // namespace wwiv::bbs::basic
