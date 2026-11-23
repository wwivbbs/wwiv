#include "wwivbasic/value.h"
#include "wwivbasic/object.h"
#include "fmt/format.h"

#include <any>
#include <cmath>
#include <cctype>
#include <charconv>
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

Value Value::make_list(std::vector<Value> items) {
  auto l = new_list();
  l.list("make_list")->items = std::move(items);
  return l;
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

ObjectPtr Value::object(std::string_view fn) const {
  if (type != Type::OBJECT) {
    throw BasicError(fmt::format("{}: expected an object, got {}", fn, a_type_name()));
  }
  return std::get<ObjectPtr>(value_);
}

std::string Value::a_type_name() const {
  if (type == Type::OBJECT) {
    // Class names are the embedder's, so don't guess "a" or "an".
    return "an object of class " + type_name();
  }
  // The built-in type names; only INTEGER and ITERATOR start with a vowel.
  const auto name = type_name();
  const bool vowel =
      !name.empty() && std::string_view("AEIOU").find(name.front()) != std::string_view::npos;
  return (vowel ? "an " : "a ") + name;
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
  case Type::OBJECT:
    return std::get<ObjectPtr>(value_)->cls->name;
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
  case Type::OBJECT:
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
  case Type::OBJECT:
    return true;
  }
  return false;
}

std::optional<double> Value::parse_number(std::string_view s) {
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
    i++;
  }
  bool negative = false;
  if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
    negative = s[i] == '-';
    i++;
  }
  // Only decimal digits or a '.' can start a number; from_chars would also
  // accept "inf" and "nan".
  if (i >= s.size() || !(std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.')) {
    return std::nullopt;
  }
  double d{};
  const auto [ptr, ec] = std::from_chars(s.data() + i, s.data() + s.size(), d);
  if (ec != std::errc() || !std::isfinite(d)) {
    return std::nullopt;
  }
  return negative ? -d : d;
}

namespace {

// Formats a REAL. Whole numbers with up to 10 digits print in full
// (3000000000), since timestamps and file sizes are common; others print
// like printf("%g") (0.5, 3.14159, 1e+10), as in my_basic.
std::string format_real(double d) {
  if (std::trunc(d) == d && std::fabs(d) < 1e10) {
    return fmt::format("{:.0f}", d);
  }
  return fmt::format("{:g}", d);
}

// Truncates toward zero and clamps to the INTEGER range.
int clamp_to_int(double d) {
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

} // namespace

int Value::toInt() const {
  switch (type) {
  case Type::BOOLEAN:
    return std::get<bool>(value_) ? 1 : 0;
  case Type::INTEGER:
    return std::get<int>(value_);
  case Type::REAL:
    return clamp_to_int(std::get<double>(value_));
  case Type::STRING:
    return clamp_to_int(parse_number(std::get<std::string>(value_)).value_or(0));
  case Type::LIST:
  case Type::ITERATOR:
  case Type::OBJECT:
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
  case Type::STRING:
    return parse_number(std::get<std::string>(value_)).value_or(0);
  case Type::LIST:
  case Type::ITERATOR:
  case Type::OBJECT:
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
    return format_real(std::get<double>(value_));
  case Type::STRING:
    return std::get<std::string>(value_);
  case Type::LIST:
  case Type::ITERATOR:
  case Type::OBJECT:
    return type_name();
  }
  return {};
}

void Value::check_scalar(const Value& that, std::string_view op) const {
  for (const auto* v : {this, &that}) {
    if (v->type == Type::LIST || v->type == Type::ITERATOR || v->type == Type::OBJECT) {
      throw BasicError(fmt::format("Operator '{}' cannot be used with {}", op, v->a_type_name()));
    }
  }
}

// Each switch lists every Type (no default:) so the compiler flags any new
// Type that isn't handled. LIST, ITERATOR and OBJECT are rejected by check_scalar()
// before the switch, so those cases are unreachable.

[[noreturn]] static void unreachable_type(std::string_view op) {
  throw BasicError(fmt::format("Internal error: unhandled type in operator '{}'", op));
}

namespace {

// True if v is an INTEGER or BOOLEAN, or a STRING holding a whole number that
// fits in an INTEGER. Arithmetic on two of these stays in integers.
bool is_integral(const Value& v) {
  switch (v.value_type()) {
  case Value::Type::BOOLEAN:
  case Value::Type::INTEGER:
    return true;
  case Value::Type::REAL:
  case Value::Type::LIST:
  case Value::Type::ITERATOR:
  case Value::Type::OBJECT:
    return false;
  case Value::Type::STRING: {
    const auto d = v.toDouble();
    // Whole and small enough for an INTEGER; "1e30" is not.
    return std::trunc(d) == d && d >= std::numeric_limits<int>::min() &&
           d <= std::numeric_limits<int>::max();
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
    const long long y = b.toInt();
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

// True if a comparison should compare numbers: the left side is an INTEGER or
// REAL (a STRING on the right is read as a number, as in arithmetic), or a
// BOOLEAN compared with a REAL. Otherwise the left side's type decides.
bool compare_numerically(const Value& a, const Value& b) {
  switch (a.value_type()) {
  case Value::Type::INTEGER:
  case Value::Type::REAL:
    return true;
  case Value::Type::BOOLEAN:
    return b.value_type() == Value::Type::REAL;
  case Value::Type::STRING:
  case Value::Type::LIST:
  case Value::Type::ITERATOR:
  case Value::Type::OBJECT:
    return false;
  }
  return false;
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
  case Type::OBJECT:
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
  case Type::OBJECT:
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
  case Type::OBJECT:
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
  case Type::OBJECT:
    break;
  }
  unreachable_type("/");
}

// MOD works on integers: REALs are truncated first, like my_basic, so
// 7.5 MOD 0.5 is a division by zero there too.
Value Value::operator%(const Value& that) const {
  check_scalar(that, "MOD");
  switch (type) {
  case Type::BOOLEAN:
    return Value(false);
  case Type::INTEGER:
  case Type::REAL: {
    // 64 bits, so INT_MIN MOD -1 doesn't overflow.
    const long long x = toInt();
    const long long y = that.toInt();
    if (y == 0) {
      throw BasicError("Division by zero");
    }
    return Value(static_cast<int>(x % y));
  }
  case Type::STRING:
    return Value(toString() + that.toString());
  case Type::LIST:
  case Type::ITERATOR:
  case Type::OBJECT:
    break;
  }
  unreachable_type("MOD");
}

bool Value::operator<(const Value& that) const {
  check_scalar(that, "<");
  if (compare_numerically(*this, that)) {
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
  case Type::OBJECT:
    break;
  }
  unreachable_type("<");
}

bool Value::operator>(const Value& that) const {
  check_scalar(that, ">");
  if (compare_numerically(*this, that)) {
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
  case Type::OBJECT:
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
    if (that.type == Type::LIST || that.type == Type::ITERATOR || that.type == Type::OBJECT) {
      return false;
    }
    break;
  case Type::LIST:
  case Type::ITERATOR:
    // Same type and the same underlying list.
    return type == that.type && value_ == that.value_;
  case Type::OBJECT: {
    // The same C++ object, even if it was wrapped twice.
    if (that.type != Type::OBJECT) {
      return false;
    }
    const auto& a = std::get<ObjectPtr>(value_);
    const auto& b = std::get<ObjectPtr>(that.value_);
    return a && b && a->self.get() == b->self.get() && a->cls == b->cls;
  }
  }
  if (compare_numerically(*this, that)) {
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
  case Type::OBJECT:
    break;
  }
  unreachable_type("=");
}

std::ostream& operator<<(std::ostream& os, const Value& v) {
  os << v.toString();
  return os;
}

} // namespace wwivbasic
