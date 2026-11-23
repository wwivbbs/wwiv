#include "stdlib/numbers.h"

#include "wwivbasic/module.h"
#include "wwivbasic/value.h"

#include <cmath>

namespace wwivbasic::stdlib {

void register_numbers(Module& m) {
  // ABS(n) is the absolute value; an INTEGER stays an INTEGER.
  m.native_function("ABS", [](Value v) {
    if (v.value_type() == Value::Type::REAL) {
      return Value(std::fabs(v.toDouble()));
    }
    return Value::number(std::fabs(static_cast<double>(v.toInt())));
  });
  // SGN(n) is -1, 0 or 1.
  m.native_function("SGN", [](double d) { return d < 0 ? -1 : (d > 0 ? 1 : 0); });
  // ROUND(n) rounds to the nearest whole number, halves away from zero.
  m.native_function("ROUND", [](double d) { return Value::number(std::round(d)); });
  // FLOOR(n) rounds down, CEIL(n) rounds up, FIX(n) rounds toward zero.
  m.native_function("FLOOR", [](double d) { return Value::number(std::floor(d)); });
  m.native_function("CEIL", [](double d) { return Value::number(std::ceil(d)); });
  m.native_function("FIX", [](double d) { return Value::number(std::trunc(d)); });
}

} // namespace wwivbasic::stdlib
