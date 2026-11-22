#pragma once

#include "BasicLexer.h"
#include "BasicParser.h"
#include "value.h"
#include "core/stl.h"
#include "fmt/format.h"

#include <any>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

namespace wwivbasic {

class Var {
public:
  Var(const std::string& n, const Value& v) : name_(n), value_(v) {}
  Value& value() { return value_; }
  Value& value(const Value& v) { value_ = v;  return value_; }

private:
  std::string name_;
  Value value_;
};

class Scope {
public:
  explicit Scope(const std::string& name) : fn_name(name) {}
  std::string fn_name;
  std::map<std::string, Var, wwiv::stl::ci_less> local_vars;
};

typedef std::function<Value(std::vector<Value>)> basic_function_fn;

class BasicFunction {
public:
  enum class Type { NATIVE, BASIC };

  BasicFunction(const std::string& n, BasicParser::ProcedureDefinitionContext* fn,
                const std::vector<std::string>& p)
      : name(n), type(Type::BASIC), def_fn(fn), params(p) {}

  BasicFunction(const std::string& n, const basic_function_fn& fn,
                const std::vector<std::string>& p)
      : name(n), type(Type::NATIVE), cpp_fn(fn), params(p) {}

  std::string name;
  Type type{Type::BASIC};
  BasicParser::ProcedureDefinitionContext* def_fn{nullptr};
  // Source file that defines a BASIC function (for error messages).
  std::string filename;
  basic_function_fn cpp_fn;
  std::vector<std::string> params;
};

// Converts a script Value into the C++ type a native function expects.
template <typename T> T from_value(const Value& v) {
  using D = std::decay_t<T>;
  if constexpr (std::is_same_v<D, Value>) {
    return v;
  } else if constexpr (std::is_same_v<D, bool>) {
    return v.toBool();
  } else if constexpr (std::is_integral_v<D>) {
    return static_cast<D>(v.toInt());
  } else if constexpr (std::is_same_v<D, std::string>) {
    return v.toString();
  } else {
    static_assert(sizeof(D) == 0, "Unsupported native function parameter type");
  }
}

// Converts a native function's return value into a script Value.
template <typename R> Value to_value(R&& r) {
  using D = std::decay_t<R>;
  if constexpr (std::is_same_v<D, Value>) {
    return r;
  } else if constexpr (std::is_same_v<D, bool>) {
    return Value(static_cast<bool>(r));
  } else if constexpr (std::is_integral_v<D>) {
    return Value(static_cast<int>(r));
  } else {
    return Value(std::string(r));
  }
}

template<class T>
struct AsFunction
  : public AsFunction<decltype(&T::operator())>
{};

template<class ReturnType, class... Args>
struct AsFunction<ReturnType(Args...)> {
  using type = std::function<ReturnType(Args...)>;
};

template<class ReturnType, class... Args>
struct AsFunction<ReturnType(*)(Args...)> {
  using type = std::function<ReturnType(Args...)>;
};

template<class Class, class ReturnType, class... Args>
struct AsFunction<ReturnType(Class::*)(Args...) const> {
  using type = std::function<ReturnType(Args...)>;
};

// makes various types of functions into a std::function
template<class F>
auto as_fn(F f) -> typename AsFunction<F>::type {
  return { f };
}

template <typename R, typename... Args, size_t... I>
Value call_native_(const std::function<R(Args...)>& f, const std::vector<Value>& params,
                   std::index_sequence<I...>) {
  if constexpr (std::is_void_v<R>) {
    f(from_value<Args>(params.at(I))...);
    return {};
  } else {
    return to_value(f(from_value<Args>(params.at(I))...));
  }
}

// Wraps a C++ function taking and returning int, bool, std::string or Value
// as a basic_function_fn. Arguments are converted to the declared types.
template <typename R, typename... Args>
basic_function_fn make_basic_fn_(const std::string& name, std::function<R(Args...)> f) {
  return [=](const std::vector<Value>& params) -> Value {
    if (params.size() != sizeof...(Args)) {
      throw BasicError(fmt::format("{}: expected {} argument(s), got {}", name, sizeof...(Args),
                                   params.size()));
    }
    return call_native_(f, params, std::index_sequence_for<Args...>{});
  };
}

template<class F>
basic_function_fn make_basic_fn(const std::string& name, F f) {
  return make_basic_fn_(name, as_fn(f));
}

class Module {
public:
  explicit Module(const std::string& name) : name_(name) {
    scopes.emplace_back("<GLOBAL>");
  }

  // Creates a variable in the current function scope or updates an existing
  // visible variable.
  void upsert(const std::string& name, const Value& value);
  // Gets a visible variable: one in the current function's scope, or a global.
  std::optional<Var> var(const std::string& name);
  bool has_var(const std::string& name) const;
  bool has_fn(const std::string& name) const;

  // Registers a native function that takes its arguments as a vector, with no
  // argument count checking.
  void native_functionl(const std::string& name, const basic_function_fn& fn) {
    functions.insert_or_assign(name, BasicFunction(name, fn, {}));
  }

  // Registers a native C++ function or lambda; arguments are converted to the
  // declared parameter types and the argument count is checked.
  template<class F>
  void native_function(const std::string& name, F f) {
    functions.insert_or_assign(name, BasicFunction(name, make_basic_fn(name, f), {}));
  }

  const std::string& name() const { return name_; }

  // scopes.front() is the module's global scope; each function call pushes
  // a new scope.
  std::deque<Scope> scopes;
  std::map<std::string, BasicFunction, wwiv::stl::ci_less> functions;
  // list of modules currently imported using "IMPORT @module"
  std::set<std::string, wwiv::stl::ci_less> imported_modules;

private:
  Var* find_var(const std::string& name);
  std::string name_;
};

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

// A resolved function along with the module that owns it.
struct ResolvedFunction {
  Module* module{nullptr};
  BasicFunction* fn{nullptr};
};

class Context {
public:
  explicit Context(const std::filesystem::path& path);
  Context();
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

  // Adds a native module, replacing any existing module with the same name.
  Module& add_module(const std::string& name);

  // Parses source text. A path that is already loaded is not replaced, since
  // parse trees in use point into it.
  bool add_source(const std::filesystem::path& path, const std::string& text);
  bool add_source(const std::filesystem::path& path);

  // Gets the parse tree for some unit; nullopt (with errors) on parse failure.
  std::optional<antlr4::tree::ParseTree*> parseTree(const std::filesystem::path& filename);

  // Writes script output (PRINT). Defaults to stdout.
  std::function<void(const std::string&)> out;

  // When true, the interpreter logs execution details to stderr.
  bool trace{false};

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
