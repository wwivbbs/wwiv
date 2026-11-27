#pragma once

// The embedding API. A minimal embedder:
//
//   wwivbasic::StdioConsole console;
//   wwivbasic::Interpreter interp(console);
//   auto& io = interp.add_module("my.io");
//   io.native_function("HELLO", [](std::string name) { return "Hello, " + name; });
//   auto result = interp.run_file("script.bas");
//   for (const auto& e : result.errors) { ... }

#include "wwivbasic/console.h"
#include "wwivbasic/debug.h"
#include "wwivbasic/module.h"
#include "wwivbasic/value.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wwivbasic {

class Context;

struct RunOptions {
  // When false, only parse (and check) the script.
  bool execute{true};
};

struct RunResult {
  bool ok() const { return errors.empty() && !stopped; }
  // Errors formatted as "file:line[:column]: error: message". An error that
  // spans several lines (such as one from an imported file) has '\n' between
  // them.
  std::vector<std::string> errors;
  // True if the stop check (Interpreter::set_stop_check) stopped the script.
  // That isn't an error, so errors is empty, but the script didn't finish.
  bool stopped{false};
};

class Interpreter {
public:
  // The console must outlive the interpreter.
  explicit Interpreter(Console& console);
  ~Interpreter();
  Interpreter(const Interpreter&) = delete;
  Interpreter& operator=(const Interpreter&) = delete;

  // Adds a module of native functions, such as "wwiv.io". Scripts call its
  // functions as wwiv.io.name(), or as name() after IMPORT "@wwiv.io".
  // Returns the existing module if there is already one with this name (it is
  // never replaced, so references to its functions and classes stay valid).
  Module& add_module(const std::string& name);

  // The global module, which holds the built-in functions. Functions added
  // here can be called from any script without a prefix.
  Module& globals();

  // Parses the script at path and, unless options.execute is false, runs it.
  // An interpreter runs one script: calling run_file or run_source again
  // returns an error. Use a new Interpreter for the next script.
  RunResult run_file(const std::filesystem::path& path, const RunOptions& options = {});

  // Parses and runs script text. The filename is used in error messages and
  // to resolve IMPORT "file.bas".
  RunResult run_source(const std::string& filename, const std::string& text,
                       const RunOptions& options = {});

  // The parse tree of a script that has been run (or checked), as nested
  // text, for debugging. nullopt if it wasn't loaded or failed to parse.
  std::optional<std::string> parse_tree(const std::filesystem::path& path) const;

  // Calls a function of the running script by name (a DEF, or a built-in or
  // native function), as a call in the script would, and returns its result.
  // A native function uses it to call back into the script while it runs,
  // such as a lightbar calling a script function for each key:
  //
  //   m.native_function("EACH", [&interp](std::string fn) {
  //     for (int i = 1; i <= 3; i++) interp.call(fn, {Value(i)});
  //     return Value();
  //   });
  //
  // Errors in the function are BasicErrors reported at their line in the
  // script, and the stop check runs as usual. If the function runs END, call
  // throws (a type that isn't a std::exception) to end the native function
  // and then the script; native code must let it through, not catch (...).
  // Throws BasicError if no script is running.
  Value call(const std::string& name, const std::vector<Value>& args);

  // Whether call(name, ...) would find a function: a DEF of the running
  // script, a built-in or a native function. False if no script is running.
  bool has_function(const std::string& name) const;

  // When on, logs function calls and assignments to stderr.
  void set_trace(bool trace);

  // Called before each statement and each loop iteration. When it returns
  // true the script stops, and the RunResult has stopped set. WWIV uses it to
  // end a script when the caller hangs up. It should be cheap.
  void set_stop_check(std::function<bool()> stop);

  // For debuggers: called on the script's thread before each statement runs.
  // When it returns true the script stops, as for the stop check (RunResult
  // has stopped set). While it runs, call_stack(), variables() and source()
  // describe the script; a debugger pauses the script by not returning.
  void set_statement_hook(std::function<bool(const DebugLocation&)> hook);

  // The running script's call stack, innermost frame first. The last frame
  // is the script's top-level code. Empty if no script is running. Like
  // variables(), only call it on the script's thread (from the statement
  // hook or a native function).
  std::vector<DebugFrame> call_stack() const;

  // The variables visible in each frame of call_stack(), innermost first.
  std::vector<DebugVariable> variables() const;

  // The text of a source file that has been loaded (the script, or a file
  // it IMPORTed), by the name DebugLocation::file gives. nullopt if no such
  // file has been loaded.
  std::optional<std::string> source(const std::string& file) const;

  // Only files under dir can be loaded with IMPORT "file.bas"; importing
  // anything else, such as "../x.bas" or an absolute path elsewhere, is an
  // error. By default any file can be imported.
  void set_import_root(const std::filesystem::path& dir);

private:
  // Returns an error result if a script has already been run.
  std::optional<RunResult> check_first_run(const std::string& name);

  std::unique_ptr<Context> ctx_;
  bool ran_{false};
};

} // namespace wwivbasic
