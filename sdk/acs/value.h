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
#ifndef __INCLUDED_SDK_ACS_VALUE_H__
#define __INCLUDED_SDK_ACS_VALUE_H__

#include "core/parser/ast.h"
#include "core/parser/lexer.h"
#include "sdk/user.h"
#include <any>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
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

  static Value eval(Value l, wwiv::core::parser::Operator op, Value r);

  ValueType value_type;
  std::any value;
};

std::ostream& operator<<(std::ostream& os, const Value& a);

}

#endif // __INCLUDED_SDK_ACS_EVAL_H__