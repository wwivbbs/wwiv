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
#include "sdk/acs/eval_error.h"
#include <string>

using namespace wwiv::core;
using namespace parser;
using namespace wwiv::strings;

namespace wwiv::sdk::acs {

// TODO(rushfan): Move this to sdk, this is from bbs/arword.cpp
static std::string word_to_arstr(int ar) {
  if (!ar) {
    return {};
  }
  std::string arstr;
  for (auto i = 0; i < 16; i++) {
    if ((1 << i) & ar) {
      arstr.push_back(static_cast<char>('A' + i));
    }
  }
  return arstr;
}

static uint16_t str_to_arword(const std::string& arstr) {
  uint16_t rar = 0;
  const auto s = ToStringUpperCase(arstr);

  for (auto i = 0; i < 16; i++) {
    if (s.find(static_cast<char>(i + 'A')) != std::string::npos) {
      rar |= (1 << i);
    }
  }
  return rar;
}

Ar::Ar(int ar, bool user_side) : ar_(static_cast<uint16_t>(ar)), user_side_(user_side) {}

Ar::Ar(char ar) : ar_(static_cast<uint16_t>(1 << (ar - 'A'))) {}

Ar::Ar(const std::string& ar, bool user_side) : ar_(str_to_arword(ar)), user_side_(user_side) {}

Ar::Ar(const std::string& ar) : ar_(str_to_arword(ar)) {}


bool Ar::eq(const Ar& that) const {
  if (user_side_ && that.user_side_) {
    DLOG(FATAL) << "Can not have user side ar on both sides.";
    LOG(ERROR) << "Can not have user side ar on both sides.";
    return false;
  }
  if (user_side_) {
    return that.ar_ == 0 || ar_ & that.ar_;
  }
  if (that.user_side_) {
    return that.eq(*this);
  }
  DLOG(FATAL) << "Ar::eq called with no user side.";
  LOG(ERROR) << "Ar::eq called with no user side.";
  // Neither is a user side, so return exact match.
  // Always return true if either side allows everything.
  return ar_ & that.ar_;
}

std::string Ar::as_string() const { return fmt::format("Ar({})", word_to_arstr(ar_));  }

int Ar::as_integer() const { return ar_; }

std::ostream& operator<<(std::ostream& os, const Ar& a) {
  os << a.as_string();
  return os;
}


std::ostream& operator<<(std::ostream& os, const Value& a) {
  if (a.value_type == ValueType::number) {
    os << std::any_cast<int>(a.value_);
  } else if (a.value_type == ValueType::string) {
    os << std::any_cast<std::string>(a.value_);
  } else if (a.value_type == ValueType::boolean) {
    os << (std::any_cast<bool>(a.value_) ? "true" : "false");
  } else if (a.value_type == ValueType::ar) {
    os << std::any_cast<Ar>(a.value_);
  } else {
    os << "[unknown value_ of type: " << a.value_.type().name() << "]";
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
    break;
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
  case Operator::eq: {
    if (vt == ValueType::number) {
      return Value(l.as_number() == r.as_number());
    }
    if (vt == ValueType::boolean) {
      return Value(l.as_boolean() == r.as_boolean());
    }
    if (vt == ValueType::string) {
      return Value(iequals(l.as_string(), r.as_string()));
    }
    if (vt == ValueType::ar) {
      return Value(l.as_ar() == r.as_ar());
    }
  }
  break;
  case Operator::ne: {
    if (vt == ValueType::number) {
      return Value(l.as_number() != r.as_number());
    }
    if (vt == ValueType::boolean) {
      return Value(l.as_boolean() != r.as_boolean());
    }
    if (vt == ValueType::string) {
      return Value(!iequals(l.as_string(), r.as_string()));
    }
    if (vt == ValueType::ar) {
      return Value(l.as_ar() != r.as_ar());
    }
  }
  break;
  case Operator::logical_or:
    return Value(l.as_boolean() || r.as_boolean());
  case Operator::logical_and:
    return Value(l.as_boolean() && r.as_boolean());
  case Operator::UNKNOWN:
    return Value(false);
  }
  return Value(false);
}


int Value::as_number() {
  switch (value_type) {
  case ValueType::number:
    return std::any_cast<int>(value_);
  case ValueType::string:
    return to_number<int>(std::any_cast<std::string>(value_));
  case ValueType::boolean: {
    const auto b = std::any_cast<bool>(value_);
    return b ? 1 : 0;
  }
  case ValueType::ar:
    return std::any_cast<Ar>(value_).as_integer();
  case ValueType::unknown:
    return 0;
  }
  return 0;
}

std::string Value::as_string() {
  switch (value_type) {
  case ValueType::number:
    return std::to_string(std::any_cast<int>(value_));
  case ValueType::string:
    return std::any_cast<std::string>(value_);
  case ValueType::boolean: {
    const auto b = std::any_cast<bool>(value_);
    return b ? "true" : "false";
  }
  case ValueType::ar:
    return std::any_cast<Ar>(value_).as_string();
  case ValueType::unknown:
    DLOG(FATAL) << "unknown value type: " << static_cast<int>(value_type);
    return "";
  }
  DLOG(FATAL) << "unknown value type: " << static_cast<int>(value_type);
  return "";
}

bool Value::as_boolean() {
  switch (value_type) {
  case ValueType::string: {
    const auto s = std::any_cast<std::string>(value_);
    return iequals(s, "true");
  }
  case ValueType::number:
    return std::any_cast<int>(value_) != 0;
  case ValueType::boolean:
    return std::any_cast<bool>(value_);
  case ValueType::ar:
  case ValueType::unknown:
    return false;
  }
  return false;
}

Ar Value::as_ar() {
  switch (value_type) {
  case ValueType::ar:
    return std::any_cast<Ar>(value_);
  case ValueType::boolean:
  case ValueType::number:
    // This is not a user value since it didn't come from
    // the UserProvider.
    return Ar(0, false);
  case ValueType::string:
    return Ar(std::any_cast<std::string>(value_));
  case ValueType::unknown:
    DLOG(FATAL) << "ValueType::unknown";
    break;
  }
  throw eval_error(fmt::format("Unable to coerce valuetype: {} to Ar", static_cast<int>(value_type)));
}


}