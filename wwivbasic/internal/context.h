#pragma once

// Internal: the interpreter's state. Embedders use wwivbasic/interpreter.h.

#include "wwivbasic/internal/antlr.h"
#include "wwivbasic/console.h"
#include "wwivbasic/module.h"
#include "wwivbasic/value.h"
#include "core/stl.h"
#include "fmt/format.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace wwivbasic {

class SourceUnit;

class BasicParserErrorListener : public antlr4::BaseErrorListener {
public:
  BasicParserErrorListener(SourceUnit* su) : su_(su) {}
  void syntaxError(antlr4::Recognizer* recognizer, antlr4::Token* offendingSymbol, size_t line,
    size_t charPositionInLine, const std::string& msg,
    std::exception_ptr e) override;
  SourceUnit* su_{ nullptr };
};

class SourceUnit {
public:
  SourceUnit(const std::string& filename, const std::string& text)
      : filename_(filename), text_(text), input_(text), lexer_(&input_), tokens_(&lexer_),
        parser_(&tokens_), parserError_(this) {
    lexer_.removeErrorListeners();
    lexer_.addErrorListener(&parserError_);
    parser_.removeErrorListeners();
    parser_.addErrorListener(&parserError_);
    tree_ = parser_.main();
  }

  // Gets the parse tree for this source unit.
  antlr4::tree::ParseTree* tree() { return tree_; }
  BasicParser* parser() { return &parser_; }

  std::string filename_;
  std::string text_;
  antlr4::ANTLRInputStream input_;
  BasicLexer lexer_;
  antlr4::CommonTokenStream tokens_;
  BasicParser parser_;
  antlr4::tree::ParseTree* tree_{nullptr};
  BasicParserErrorListener parserError_;
  std::vector<std::string> errors;
};

// Thrown when the embedder's stop check asks the script to stop. Not a
// BasicError, so statements and native-function guards pass it through, and
// not a std::exception, so generic handlers don't turn it into an error.
struct StopScript {};

// Thrown by Interpreter::call when the script function it called ran END, so
// the native function that called it stops too. Like StopScript, it passes
// through statements and native-function guards; the run then ends normally.
struct EndScript {};

class ExecutionVisitor;

// A resolved function along with the module that owns it.
struct ResolvedFunction {
  Module* module{nullptr};
  BasicFunction* fn{nullptr};
};

class Context {
public:
  explicit Context(Console& console);
  // Modules hold pointers into each other via root/module.
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  // Creates a variable at the top scope or updates existing variable.
  void upsert(const std::string& name, const Value& value);
  // Gets the value of a variable.
  std::optional<Var> var(const std::string& name);
  // Finds a function by (possibly qualified) name. Unqualified names are
  // looked up in the current module, the root module, then in modules
  // imported by the current module. Throws BasicError if not found.
  ResolvedFunction find_function(const std::string& function_name);
  // Like find_function, but returns nullopt instead of throwing.
  std::optional<ResolvedFunction> try_find_function(const std::string& function_name);

  // Adds a module, or returns the existing one with the same name.
  Module& add_module(const std::string& name);

  // Parses source text. A path that is already loaded is not replaced, since
  // parse trees in use point into it.
  bool add_source(const std::filesystem::path& path, const std::string& text);
  bool add_source(const std::filesystem::path& path);

  // Gets the parse tree for some unit; nullopt (with errors) on parse failure.
  std::optional<antlr4::tree::ParseTree*> parseTree(const std::filesystem::path& filename);

  // Where PRINT and the console functions write and read.
  Console& console;

  // When true, the interpreter logs execution details to stderr.
  bool trace{false};

  // The visitor running the script, while it runs (the innermost one while
  // an imported file's top-level code runs); for Interpreter::call.
  ExecutionVisitor* executor{nullptr};

  // Set by Interpreter::set_stop_check; see check_stop.
  std::function<bool()> stop_check;
  // Throws StopScript if stop_check says the script should stop.
  void check_stop() const {
    if (stop_check && stop_check()) {
      throw StopScript{};
    }
  }

  // Set by Interpreter::set_import_root: IMPORT "file.bas" may only load files
  // under this directory (normalized, without a trailing separator).
  std::optional<std::filesystem::path> import_root;

  template <typename... Args> void trace_log(fmt::format_string<Args...> f, Args&&... args) {
    if (trace) {
      fmt::print(stderr, "TRACE: {}\n", fmt::format(f, std::forward<Args>(args)...));
    }
  }

  // All registered modules
  std::map<std::string, Module, wwiv::stl::ci_less> modules;
  std::map<std::filesystem::path, std::unique_ptr<SourceUnit>> sources;
  // Source files loaded with IMPORT "file.bas", so each is only run once.
  std::set<std::filesystem::path> imported_files;

  // The key used for imported_files: the canonical path if it can be found,
  // so different spellings of the same file match.
  static std::filesystem::path import_key(const std::filesystem::path& path);
  Module* root{ nullptr };
  Module* module{ nullptr };
  std::vector<std::string> errors;
};

} // namespace wwivbasic
