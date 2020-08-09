/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*                Copyright (C)2020, WWIV Software Services               */
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

#include "core/parser/ast.h"
#include "core/parser/lexer.h"
#include "core/log.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::core::parser;
using namespace wwiv::strings;

namespace wwiv::sdk::acs {

static std::tuple<std::string, std::string> split_obj_name(const std::string& name) {
  auto last = name.rfind('.');
  if (last == std::string::npos) {
    return std::make_tuple("", name);
  }
  return std::make_tuple(name.substr(0, last), name.substr(last + 1));
}

std::optional<Value> Eval::to_value(Factor* n) {
  switch (n->factor_type()) {
  case FactorType::int_value:
    return Value(n->int_value());
    break;
  case FactorType::string_val:
    return Value(n->value());
  case FactorType::variable: {
    auto [prefix, member] = split_obj_name(n->value());
    auto vpi = providers_.find(prefix);
    if (vpi != std::end(providers_)) {
      if (auto o = vpi->second->value(member)) {
        return {o.value()};
      }
    }

  } break;
  }
  return std::nullopt;
}

Eval::Eval(std::string expression) : expression_(std::move(expression)) {}


void Eval::visit(Expression* n) { 
  VLOG(2) << "Evaluating: " << n->ToString(false);  
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
  VLOG(1) << "EVAL: L: " << left.value() << " " << to_symbol(n->op()) << " " << right.value();

  // cache value
  auto result = Value::eval(left.value(), n->op(), right.value());
  values_[n->id()] = result;
}

void Eval::visit(Factor* n) { 
  if (n->factor_type() == FactorType::variable) { 
    auto [prefix, member] = split_obj_name(n->value());
    auto vpi = providers_.find(prefix);
    if (vpi != std::end(providers_)) {
      if (auto o = vpi->second->value(member)) {
        values_.emplace(n->id(), o.value());
      }
    }
  }
}

bool Eval::eval() {
  VLOG(1) << "Eval:eval: " << expression_;
  Lexer l(expression_);
  if (!l.ok()) {
    LOG(ERROR) << "Failed to lex expression: '" << expression_ << "'";
    return false;
  }

  Ast ast{};
  if (!ast.parse(l)) {
    return false;
  }
  auto* root = ast.root();
  if (!root) {
    LOG(ERROR) << "Failed to parse expression: " << expression_;
    return false;
  }
  VLOG(1) << "Root: " << root->ToString();

  root->accept(this);

  if (auto expr = dynamic_cast<Expression*>(root)) {
    auto it = values_.find(expr->id());
    if (it == std::end(values_)) {
      return false;
    }
    return it->second.as_boolean();
  }
  return false;
}

bool Eval::add(const std::string& prefix, std::unique_ptr<ValueProvider>&& p) { 
  providers_[prefix] = std::move(p);
  return true; 
}


} // namespace wwiv::sdk::acs
