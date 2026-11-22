#include "wwivbasic/value.h"
#include "fmt/format.h"

#include <any>
#include <cmath>
#include <cstdlib>
#include <limits>
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
  } else if (a.type() == typeid(double)) {
    type = Type::REAL;
    value_ = std::any_cast<double>(a);
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

Value Value::number(double d) {
  if (std::trunc(d) == d && d >= std::numeric_limits<int>::min() &&
      d <= std::numeric_limits<int>::max()) {
    return Value(static_cast<int>(d));
  }
  return Value(d);
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
  case Type::REAL:
    return "REAL";
  case Type::STRING:
    return "STRING";
  case Type::LIST:
    return "LIST";
  case Type::ITERATOR:
    return "ITERATOR";
  }
  return "UNKNOWN";
}

bool Value::is_number() const {
  switch (type) {
  case Type::BOOLEAN:
  case Type::INTEGER:
  case Type::REAL:
    return true;
  case Type::STRING:
  case Type::LIST:
  case Type::ITERATOR:
    return false;
  }
  return false;
}

bool Value::toBool() const {
  switch (type) {
  case Type::BOOLEAN:
    return std::get<bool>(value_);
  case Type::INTEGER:
    return std::get<int>(value_) != 0;
  case Type::REAL:
    return std::get<double>(value_) != 0;
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
  case Type::REAL: {
    const auto d = std::get<double>(value_);
    if (std::isnan(d)) {
      return 0;
    }
    if (d >= static_cast<double>(std::numeric_limits<int>::max())) {
      return std::numeric_limits<int>::max();
    }
    if (d <= static_cast<double>(std::numeric_limits<int>::min())) {
      return std::numeric_limits<int>::min();
    }
    return static_cast<int>(d);
  }
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

double Value::toDouble() const {
  switch (type) {
  case Type::BOOLEAN:
    return std::get<bool>(value_) ? 1 : 0;
  case Type::INTEGER:
    return std::get<int>(value_);
  case Type::REAL:
    return std::get<double>(value_);
  case Type::STRING: {
    const auto& s = std::get<std::string>(value_);
    return std::strtod(s.c_str(), nullptr);
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
  case Type::REAL:
    return fmt::format("{:g}", std::get<double>(value_));
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

// Each switch lists every Type (no default:) so the compiler flags any new
// Type that isn't handled. LIST and ITERATOR are rejected by check_scalar()
// before the switch, so those cases are unreachable.

[[noreturn]] static void unreachable_type(std::string_view op) {
  throw BasicError(fmt::format("Internal error: unhandled type in operator '{}'", op));
}

namespace {

// True if v is an INTEGER or BOOLEAN, or a STRING holding a whole number:
// arithmetic on two of these stays in integers.
bool is_integral(const Value& v) {
  switch (v.value_type()) {
  case Value::Type::BOOLEAN:
  case Value::Type::INTEGER:
    return true;
  case Value::Type::REAL:
  case Value::Type::LIST:
  case Value::Type::ITERATOR:
    return false;
  case Value::Type::STRING: {
    const auto d = v.toDouble();
    return std::trunc(d) == d;
  }
  }
  return false;
}

// + - * / for a numeric left operand. Integer arithmetic is done in 64 bits,
// and results that don't fit in an INTEGER become REALs. `/` of two integers
// is exact (7 / 2 is 3.5, 6 / 2 is 3), like my_basic.
Value arithmetic(const Value& a, const Value& b, char op) {
  if (a.value_type() == Value::Type::INTEGER && is_integral(b)) {
    const long long x = a.toInt();
    const long long y = static_cast<long long>(b.toDouble());
    long long r = 0;
    switch (op) {
    case '+':
      r = x + y;
      break;
    case '-':
      r = x - y;
      break;
    case '*':
      r = x * y;
      break;
    case '/':
      if (y == 0) {
        throw BasicError("Division by zero");
      }
      if (x % y != 0) {
        return Value::number(static_cast<double>(x) / static_cast<double>(y));
      }
      r = x / y;
      break;
    }
    return Value::number(static_cast<double>(r));
  }
  const auto x = a.toDouble();
  const auto y = b.toDouble();
  switch (op) {
  case '+':
    return Value::number(x + y);
  case '-':
    return Value::number(x - y);
  case '*':
    return Value::number(x * y);
  case '/':
    if (y == 0) {
      throw BasicError("Division by zero");
    }
    return Value::number(x / y);
  }
  throw BasicError(fmt::format("Internal error: unknown operator '{}'", op));
}

// True if comparisons between a and b should be numeric: both are numbers
// and at least one is a REAL (INTEGER and BOOLEAN keep their existing rules).
bool compare_as_reals(const Value& a, const Value& b) {
  return a.is_number() && b.is_number() &&
         (a.value_type() == Value::Type::REAL || b.value_type() == Value::Type::REAL);
}

} // namespace

Value Value::operator+(const Value& that) const {
  check_scalar(that, "+");
  switch (type) {
  case Type::BOOLEAN:
    return Value(toBool() || that.toBool());
  case Type::INTEGER:
  case Type::REAL:
    return arithmetic(*this, that, '+');
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
  case Type::REAL:
    return arithmetic(*this, that, '-');
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
  case Type::REAL:
    return arithmetic(*this, that, '*');
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
  case Type::REAL:
    return arithmetic(*this, that, '/');
  case Type::STRING:
    return Value(toString() + that.toString());
  case Type::LIST:
  case Type::ITERATOR:
    break;
  }
  unreachable_type("/");
}

// MOD works on integers; REALs are truncated first, like my_basic.
Value Value::operator%(const Value& that) const {
  check_scalar(that, "MOD");
  switch (type) {
  case Type::BOOLEAN:
    return Value(false);
  case Type::INTEGER:
  case Type::REAL:
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
  if (compare_as_reals(*this, that)) {
    return toDouble() < that.toDouble();
  }
  switch (type) {
  case Type::BOOLEAN:
    return toBool() < that.toBool();
  case Type::INTEGER:
    return toInt() < that.toInt();
  case Type::REAL:
    return toDouble() < that.toDouble();
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
  if (compare_as_reals(*this, that)) {
    return toDouble() > that.toDouble();
  }
  switch (type) {
  case Type::BOOLEAN:
    return toBool() > that.toBool();
  case Type::INTEGER:
    return toInt() > that.toInt();
  case Type::REAL:
    return toDouble() > that.toDouble();
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
  case Type::REAL:
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
  if (compare_as_reals(*this, that)) {
    return toDouble() == that.toDouble();
  }
  switch (type) {
  case Type::BOOLEAN:
    return toBool() == that.toBool();
  case Type::INTEGER:
    return toInt() == that.toInt();
  case Type::REAL:
    return toDouble() == that.toDouble();
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
