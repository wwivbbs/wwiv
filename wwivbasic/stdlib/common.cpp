#include "wwivbasic/stdlib/common.h"

#include "core/stl.h"

namespace wwivbasic::stdlib {

Value val(const std::vector<Value>& v) {
  if (v.empty()) {
    return Value(0);
  }
  // VAL("1.5") is 1.5 and VAL("12") is 12; text that isn't a number is 0.
  return Value::number(v.front().toDouble());
}

// LEN(s) is the length of a string; LEN(l) is the number of items in a list.
// LEN() is 0, as in my_basic outside a variadic function.
Value len(const std::vector<Value>& v) {
  if (v.empty()) {
    return Value(0);
  }
  if (v.size() != 1) {
    throw BasicError(fmt::format("LEN: expected 1 argument(s), got {}", v.size()));
  }
  if (v.front().is_list()) {
    return Value(wwiv::stl::size_int(v.front().list("LEN")->items));
  }
  return Value(wwiv::stl::size_int(v.front().toString()));
}

} // namespace wwivbasic::stdlib
