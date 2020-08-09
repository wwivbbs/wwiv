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
#include "sdk/user.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::core::parser;
using namespace wwiv::strings;

namespace wwiv::sdk::acs {

std::tuple<std::string, std::string> split_obj_name(const std::string& name) {
  auto last = name.rfind('.');
  if (last == std::string::npos) {
    return std::make_tuple("", name);
  }
  return std::make_tuple(name.substr(0, last), name.substr(last + 1));
}

std::ostream& operator<<(std::ostream& os, const Value& a) {
  if (a.value_type == ValueType::number) {
    os << std::any_cast<int>(a.value);
  } else if (a.value_type == ValueType::number) {
    os << std::any_cast<std::string>(a.value);
  }
  return os;
}

/////////////////////////////////////////////////////////////////////////////
// Providers

// TODO(rushfan): Move this to sdk, this is from bbs/arword.cpp
static std::string word_to_arstr(int ar) {
  if (!ar) {
    return {};
  }
  std::string arstr;
  for (int i = 0; i < 16; i++) {
    if ((1 << i) & ar) {
      arstr.push_back(static_cast<char>('A' + i));
    }
  }
  return arstr;
}

std::optional<Value> UserValueProvider::value(const std::string& name) { 
  if (iequals(name, "sl")) {
    return {Value(user_->GetSl())};
  } else if (iequals(name, "dsl")) {
    return {Value(user_->GetDsl())};
  } else if (iequals(name, "ar")) {
    return Value(word_to_arstr(user_->GetAr()));
  }
  return std::nullopt;
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

static Value evalToBool(Value l, Operator op, Value r) {
  const auto vt = l.value_type;
  switch (op) { 
  case Operator::add:
    if (vt == ValueType::number) {
      return Value(l.as_number() + r.as_number());
    }
    return Value(StrCat(l.as_string(), r.as_string()));
  case Operator::sub:
    if (vt == ValueType::number) {
      return Value(l.as_number() - r.as_number());
    }
    LOG(ERROR) << to_string(op) << " is only allowed on numbers";
  case Operator::mul:
    if (vt == ValueType::number) {
      return Value(l.as_number() * r.as_number());
    }
    LOG(ERROR) << to_string(op) << " is only allowed on numbers";
    break;
  case Operator::div:
    if (vt == ValueType::number) {
      return Value(l.as_number() / r.as_number());
    }
    LOG(ERROR) << to_string(op) << " is only allowed on numbers";
    break;
  case Operator::gt:
    if (vt == ValueType::number) {
      return Value(l.as_number() > r.as_number());
    }
    LOG(ERROR) << to_string(op) << " is only allowed on numbers";
    break;
  case Operator::ge:
    if (vt == ValueType::number) {
      return Value(l.as_number() >= r.as_number());
    }
    LOG(ERROR) << to_string(op) << " is only allowed on numbers";
    break;
  case Operator::lt:
    if (vt == ValueType::number) {
      return Value(l.as_number() < r.as_number());
    }
    LOG(ERROR) << to_string(op) << " is only allowed on numbers";
    break;
  case Operator::le:
    if (vt == ValueType::number) {
      return Value(l.as_number() <= r.as_number());
    }
    LOG(ERROR) << to_string(op) << " is only allowed on numbers";
    break;
  case Operator::eq:
    if (vt == ValueType::number) {
      return Value(l.as_number() == r.as_number());
    } else if (vt == ValueType::boolean) {
      return Value(l.as_boolean() == r.as_boolean());
    } else if (vt == ValueType::string) {
      return Value(iequals(l.as_string(), r.as_string()));
    }
    break;
  case Operator::ne:
    if (vt == ValueType::number) {
      return Value(l.as_number() != r.as_number());
    } else if (vt == ValueType::boolean) {
      return Value(l.as_boolean() != r.as_boolean());
    } else if (vt == ValueType::string) {
      return Value(!iequals(l.as_string(), r.as_string()));
    }
    break;
  case Operator::logical_or:
    return Value(l.as_boolean() || r.as_boolean());
  case Operator::logical_and:
    return Value(l.as_boolean() && r.as_boolean());
  }
  return Value(false);
}

void Eval::visit(Expression* n) { 
  LOG(INFO) << "Evaluating: " << n->ToString(false);  
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
  LOG(INFO) << "EVAL: " << left.value() << " " << to_symbol(n->op()) << " " << right.value();

  // cache value
  auto result = evalToBool(left.value(), n->op(), right.value());
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
  Lexer l(expression_);
  if (!l.ok()) {
    LOG(ERROR) << "Failed to lex expression: " << expression_;
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


int Value::as_number() { 
  if (value_type == ValueType::number) {
    return std::any_cast<int>(value);
  } else if (value_type == ValueType::number) {
    return to_number<int>(std::any_cast<std::string>(value));
  } else if (value_type == ValueType::boolean) {
    const auto b = std::any_cast<bool>(value);
    return b ? 1 : 0;
  }
  return 0; 
}

std::string Value::as_string() { 
  if (value_type == ValueType::number) {
    return std::to_string(std::any_cast<int>(value));
  } else if (value_type == ValueType::number) {
    return std::any_cast<std::string>(value);
  } else if (value_type == ValueType::boolean) {
    const auto b = std::any_cast<bool>(value);
    return b ? "true" : "false";
  }
  return "";
}

bool Value::as_boolean() { 
  if (value_type == ValueType::string) {
    const auto s = std::any_cast<std::string>(value);
    return iequals(s, "true");
  } else if (value_type == ValueType::number) {
    return std::any_cast<int>(value) != 0;
  } else if (value_type == ValueType::boolean) {
    return std::any_cast<bool>(value);
  }
  return false;
}

} // namespace wwiv::sdk::acs
