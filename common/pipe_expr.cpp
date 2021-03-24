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
/*                                                                        */
/**************************************************************************/
#include "common/pipe_expr.h"

#include "common/output.h"
#include "common/value/bbsvalueprovider.h"
#include "common/value/uservalueprovider.h"
#include "sdk/acs/acs.h"
#include "sdk/acs/eval.h"
#include "sdk/value/valueprovider.h"

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::value;
using namespace wwiv::strings;

namespace wwiv::common {

enum class pipe_expr_token_type_t {
  variable,
  number_literal,
  string_literal,
  fn
};

class pipe_expr_token_t {
public:
  pipe_expr_token_type_t type;
  std::string lexeme;
};

static std::optional<pipe_expr_token_t> parse_string(std::string::const_iterator& it,
                                                     const std::string::const_iterator& end) {
  if (*it != '"') {
    LOG(ERROR) << "Parse String called without string!";
    return std::nullopt;
  }
  ++it;
  for (std::string s; it != end; ++it) {
    const auto c = *it;
    if (c == '"') {
      // end if string. advance one more.
      ++it;
      pipe_expr_token_t e{};
      e.type = pipe_expr_token_type_t::string_literal;
      e.lexeme = s;
      return std::make_optional(e);
    }
    s.push_back(c);
  }
  return std::nullopt;
}

static std::optional<pipe_expr_token_t> parse_number(std::string::const_iterator& it,
                                                     const std::string::const_iterator& end) {  
  std::string s;
  for (; it != end && isdigit(*it); ++it) {
    const auto c = *it;
    s.push_back(c);
  }
  if (s.empty()) {
    s ="0";
  }
  pipe_expr_token_t e{};
  e.type = pipe_expr_token_type_t::number_literal;
  e.lexeme = s;
  return std::make_optional(e);
}

std::optional<pipe_expr_token_t> PipeEval::parse_variable(std::string::const_iterator& it,
                                                          const std::string::const_iterator& end) {  
  std::string s;
  for (; it != end && (isalpha(*it) || *it == '.' || *it == '_'); ++it) {
    s.push_back(*it);
  }
  StringTrim(&s);
  StringLowerCase(&s);
  pipe_expr_token_t e{};
  if (stl::contains(fn_map_, s)) {
    e.type = pipe_expr_token_type_t::fn;
  } else {
    e.type = pipe_expr_token_type_t::variable;
  }
  e.lexeme = s;
  return std::make_optional(e);
}

std::vector<pipe_expr_token_t> PipeEval::tokenize(std::string::const_iterator& it,
                                                  const std::string::const_iterator& end) {
  std::vector<pipe_expr_token_t> r;

  std::string l;
  for (; it != end;) {
    const auto c = *it;
    if (iswspace(c)) {
      ++it;
      continue;
    }
    if (c == '"') {
      if (auto o = parse_string(it, end)) {
        r.emplace_back(o.value());
      }
    } else if (isdigit(c)) {
      if (auto o = parse_number(it, end)) {
        r.emplace_back(o.value());
      }
    } else if (isalpha(c) || c == '.' || c == '_') {
      if (auto o = parse_variable(it, end)) {
        r.emplace_back(o.value());
      }
    } else {
      ++it;
    }
  }

  return r;
}

std::string PipeEval::eval_variable(const pipe_expr_token_t& t) {
  const auto [prefix, suffix] = SplitOnceLast(t.lexeme, ".");
  if (prefix == "user") {
    // Only create user if we need it, also don't cache it since eff_sl can change
    const value::UserValueProvider user(context_);
    return user.value(suffix)->as_string();
  }
  if (prefix == "bbs") {
    const value::BbsValueProvider bbs_provider(context_.config(), context_.session_context());
    return bbs_provider.value(suffix)->as_string();
  }

  for (const auto& v : context_.value_providers()) {
    // O(N) is on for small values of n
    if (iequals(prefix, v->prefix())) {
      return v->value(suffix)->as_string();
    }
  }

  // Passthrough unknown variables.
  return StrCat("{", t.lexeme, "}"); // HACK
}

static bool is_truthy(const std::string& s) {
  if (s == "yes" || s == "on" || s == "true" || s == "y") {
    return true;
  }
  return false;
}

std::string eval_fn_mpl(Context&, const std::vector<pipe_expr_token_t>& args) {
  if (args.size() != 1) {
    return "ERROR: MPL expression requires 1 argument.";
  }
  const auto var = args.at(0).lexeme;
  if (var.empty()) {
    return {};
  }
  if (const auto num = to_number<int>(var); num > 0) {
    bout.mpl(num);
  }
  return {};
}

std::string eval_fn_set(Context& context_, const std::vector<pipe_expr_token_t>& args) {
  // Set command only
  if (args.size() != 2) {
    return "ERROR: Set expression requires two arguments.";
  }
  if (args.at(0).type != pipe_expr_token_type_t::variable) {
    return "ERROR: Set expression requires first arg to be variable";
  }
  if (const auto var = args.at(0).lexeme; var == "pause") {
    
    const auto val = is_truthy(args.at(1).lexeme);
    context_.u().set_flag(User::pauseOnPage, val);
    context_.session_context().disable_pause(!val);
  } else if (var == "lines") {
    if (args.at(1).type != pipe_expr_token_type_t::number_literal) {
      return "ERROR: 'set lines' requires number argument";
    }
    if (const auto n = to_number<int>(args.at(1).lexeme); n == 0) {
      bout.clear_lines_listed();
    } else {
      return "ERROR: 'set lines' requires number argument to be 0";
    }
  } else {
    // Handle return values from scripts back to the BBS
    context_.return_values().emplace(var, args.at(1).lexeme);
  }
  return {};
}

template <typename... Args>
std::vector<const ValueProvider*> make_vector(Args&&... args) {
    return {std::forward<Args>(args)...};
}

template <typename... Args>
static bool check_acs_pipe(const std::string& expression, const std::vector<std::unique_ptr<MapValueProvider>>& maps,
  Args... args) {
  if (StringTrim(expression).empty()) {
    // Empty expression is always allowed.
    return true;
  }

  acs::Eval eval(expression);
  auto v = make_vector(args...);
  for (const auto& vp : v) {
    eval.add(vp);
  }

  for (const auto& m : maps) {
    eval.add(m.get());
  }

  return eval.eval();
}

std::string eval_fn_if(Context& context_, const std::vector<pipe_expr_token_t>& args) {
  // Set command only
  if (args.size() != 3) {
    return "ERROR: Set expression requires three arguments.";
  }
  for (const auto& a : args) {
    if (a.type != pipe_expr_token_type_t::string_literal) {
      return "ERROR: IF expression requires all args be strings";
    }
  }

  const auto expr = args.at(0).lexeme;
  const auto yes = args.at(1).lexeme;
  const auto no = args.at(2).lexeme;

  const value::BbsValueProvider bbs_provider(context_);
  const value::UserValueProvider user_provider(context_);
  const auto b = check_acs_pipe(expr, context_.value_providers(), &user_provider, &bbs_provider);
  return b ? yes : no;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
std::string eval_fn_random(Context&, const std::vector<pipe_expr_token_t>& args) {
  const auto num = os::random_number(stl::size_int(args) - 1);
  return args.at(num).lexeme;
}

std::string PipeEval::eval_fn(const std::string& fn, const std::vector<pipe_expr_token_t>& args) {
  if (const auto it = fn_map_.find(fn); it != std::end(fn_map_)) {
    if (it->second) {
      return it->second(context_, args);
    }
  }

  return fmt::format("ERROR: Unknown function: {}", fn);
}


std::string PipeEval::eval(std::vector<pipe_expr_token_t>& tokens) {
  if (tokens.empty()) {
    return {};
  }
  const auto f = tokens.front();
  if (f.type == pipe_expr_token_type_t::string_literal) {
    return f.lexeme;
  }
  if  (f.type == pipe_expr_token_type_t::number_literal) {
    return f.lexeme;
  }
  if (f.type == pipe_expr_token_type_t::variable) {
    return eval_variable(f);
  }
  if (f.type == pipe_expr_token_type_t::fn) {
    if (tokens.size() == 1) {
      const std::vector<pipe_expr_token_t> empty;
      return eval_fn(f.lexeme, empty);
    }
    const std::vector<pipe_expr_token_t> remaining(std::begin(tokens)+1, std::end(tokens));
    return eval_fn(f.lexeme, remaining);
  }
  return {};
}

// Evaluates a pipe expression without the {} around it.
std::string PipeEval::evaluate_pipe_expression_string(const std::string& expr) {
  auto it = std::cbegin(expr);
  const auto end = std::cend(expr);
  auto tokens = tokenize(it, end);

  return eval(tokens);
}

PipeEval::PipeEval(Context& context) : context_(context) {
  fn_map_.try_emplace("pausescr", [](Context&, const std::vector<pipe_expr_token_t>&) -> std::string {
    bout.pausescr();
    return {};
  });
  fn_map_.try_emplace("random", eval_fn_random);
  fn_map_.try_emplace("if", eval_fn_if);
  fn_map_.try_emplace("mpl", eval_fn_mpl);
  fn_map_.try_emplace("set", eval_fn_set);
  fn_map_.try_emplace("sleep", [](Context&, const std::vector<pipe_expr_token_t>& a) -> std::string {
    if (a.empty()) {
      return {};
    }
    const auto num = to_number<int>(a.front().lexeme);
    os::sleep_for(std::chrono::milliseconds(num));
    return {};
  });
  fn_map_.try_emplace("spin", [](Context&, const std::vector<pipe_expr_token_t>& a) -> std::string {
    if (a.size() < 2) {
      return {};
    }
    const auto text = a.at(0).lexeme;
    const auto color = to_number<int>(a.at(1).lexeme);
    bout.spin_puts(text, color);
    return {};
  });
  fn_map_.try_emplace("backprint", [](Context&, const std::vector<pipe_expr_token_t>& a) -> std::string {
    if (a.size() < 4) {
      return {};
    }
    const auto text = a.at(0).lexeme;
    const auto color = to_number<int>(a.at(1).lexeme);
    const auto char_delay = std::chrono::milliseconds(to_number<int>(a.at(2).lexeme));
    const auto str_delay = std::chrono::milliseconds(to_number<int>(a.at(3).lexeme));
    bout.back_puts(text, color, char_delay, str_delay);
    return {};
  });
  fn_map_.try_emplace("rainbow", [](Context&, const std::vector<pipe_expr_token_t>& a) -> std::string {
    const auto text = a.at(0).lexeme;
    bout.rainbow(text);
    return {};
  });
}

std::string PipeEval::eval(std::string expr) {
  auto temp = std::move(expr);
  if (temp.length() <= 2) {
    // Cant' be {..} if length < 2
    return {};
  }
  temp.pop_back();
  temp = temp.substr(1);
  // Now we have ... not {...}

  return evaluate_pipe_expression_string(temp);
}

}
