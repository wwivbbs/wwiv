#pragma once

#include "value.h"

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace wwivbasic {

class Context;

// Stand-ins for the wwiv, wwiv.io and wwiv.data modules that WWIV provides,
// so scripts written for the BBS can run in basicrun and in tests.
//
// Output goes through Context::out. Input (GETS, GETKEY, YN, NY) is read a
// line at a time from `in`. Screen and BBS actions (CLS, PRINTFILE, PAUSE,
// COMMAND) print a marker such as "[cls]" instead. wwiv.data keeps saved
// lists in memory only.
//
// TODO(M3/M4): basicrun will use a stdin/stdout Console instead, and the real
// wwiv.* modules become bindings in WWIV (see docs/PLAN.md).
class WwivStub {
public:
  WwivStub(Context& ctx, std::istream& in);

  // Values returned by wwiv.interpret(), keyed by macro character.
  std::map<char, std::string> macros{{'N', "SYSOP"}, {'#', "1"}};

private:
  std::string read_line();

  Context& ctx_;
  std::istream& in_;
  std::map<std::string, std::vector<Value>> data_;
};

} // namespace wwivbasic
