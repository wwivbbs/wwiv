#pragma once

#include "wwivbasic/value.h"
#include <vector>

namespace wwivbasic::stdlib {

  Value val(const std::vector<Value>& v);
  Value len(const std::vector<Value>& v);


} // namespace wwivbasic::stdlib
