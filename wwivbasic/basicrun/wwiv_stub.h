#pragma once

#include "wwivbasic/console.h"
#include "wwivbasic/interpreter.h"
#include "wwivbasic/value.h"

#include <map>
#include <string>
#include <vector>

namespace wwivbasic {

// Stand-ins for the wwiv, wwiv.io and wwiv.data modules that WWIV provides,
// so scripts written for the BBS can run in basicrun and in tests. They use
// only the public embedding API, like real bindings would.
//
// Output and input (GETS, GETKEY, YN, NY) go through the Console. BBS
// actions (PRINTFILE, PAUSE, COMMAND) print a marker such as "[pause]"
// instead. wwiv.data keeps saved lists in memory only.
//
// TODO(M4): the real wwiv.* modules become bindings in WWIV (see
// docs/PLAN.md).
class WwivStub {
public:
  WwivStub(Interpreter& interp, Console& console);

  // Values returned by wwiv.interpret(), keyed by macro character.
  std::map<char, std::string> macros{{'N', "SYSOP"}, {'#', "1"}};

private:
  Console& console_;
  std::map<std::string, std::vector<Value>> data_;
};

} // namespace wwivbasic
