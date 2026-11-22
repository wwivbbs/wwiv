#include "stdlib/common.h"

#include "core/stl.h"

namespace wwivbasic::stdlib {

Value val(const std::vector<Value>& v) {
  if (v.empty()) {
    return Value(0);
  }
  return Value(v.front().toInt());
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
