/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*           Copyright (C)2020-2021, WWIV Software Services               */
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
#ifndef INCLUDED_SDK_ACS_VALUE_H
#define INCLUDED_SDK_ACS_VALUE_H

#include "core/parser/ast.h"
#include "sdk/user.h"
#include <any>
#include <iostream>
#include <memory>
#include <string>

namespace wwiv::sdk::value {

class Ar {
public:
  // AR set of multiple AR values.  If user_side is true then
  // This AR set represents the set of AR values possessed
  // and the other side represents the AR values required.
  Ar(int ar, bool user_side);
  // Don't be explicit so that we can let the equality operators
  // work easily below.
  // ReSharper disable once CppNonExplicitConvertingConstructor
  Ar(char ar);
  // ReSharper disable once CppNonExplicitConvertingConstructor
  Ar(const std::string& ar, bool user_side);
  // ReSharper disable once CppNonExplicitConvertingConstructor
  Ar(const std::string& ar);
  // ReSharper disable once CppNonExplicitConvertingConstructor
  Ar(const char* ar) : Ar(std::string(ar)) {}

  [[nodiscard]] bool eq(const Ar& that) const;
  [[nodiscard]] std::string as_string() const;
  [[nodiscard]] int as_integer() const;

  uint16_t ar_;
  bool user_side_{false};
};

inline bool operator==(const Ar& lhs, const Ar& rhs) { return lhs.eq(rhs); }
inline bool operator!=(const Ar& lhs, const Ar& rhs) { return !lhs.eq(rhs); }
std::ostream& operator<<(std::ostream& os, const Ar& a);


enum class ValueType { unknown, number, string, boolean, ar };

class Value {
public:
  Value() : value_type(ValueType::unknown) {}
  explicit Value(bool v) : value_type(ValueType::boolean), value_(std::make_any<bool>(v)) {}
  explicit Value(int v) : value_type(ValueType::number), value_(std::make_any<int>(v)) {}
  explicit Value(unsigned v)
      : value_type(ValueType::number), value_(std::make_any<int>(static_cast<int>(v))) {}
  explicit Value(const std::string& v) : value_type(ValueType::string), value_(std::make_any<std::string>(v)) {}
  explicit Value(const char* v) : Value(std::string(v)) {}
  explicit Value(Ar a) : value_type(ValueType::ar), value_(std::make_any<Ar>(a)) {}

  [[nodiscard]] int as_number();
  [[nodiscard]] std::string as_string();
  [[nodiscard]] bool as_boolean();
  [[nodiscard]] Ar as_ar();

  [[nodiscard]] bool is_boolean() const { return value_type == ValueType::boolean; }

  [[nodiscard]] static Value eval(Value l, wwiv::core::parser::Operator op, Value r);

  friend bool operator==(const Ar& lhs, const Ar& rhs);
  friend bool operator!=(const Ar& lhs, const Ar& rhs);
  friend std::ostream& operator<<(std::ostream& os, const Value& a);

private: 
  ValueType value_type;
  std::any value_;
};

std::ostream& operator<<(std::ostream& os, const Value& a);

}

#endif
