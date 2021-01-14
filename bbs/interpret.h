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
#ifndef INCLUDED_BBS_INTERPRET_H
#define INCLUDED_BBS_INTERPRET_H

#include "common/context.h"
#include "common/macro_context.h"
#include "sdk/ansi/ansi.h"
#include <string>

class BbsMacroContext final : public wwiv::common::MacroContext {
public:
  typedef std::function<std::string(std::string)> eval_expr_fn;

  explicit BbsMacroContext(wwiv::common::Context* context, eval_expr_fn f) : MacroContext(context), eval_fn_(f) {}
  ~BbsMacroContext() override = default;

  // Interprets only a macro code.
  [[nodiscard]] std::string interpret_macro_char(char c) const override;
  // Interprets a macro, movement or command.
  // Expects the leading '@', '{', or '[' to be present.
  [[nodiscard]] wwiv::common::Interpreted interpret_string(const std::string&) const override;

  [[nodiscard]] wwiv::common::Interpreted evaluate_expression(const std::string&) const override;

  void set_config(wwiv::sdk::Config* config) { config_ = config; }

private:
  wwiv::sdk::Config* config_{nullptr};
  eval_expr_fn eval_fn_;
};

enum class pipe_type_t { none, pipe, macro, movement, expr };
class BbsMacroFilter final : public wwiv::sdk::ansi::AnsiFilter {
public:
  BbsMacroFilter(AnsiFilter* chain, const wwiv::common::MacroContext* ctx)
      : chain_(chain), ctx_(ctx) {}
  bool write(char c) override;
  bool attr(uint8_t a) override;

private:
  AnsiFilter* chain_;
  const wwiv::common::MacroContext* ctx_;
  pipe_type_t pipe_{pipe_type_t::none};
};

#endif
