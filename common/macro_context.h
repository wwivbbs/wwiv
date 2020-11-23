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
#ifndef INCLUDED_COMMON_MACRO_CONTEXT_H
#define INCLUDED_COMMON_MACRO_CONTEXT_H

#include "common/context.h"
#include <string>

namespace wwiv::common {

enum class interpreted_cmd_t { text, movement, expression };
struct Interpreted {
  Interpreted(std::string t, interpreted_cmd_t c) : text(std::move(t)), cmd(c) {}
  // ReSharper disable once CppNonExplicitConvertingConstructor
  Interpreted(std::string t) : text(std::move(t)) {}
  Interpreted() = default;
  int left{0};
  int right{0};
  int up{0};
  int down{0};
  int x{-1};
  int y{-1};
  std::string text;
  interpreted_cmd_t cmd{interpreted_cmd_t::text};
  bool cls{false};
  bool clreol{false};
  bool clrbol{false};
  int nl{0};
};

/**
 * Context and interpreter class for MCI codes and friends.
 */
class MacroContext {
public:
  explicit MacroContext(Context* context) : context_(context) {}
  virtual ~MacroContext() = default;

  // Interprets only a macro code.
  [[nodiscard]] virtual std::string interpret_macro_char(char c) const = 0;
  // Interprets a macro, movement or command.
  // Expects the leading '@', '{', or '[' to be present.
  [[nodiscard]] virtual Interpreted interpret_string(const std::string&) const = 0;
  // Executes a Macro's expression.  This is likely to be done only by
  // the BBS.
  [[nodiscard]] virtual Interpreted evaluate_expression(const std::string&) const = 0;
  // Interprets a macro, movement or command.
  // Expects the leading '@', '{', or '[' to be present.
  [[nodiscard]] virtual Interpreted interpret(std::string::const_iterator& it,
                                              std::string::const_iterator end) const;
  // Context to use when evaluating macros, movements and commands
  [[nodiscard]] Context* context() const { return context_; }

protected:
  Context* context_{nullptr};
};

} // namespace wwiv::common

#endif // __INCLUDED_BBS_INTERPRET_H__
