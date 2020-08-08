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
#ifndef __INCLUDED_SDK_ACS_EVAL_H__
#define __INCLUDED_SDK_ACS_EVAL_H__

#include "core/parser/ast.h"
#include "core/parser/lexer.h"
#include "sdk/user.h"
#include <any>
#include <iostream>
#include <map>
#include <unordered_map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wwiv::sdk::acs {

  enum class ValueType { unknown, number, string, boolean };
class Value {
public:
  Value() : value_type(ValueType::unknown) {}
  Value(bool v) : value_type(ValueType::boolean), value(std::make_any<bool>(v)) {}
  Value(int v) : value_type(ValueType::number), value(std::make_any<int>(v)) {}
  Value(std::string v) : value_type(ValueType::string), value(std::make_any<std::string>(v)) {}

  int as_number();
  std::string as_string();
  bool as_boolean();

  ValueType value_type;
  std::any value;

};

std::ostream& operator<<(std::ostream& os, const Value& a);

/** Provides a value for an identifier of the form "prefix.value" */
class ValueProvider {
public:
  ValueProvider(const std::string& prefix) : prefix_(prefix) {}
  virtual ~ValueProvider() = default;

  virtual std::optional<Value> value(const std::string& name) = 0;

private:
  const std::string prefix_;
};

class UserValueProvider : public ValueProvider {
public:
  UserValueProvider(wwiv::sdk::User* user) : ValueProvider("user"), user_(user) {}
  std::optional<Value> value(const std::string& name) override;

private:
  wwiv::sdk::User* user_;
};

class Eval : public wwiv::core::parser::AstVisitor {
public:
  explicit Eval(std::string expression);
  ~Eval() = default;

  bool eval();
  bool add(const std::string& prefix, std::unique_ptr<ValueProvider>&& p);
  std::optional<Value> to_value(wwiv::core::parser::Factor* n);

  virtual void visit(wwiv::core::parser::AstNode*) override {}
  virtual void visit(wwiv::core::parser::Expression* n) override;
  virtual void visit(wwiv::core::parser::Factor* n) override;

private:
  std::string expression_;
  std::map<std::string, std::unique_ptr<ValueProvider>> providers_;
  std::unordered_map<int, Value> values_;
};  // class

} 

#endif // __INCLUDED_SDK_FILES_TIC_H__