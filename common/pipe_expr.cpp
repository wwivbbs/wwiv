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
#include "sdk/acs/acs.h"
#include "sdk/acs/eval.h"
#include "sdk/acs/uservalueprovider.h"
#include "sdk/acs/valueprovider.h"

using std::string;
using std::to_string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::common {

enum class pipe_expr_token_type_t {
  variable,
  number_literal,
  string_literal,
  fn
};

struct pipe_expr_token_t {
  pipe_expr_token_type_t type;
  std::string lexeme;
};

static std::optional<pipe_expr_token_t> parse_string(string::const_iterator& it, const string::const_iterator& end) {
  if (*it != '"') {
    LOG(ERROR) << "Parse String called without string!";
    return std::nullopt;
  }
  ++it;
  std::string s;
  for (; it != end; ++it) {
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

static std::optional<pipe_expr_token_t> parse_number(string::const_iterator& it, const string::const_iterator& end) {  
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

static std::optional<pipe_expr_token_t> parse_variable(string::const_iterator& it, const string::const_iterator& end) {  
  std::string s;
  for (; it != end && (isalpha(*it) || *it == '.' || *it == '_'); ++it) {
    s.push_back(*it);
  }
  StringTrim(&s);
  StringLowerCase(&s);
  pipe_expr_token_t e{};
  e.type = pipe_expr_token_type_t::variable;
  e.lexeme = s;
  if (s == "set") {
    e.type = pipe_expr_token_type_t::fn;
  }
  if (s == "if") {
    e.type = pipe_expr_token_type_t::fn;
  }
  if (s == "pause" && (it == end || *it != '=')) {
    // pause can be both a variable
    e.type = pipe_expr_token_type_t::fn;
  }

  return std::make_optional(e);
}

static std::vector<pipe_expr_token_t> tokenize(std::string::const_iterator& it, const std::string::const_iterator& end) {
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
  std::map<std::string, std::unique_ptr<acs::ValueProvider>> p;
  auto eff_sl = context_.session_context().effective_sl();
  if (eff_sl == 0) {
    eff_sl = context_.u().sl();
  }
  const auto& eslrec = context_.config().sl(eff_sl);
  p["user"] = std::make_unique<acs::UserValueProvider>(context_.config(), context_.u(), eff_sl, eslrec);

  const auto [prefix, suffix] = SplitOnceLast(t.lexeme, ".");
  if (const auto& iter = p.find(prefix); iter != std::end(p)) {
    return iter->second->value(suffix)->as_string();  
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

std::string PipeEval::eval_fn_set(const std::vector<pipe_expr_token_t>& args) {
  // Set command only
  if (args.size() != 2) {
    return "ERROR: Set expression requires two arguments.";
  }
  if (args.at(0).type != pipe_expr_token_type_t::variable) {
    return "ERROR: Set expression requires first arg to be variable";
  }
  if (const auto var = args.at(0).lexeme; var == "pause") {
    
    const auto val = is_truthy(args.at(1).lexeme);
    context_.u().SetStatusFlag(User::pauseOnPage, val);
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

// TODO(rushfan): make sdk::acs::check_acs take optional vector of ValueProviders
static bool check_acs(const Config& config, const User& user, int eff_sl,
                      const std::string& expression,
                      const std::vector<std::unique_ptr<MapValueProvider>>& maps) {
  if (StringTrim(expression).empty()) {
    // Empty expression is always allowed.
    return true;
  }

  acs::Eval eval(expression);
  const auto& eslrec = config.sl(eff_sl);
  eval.add("user", std::make_unique<acs::UserValueProvider>(config, user, eff_sl, eslrec));

  for (const auto& m : maps) {
    auto mm = std::make_unique<MapValueProvider>(m->prefix(), m->map());
    eval.add(m->prefix(), std::move(mm));
  }

  return eval.eval();
}


std::string PipeEval::eval_fn_if(const std::vector<pipe_expr_token_t>& args) {
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

  auto eff_sl = context_.session_context().effective_sl();
  if (eff_sl == 0) {
    eff_sl = context_.u().sl();
  }
  const auto b = check_acs(context_.config(), context_.u(), eff_sl, expr, context_.value_providers());
  return b ? yes : no;
}

std::string PipeEval::eval_fn(const std::string& fn, const std::vector<pipe_expr_token_t>& args) {
  if (fn == "pause") {
    bout.pausescr();
    return {};
  }
  if (fn == "set") {
    return eval_fn_set(args);
  }
  if (fn == "if") {
    return eval_fn_if(args);
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
