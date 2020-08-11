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
#include "sdk/acs/value.h"

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

static uint16_t str_to_arword(const std::string& arstr) {
  uint16_t rar = 0;
  auto s = ToStringUpperCase(arstr);

  for (int i = 0; i < 16; i++) {
    if (s.find(static_cast<char>(i + 'A')) != std::string::npos) {
      rar |= (1 << i);
    }
  }
  return rar;
}

Ar::Ar(int ar) : ar_(static_cast<uint16_t>(ar)) {}

Ar::Ar(char ar) : ar_(static_cast<uint16_t>(1 << (ar - 'A'))) {}

Ar::Ar(const std::string ar) : ar_(str_to_arword(ar)) {}

bool Ar::eq(const Ar& that) const {
  // Always return true if either side allows everything.
  return ar_ == 0 || that.ar_ == 0 || (ar_ & that.ar_);
}

std::string Ar::as_string() const { return word_to_arstr(ar_);  }

int Ar::as_integer() const { return ar_; }


std::ostream& operator<<(std::ostream& os, const Value& a) {
  if (a.value_type == ValueType::number) {
    os << std::any_cast<int>(a.value);
  } else if (a.value_type == ValueType::string) {
    os << std::any_cast<std::string>(a.value);
  } else if (a.value_type == ValueType::boolean) {
    os << std::any_cast<bool>(a.value) ? "true" : "false";
  } else {
    os << "[unknown value of type: " << a.value.type().name() << "/" << a.value.type().raw_name()
       << "]";
  }
  return os;
}

//static 
Value Value::eval(Value l, Operator op, Value r) {
  const auto vt = l.value_type;
  const auto rvt = r.value_type;
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
    } else if (vt == ValueType::ar) {
      return Value(l.as_ar() == r.as_ar());
    }
    break;
  case Operator::ne:
    if (vt == ValueType::number) {
      return Value(l.as_number() != r.as_number());
    } else if (vt == ValueType::boolean) {
      return Value(l.as_boolean() != r.as_boolean());
    } else if (vt == ValueType::string) {
      return Value(!iequals(l.as_string(), r.as_string()));
    } else if (vt == ValueType::ar) {
      return Value(l.as_ar() != r.as_ar());
    }
    break;
  case Operator::logical_or:
    return Value(l.as_boolean() || r.as_boolean());
  case Operator::logical_and:
    return Value(l.as_boolean() && r.as_boolean());
  }
  return Value(false);
}


int Value::as_number() {
  switch (value_type) {
  case ValueType::number:
    return std::any_cast<int>(value);
  case ValueType::string:
    return to_number<int>(std::any_cast<std::string>(value));
  case ValueType::boolean: {
    const auto b = std::any_cast<bool>(value);
    return b ? 1 : 0;
  }
  case ValueType::ar:
    return std::any_cast<Ar>(value).as_integer();
  default:
    return 0;
  }
}

std::string Value::as_string() {
  switch (value_type) {
  case ValueType::number:
    return std::to_string(std::any_cast<int>(value));
  case ValueType::string:
    return std::any_cast<std::string>(value);
  case ValueType::boolean: {
    const auto b = std::any_cast<bool>(value);
    return b ? "true" : "false";
  }
  case ValueType::ar:
    return std::any_cast<Ar>(value).as_string();
    break;
  default:
    DLOG(FATAL) << "unknown value type: " << static_cast<int>(value_type);
    return "";
  }
}

bool Value::as_boolean() {
  switch (value_type) {
  case ValueType::string: {
    const auto s = std::any_cast<std::string>(value);
    return iequals(s, "true");
  }
  case ValueType::number:
    return std::any_cast<int>(value) != 0;
  case ValueType::boolean:
    return std::any_cast<bool>(value);
  case ValueType::ar:
    return false;
  default:
    return false;
  }
}

Ar Value::as_ar() {
  switch (value_type) {
  case ValueType::ar:
    return std::any_cast<Ar>(value);
  case ValueType::boolean:
    return Ar(0);
  case ValueType::number:
    return Ar(0);
  case ValueType::string:
    return Ar(std::any_cast<std::string>(value));
  case ValueType::unknown:
    DLOG(FATAL) << "ValueType::unknown";
    break;
  }
}


}