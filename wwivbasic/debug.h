#pragma once

// What a debugger sees of a running script. See Interpreter::set_statement_hook.

#include "wwivbasic/value.h"

#include <string>

namespace wwivbasic {

// The statement about to run.
struct DebugLocation {
  // The source file, as given to run_file/run_source or as IMPORT found it.
  std::string file;
  // 1-based line and column of the statement's first token.
  int line{0};
  int column{0};
  // How deeply nested the statement is: 0 for the script's top-level code,
  // and one more for each function call or IMPORTed file's top-level code
  // that is running. A debugger's "step over" stops at the next statement
  // whose depth is no greater than the current one.
  int depth{0};
};

// One entry of the call stack.
struct DebugFrame {
  // The function's name as defined (DEF name), or "" for a file's top-level
  // code (the script, or an IMPORTed file while its top-level code runs).
  std::string function;
  std::string file;
  // The line of the statement running in this frame (for frames other than
  // the innermost, the statement that made the call).
  int line{0};
};

// A variable that's visible in some frame of the call stack.
struct DebugVariable {
  // The variable's name, or "module.name" for a variable of a module other
  // than the script's own (one an IMPORTed file's MODULE statement named).
  std::string name;
  // As Value::type_name(): INTEGER, STRING, LIST, ... or an object's class.
  std::string type;
  // As debug_string() shows the value.
  std::string value;
  // The index of the frame in call_stack() whose variable this is: a
  // function's parameters and locals, or for a top-level frame, the globals
  // of the module its code runs in.
  int frame{0};
};

// A value as a debugger shows it: scalars as PRINT shows them, lists as
// [1, "a", [2]] (up to 100 items each, nested up to 4 deep), iterators as
// ITERATOR and objects as their class name.
std::string debug_string(const Value& v);

} // namespace wwivbasic
