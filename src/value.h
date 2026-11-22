#pragma once

#include "fmt/format.h"

#include <any>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace wwivbasic {

// An error raised while running a script. The executor fills in the source
// location of the statement being executed if it was not already set.
class BasicError : public std::runtime_error {
public:
  explicit BasicError(const std::string& msg) : std::runtime_error(msg) {}
  std::string file;
  size_t line{0};
};

class Value {
public:
  enum class Type { BOOLEAN, INTEGER, STRING };

  Value() : value_(std::string()), type(Type::STRING) {}
  explicit Value(bool b) : value_(b), type(Type::BOOLEAN) {}
  explicit Value(int i) : value_(i), type(Type::INTEGER) {}
  explicit Value(const std::string& s) : value_(s), type(Type::STRING) {}
  explicit Value(const char* s) : value_(std::string(s)), type(Type::STRING) {}
  explicit Value(const std::any& a);

  int set(int i) {
    value_ = i;
    type = Type::INTEGER;
    return i;
  }

  std::string set(std::string_view sv) {
    auto s = std::string(sv);
    value_ = s;
    type = Type::STRING;
    return s;
  }

  Type value_type() const { return type; }

  bool toBool() const;
  int toInt() const;
  std::string toString() const;
  std::any toAny() const;

  template <typename T> 
  T get() const { return std::get<T>(value_); }

  // operators
  Value operator+(const Value& that) const;
  Value operator-(const Value& that) const;
  Value operator*(const Value& that) const;
  Value operator/(const Value& that) const;
  Value operator%(const Value& that) const;

  Value operator||(const Value& that) const;
  Value operator&&(const Value& that) const;

  bool operator<(const Value& that) const;
  bool operator>(const Value& that) const;
  bool operator==(const Value& that) const;
  bool operator!=(const Value& that) const { return !(*this == that); }

private:
  std::variant<bool, int, std::string> value_;
  Type type;
};

std::ostream& operator<<(std::ostream& os, const Value& v);

} // namespace wwivbasic

template <> struct fmt::formatter<wwivbasic::Value> : fmt::formatter<std::string> {
  auto format(const wwivbasic::Value& v, format_context& ctx) const {
    return fmt::formatter<std::string>::format(v.toString(), ctx);
  }
};
