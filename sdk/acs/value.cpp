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


int Value::as_number() {
  if (value_type == ValueType::number) {
    return std::any_cast<int>(value);
  } else if (value_type == ValueType::string) {
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
  } else if (value_type == ValueType::string) {
    return std::any_cast<std::string>(value);
  } else if (value_type == ValueType::boolean) {
    const auto b = std::any_cast<bool>(value);
    return b ? "true" : "false";
  }
  DLOG(FATAL) << "unknown value type: " << static_cast<int>(value_type);
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

}