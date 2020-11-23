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
/*                                                                        */
/**************************************************************************/
#include "bbs/pipe_expr.h"

#include "common/output.h"

using std::string;
using std::to_string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::bbs {

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

std::optional<pipe_expr_token_t> parse_string(string::const_iterator& it, const string::const_iterator& end) {
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

std::optional<pipe_expr_token_t> parse_number(string::const_iterator& it, const string::const_iterator& end) {  
  std::string s;
  for (; isdigit(*it) && it != end; ++it) {
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

std::optional<pipe_expr_token_t> parse_variable(string::const_iterator& it, const string::const_iterator& end) {  
  std::string s;
  for (; it != end && (isalpha(*it) || *it == '.'); ++it) {
    s.push_back(*it);
  }
  StringTrim(&s);
  StringLowerCase(&s);
  pipe_expr_token_t e{};
  e.type = pipe_expr_token_type_t::variable;
  e.lexeme = s;
  if (s == "set") {
    // Only set and pause are supported now.
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
    } else if (isalpha(c) || c == '.') {
      if (auto o = parse_variable(it, end)) {
        r.emplace_back(o.value());
      }
    } else {
      ++it;
    }
  }

  return r;
}

string eval_variable(const pipe_expr_token_t& t) {
  // TODO(rushfan): Implement me.
  return t.lexeme;  // HACK
}

bool is_truthy(const std::string& s) {
  if (s == "yes" || s == "on" || s == "true" || s == "y") {
    return true;
  }
  return false;
}

string eval_fn(const string& fn, const std::vector<pipe_expr_token_t>& args) {
  if (fn == "pause") {
    bout.pausescr();
    return {};
  }
  if (fn != "set") {
    return fmt::format("ERROR: Unknown function: {}", fn);
  }
  // Set command only
  if (args.size() != 2) {
    return "ERROR: Set expression requires two arguments.";
  }
  if (args.at(0).type != pipe_expr_token_type_t::variable) {
    return "ERROR: Set expression requires first arg to be variable";
  }
  const auto var = args.at(0).lexeme;
  if (var == "pause") {
    const auto val = is_truthy(args.at(1).lexeme);
    bout.user().SetStatusFlag(User::pauseOnPage, val);
    bout.sess().disable_pause(!val);
  } else if (var == "lines") {
    if (args.at(1).type != pipe_expr_token_type_t::number_literal) {
      return "ERROR: 'set lines' requires number argument";
    }
    const auto n = to_number<int>(args.at(1).lexeme);
    if (n == 0) {
      bout.clear_lines_listed();
    } else {
      return "ERROR: 'set lines' requires number argument to be 0";
    }
  }
  return {};
}


std::string eval(std::vector<pipe_expr_token_t>& tokens) {
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
string evaluate_pipe_expression_string(const std::string& expr) {
  auto it = std::cbegin(expr);
  const auto end = std::cend(expr);
  auto tokens = tokenize(it, end);

  return eval(tokens);
}

std::string evaluate_pipe_expression(std::string expr) {
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
