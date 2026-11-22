#pragma once

#include "fmt/format.h"

#include <any>
#include <iosfwd>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace wwivbasic {

// An error raised while running a script. The executor fills in the source
// location of the statement being executed if it was not already set.
class BasicError : public std::runtime_error {
public:
  explicit BasicError(const std::string& msg) : std::runtime_error(msg) {}
  std::string file;
  size_t line{0};
};

class Value;
struct ListData;
struct IteratorData;

using ListPtr = std::shared_ptr<ListData>;
using IteratorPtr = std::shared_ptr<IteratorData>;

class Value {
public:
  // LIST and ITERATOR values are references: copying the Value shares the
  // underlying list, as in my_basic.
  enum class Type { BOOLEAN, INTEGER, REAL, STRING, LIST, ITERATOR };

  Value() : value_(std::string()), type(Type::STRING) {}
  explicit Value(bool b) : value_(b), type(Type::BOOLEAN) {}
  explicit Value(int i) : value_(i), type(Type::INTEGER) {}
  explicit Value(double d) : value_(d), type(Type::REAL) {}
  explicit Value(const std::string& s) : value_(s), type(Type::STRING) {}
  explicit Value(const char* s) : value_(std::string(s)), type(Type::STRING) {}
  explicit Value(ListPtr l) : value_(std::move(l)), type(Type::LIST) {}
  explicit Value(IteratorPtr it) : value_(std::move(it)), type(Type::ITERATOR) {}
  explicit Value(const std::any& a);

  // A REAL, or an INTEGER if d is a whole number that fits in one, like
  // my_basic: arithmetic results go through this, so 1.5 + 1.5 is 3.
  static Value number(double d);

  // Parses a decimal number at the start of s, after any spaces and an
  // optional sign: "12", "1.5", " -3", "2.5e3", "12abc" (12). Hex, inf and nan
  // are not numbers. Doesn't depend on the locale. nullopt if s doesn't start
  // with a number or it doesn't fit in a double.
  static std::optional<double> parse_number(std::string_view s);

  // Creates a new, empty list.
  static Value new_list();

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
  bool is_list() const { return type == Type::LIST; }
  bool is_iterator() const { return type == Type::ITERATOR; }
  // BOOLEAN, INTEGER or REAL.
  bool is_number() const;

  // The list, or throws BasicError naming `fn` if this is not a list.
  ListPtr list(std::string_view fn) const;
  // The iterator, or throws BasicError naming `fn` if this is not an iterator.
  IteratorPtr iterator(std::string_view fn) const;

  bool toBool() const;
  // REALs are truncated toward zero.
  int toInt() const;
  double toDouble() const;
  // REALs print like printf("%g") (3.14159, 0.5). Lists print as "LIST" and
  // iterators as "ITERATOR", like my_basic.
  std::string toString() const;
  // The type name used in error messages: BOOLEAN, INTEGER, STRING, ...
  std::string type_name() const;
  // The type name with "a" or "an": "a STRING", "an INTEGER".
  std::string a_type_name() const;

  template <typename T>
  T get() const { return std::get<T>(value_); }

  // operators
  Value operator+(const Value& that) const;
  Value operator-(const Value& that) const;
  Value operator*(const Value& that) const;
  Value operator/(const Value& that) const;
  Value operator%(const Value& that) const;

  bool operator<(const Value& that) const;
  bool operator>(const Value& that) const;
  // Scalars compare by value; lists and iterators by identity.
  bool operator==(const Value& that) const;
  bool operator!=(const Value& that) const { return !(*this == that); }

private:
  // Throws unless both operands are scalars (BOOLEAN, INTEGER, REAL, STRING).
  void check_scalar(const Value& that, std::string_view op) const;

  std::variant<bool, int, double, std::string, ListPtr, IteratorPtr> value_;
  Type type;
};

struct ListData {
  std::vector<Value> items;
};

// Position within a list. Starts before the first item; MOVE_NEXT advances.
struct IteratorData {
  ListPtr list;
  int pos{-1};
};

std::ostream& operator<<(std::ostream& os, const Value& v);

} // namespace wwivbasic

template <> struct fmt::formatter<wwivbasic::Value> : fmt::formatter<std::string> {
  auto format(const wwivbasic::Value& v, format_context& ctx) const {
    return fmt::formatter<std::string>::format(v.toString(), ctx);
  }
};
