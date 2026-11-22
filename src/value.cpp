#include "value.h"
#include "fmt/format.h"

#include <any>
#include <iostream>
#include <string>

namespace wwivbasic {

Value::Value(const std::any& a) : value_(std::string()), type(Type::STRING) {
  if (!a.has_value()) {
    return;
  }
  if (a.type() == typeid(Value)) {
    *this = std::any_cast<Value>(a);
  } else if (a.type() == typeid(bool)) {
    type = Type::BOOLEAN;
    value_ = std::any_cast<bool>(a);
  } else if (a.type() == typeid(int)) {
    type = Type::INTEGER;
    value_ = std::any_cast<int>(a);
  } else if (a.type() == typeid(std::string)) {
    type = Type::STRING;
    value_ = std::any_cast<std::string>(a);
  } else {
    throw BasicError(fmt::format("Internal error: unexpected value type: {}", a.type().name()));
  }
}

Value Value::new_list() {
  return Value(std::make_shared<ListData>());
}

ListPtr Value::list(std::string_view fn) const {
  if (type != Type::LIST) {
    throw BasicError(fmt::format("{}: expected a LIST, got {}", fn, type_name()));
  }
  return std::get<ListPtr>(value_);
}

IteratorPtr Value::iterator(std::string_view fn) const {
  if (type != Type::ITERATOR) {
    throw BasicError(fmt::format("{}: expected an ITERATOR, got {}", fn, type_name()));
  }
  return std::get<IteratorPtr>(value_);
}

std::string Value::type_name() const {
  switch (type) {
  case Type::BOOLEAN:
    return "BOOLEAN";
  case Type::INTEGER:
    return "INTEGER";
  case Type::STRING:
    return "STRING";
  case Type::LIST:
    return "LIST";
  case Type::ITERATOR:
    return "ITERATOR";
  }
  return "UNKNOWN";
}

bool Value::toBool() const {
  switch (type) {
  case Type::BOOLEAN:
    return std::get<bool>(value_);
  case Type::INTEGER:
    return std::get<int>(value_) != 0;
  case Type::STRING:
    return std::get<std::string>(value_) == "TRUE";
  case Type::LIST:
  case Type::ITERATOR:
    return true;
  }
  return false;
}

int Value::toInt() const {
  switch (type) {
  case Type::BOOLEAN:
    return std::get<bool>(value_) ? 1 : 0;
  case Type::INTEGER:
    return std::get<int>(value_);
  case Type::STRING:
    try {
      return std::stoi(std::get<std::string>(value_));
    } catch (const std::exception&) {
      return 0;
    }
  case Type::LIST:
  case Type::ITERATOR:
    return 0;
  }
  return 0;
}

std::string Value::toString() const {
  switch (type) {
  case Type::BOOLEAN:
    return std::get<bool>(value_) ? "TRUE" : "FALSE";
  case Type::INTEGER:
    return std::to_string(std::get<int>(value_));
  case Type::STRING:
    return std::get<std::string>(value_);
  case Type::LIST:
  case Type::ITERATOR:
    return type_name();
  }
  return {};
}

void Value::check_scalar(const Value& that, std::string_view op) const {
  for (const auto* v : {this, &that}) {
    if (v->type == Type::LIST || v->type == Type::ITERATOR) {
      throw BasicError(fmt::format("Operator '{}' cannot be used with a {}", op, v->type_name()));
    }
  }
}

// The arithmetic and comparison operators dispatch on the left operand's
// type. Each switch lists every Type (no default:) so the compiler flags any
// new Type that isn't handled. LIST and ITERATOR are rejected by
// check_scalar() before the switch, so those cases are unreachable.

[[noreturn]] static void unreachable_type(std::string_view op) {
  throw BasicError(fmt::format("Internal error: unhandled type in operator '{}'", op));
}

Value Value::operator+(const Value& that) const {
  check_scalar(that, "+");
  switch (type) {
  case Type::BOOLEAN:
    return Value(toBool() || that.toBool());
  case Type::INTEGER:
    return Value(toInt() + that.toInt());
  case Type::STRING:
    return Value(toString() + that.toString());
  case Type::LIST:
  case Type::ITERATOR:
    break;
  }
  unreachable_type("+");
}

Value Value::operator-(const Value& that) const {
  check_scalar(that, "-");
  switch (type) {
  case Type::BOOLEAN:
    return Value(!(toBool() && that.toBool()));
  case Type::INTEGER:
    return Value(toInt() - that.toInt());
  case Type::STRING:
    return Value(toString() + that.toString());
  case Type::LIST:
  case Type::ITERATOR:
    break;
  }
  unreachable_type("-");
}

Value Value::operator*(const Value& that) const {
  check_scalar(that, "*");
  switch (type) {
  case Type::BOOLEAN:
    // TRUE * TRUE is 1, an INTEGER (as before lists were added).
    return Value(static_cast<int>(toBool() && that.toBool()));
  case Type::INTEGER:
    return Value(toInt() * that.toInt());
  case Type::STRING:
    return Value(toString() + that.toString());
  case Type::LIST:
  case Type::ITERATOR:
    break;
  }
  unreachable_type("*");
}

Value Value::operator/(const Value& that) const {
  check_scalar(that, "/");
  switch (type) {
  case Type::BOOLEAN:
    return Value(false);
  case Type::INTEGER:
    if (that.toInt() == 0) {
      throw BasicError("Division by zero");
    }
    return Value(toInt() / that.toInt());
  case Type::STRING:
    return Value(toString() + that.toString());
  case Type::LIST:
  case Type::ITERATOR:
    break;
  }
  unreachable_type("/");
}

Value Value::operator%(const Value& that) const {
  check_scalar(that, "MOD");
  switch (type) {
  case Type::BOOLEAN:
    return Value(false);
  case Type::INTEGER:
    if (that.toInt() == 0) {
      throw BasicError("Division by zero");
    }
    return Value(toInt() % that.toInt());
  case Type::STRING:
    return Value(toString() + that.toString());
  case Type::LIST:
  case Type::ITERATOR:
    break;
  }
  unreachable_type("MOD");
}

bool Value::operator<(const Value& that) const {
  check_scalar(that, "<");
  switch (type) {
  case Type::BOOLEAN:
    return toBool() < that.toBool();
  case Type::INTEGER:
    return toInt() < that.toInt();
  case Type::STRING:
    return toString() < that.toString();
  case Type::LIST:
  case Type::ITERATOR:
    break;
  }
  unreachable_type("<");
}

bool Value::operator>(const Value& that) const {
  check_scalar(that, ">");
  switch (type) {
  case Type::BOOLEAN:
    return toBool() > that.toBool();
  case Type::INTEGER:
    return toInt() > that.toInt();
  case Type::STRING:
    return toString() > that.toString();
  case Type::LIST:
  case Type::ITERATOR:
    break;
  }
  unreachable_type(">");
}

bool Value::operator==(const Value& that) const {
  switch (type) {
  case Type::BOOLEAN:
  case Type::INTEGER:
  case Type::STRING:
    if (that.type == Type::LIST || that.type == Type::ITERATOR) {
      return false;
    }
    break;
  case Type::LIST:
  case Type::ITERATOR:
    // Same type and the same underlying object.
    return type == that.type && value_ == that.value_;
  }
  switch (type) {
  case Type::BOOLEAN:
    return toBool() == that.toBool();
  case Type::INTEGER:
    return toInt() == that.toInt();
  case Type::STRING:
    return toString() == that.toString();
  case Type::LIST:
  case Type::ITERATOR:
    break;
  }
  unreachable_type("=");
}

std::ostream& operator<<(std::ostream& os, const Value& v) {
  os << v.toString();
  return os;
}

} // namespace wwivbasic
