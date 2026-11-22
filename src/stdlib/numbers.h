#pragma once

namespace wwivbasic {
class Module;
}

namespace wwivbasic::stdlib {

// Registers ABS, SGN, ROUND, FLOOR, CEIL and FIX. Each takes a number and
// returns an INTEGER when the result is whole, otherwise a REAL.
void register_numbers(Module& m);

} // namespace wwivbasic::stdlib
