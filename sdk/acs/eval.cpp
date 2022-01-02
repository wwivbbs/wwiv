/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*           Copyright (C)2020-2022, WWIV Software Services               */
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
#include "sdk/acs/eval.h"

#include "core/log.h"
#include "core/strings.h"
#include "core/parser/ast.h"
#include "core/parser/lexer.h"
#include "fmt/ostream.h"
#include "fmt/printf.h"
#include "fmt/ostream.h"
#include "sdk/acs/eval_error.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::core::parser;
using namespace wwiv::strings;
using namespace wwiv::sdk::value;

namespace wwiv::sdk::acs {

static std::tuple<std::string, std::string> split_obj_name(const std::string& name) {
  if (const auto idx = name.find('.'); idx == std::string::npos) {
    // If we only have a name with no dot, return it as the attribute and not prefix.
    return std::make_tuple("", name);
  }
  return SplitOnceLast(name, ".");
}

std::optional<Value> Eval::to_value(Factor* n) {
  switch (n->factor_type()) {
  case FactorType::int_value:
    return Value(n->int_value());
  case FactorType::string_val:
    return Value(n->value());
  case FactorType::variable: {
    const auto sval = n->value();
    auto [prefix, member] = split_obj_name(sval);
    if (auto vpi = providers_.find(prefix); vpi != std::end(providers_)) {
      if (auto o = vpi->second->value(member)) {
        return {o.value()};
      }
    }
    throw eval_error(fmt::format("No object named '{}' exists.", n->value()));
  }
  }
  throw eval_error(fmt::format("Error finding factor for object: '{}'.", to_string(*n)));
}


Eval::Eval(std::string expression) : expression_(std::move(expression)) {
  add(&default_provider_);  
}


void Eval::visit(Expression* n) { 
  //VLOG(2) << "Evaluating: " << n->ToString(false);  
  std::optional<Value> left;
  if (auto* factor = dynamic_cast<Factor*>(n->left())) {
    left = to_value(factor);
  } else {
    // left must be expression, grab it from the cache.
    left = values_[n->left()->id()];
  }
  std::optional<Value> right;
  if (auto* factor = dynamic_cast<Factor*>(n->right())) {
    right = to_value(factor);
  } else {
    // left must be expression, grab it from the cache.
    right = values_[n->right()->id()];
  }

  auto eval_expr = fmt::format("{}(id:{}) {} {}(id:{})", left.value(), n->left()->id(),
                               to_symbol(n->op()), right.value(), n->right()->id());
  //VLOG(1) << "EVAL: " << eval_expr;

  // cache value
  auto result = Value::eval(left.value(), n->op(), right.value());
  if (result.is_boolean()) {
    debug_info_.emplace_back(fmt::format("Expression '{}' evaluated to {}. Stored as id: '{}'",
                                         eval_expr, result.as_boolean() ? "true" : "false",
                                         n->id()));
  }
  values_[n->id()] = result;
}

void Eval::visit(Factor* n) { 
  if (n->factor_type() == FactorType::variable) {
    const auto sval = n->value();
    auto [prefix, member] = split_obj_name(sval);
    if (auto vpi = providers_.find(prefix); vpi != std::end(providers_)) {
      if (auto o = vpi->second->value(member)) {
        values_.emplace(n->id(), o.value());
      }
    }
  }
}

bool Eval::eval_throws() {
  //VLOG(1) << "Eval::eval_throws: " << expression_;

  Lexer l(expression_);
  if (!l.ok()) {
    std::string error_token;
    for (const auto& t : l.tokens()) {
      if (t.type == TokenType::error) {
        error_token += to_string(t);
      }
    }
    throw eval_error(fmt::format("Failed to lex expression: '{}'; \r\nError {}: ", expression_, error_token));
  }

  Ast ast{};
  if (!ast.parse(l)) {
      return false;
  }
  auto* root = ast.root();
  if (!root) {
    throw eval_error(fmt::format("Failed to parse expression: '{}'.", expression_));
  }
  //VLOG(1) << "Root: " << root->ToString();
  if (root->ast_type() == AstType::AST_ERROR) {
    const auto* error_node = dynamic_cast<ErrorNode*>(root);
    throw eval_error(error_node->message);
  }

  root->accept(this);

  if (auto* expr = dynamic_cast<Expression*>(root)) {
    auto it = values_.find(expr->id());
    if (it == std::end(values_)) {
      throw eval_error(fmt::format("Unable to find expression id: '{}'.", expr->id()));
    }
    return it->second.as_boolean();
  }
  throw eval_error(fmt::format("Failed to parse expression: '{}'.", expression_));
}

bool Eval::eval() {

  try {
    return eval_throws();
  } catch (const eval_error& error) {
    error_text_ = error.what();
    //VLOG(1) << "Eval Error: " << error_text_;
    debug_info_.emplace_back(error_text_);
  }
  return false;
}

bool Eval::add(const value::ValueProvider* p) { 
  providers_[p->prefix()] = p;
  return true; 
}

bool Eval::add(std::unique_ptr<value::ValueProvider>&& p) {
  add(p.get());
  providers_storage_[p->prefix()] = std::move(p);
  return true; 
}


} // namespace wwiv::sdk::acs
