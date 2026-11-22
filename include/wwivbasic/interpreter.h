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
#include "wwivbasic/module.h"
#include "wwivbasic/value.h"

#include <filesystem>
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
  bool ok() const { return errors.empty(); }
  // Errors formatted as "file:line[:column]: error: message".
  std::vector<std::string> errors;
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
  // Replaces any existing module with the same name.
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

  // When on, logs function calls and assignments to stderr.
  void set_trace(bool trace);

private:
  // Returns an error result if a script has already been run.
  std::optional<RunResult> check_first_run(const std::string& name);

  std::unique_ptr<Context> ctx_;
  bool ran_{false};
};

} // namespace wwivbasic
