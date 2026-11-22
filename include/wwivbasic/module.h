#pragma once

// Modules hold the functions scripts can call. Embedders add native (C++)
// functions to a module with native_function or native_functionl; see
// interpreter.h for how to create modules.

#include "wwivbasic/value.h"
#include "core/stl.h"
#include "fmt/format.h"

#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace antlr4 {
class ParserRuleContext;
}

namespace wwivbasic {

class Context;
class ExecutionVisitor;
class FunctionDefVisitor;

// A native function that takes its arguments as a vector.
using basic_function_fn = std::function<Value(std::vector<Value>)>;

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

namespace detail {

template <class T> struct AsFunction : public AsFunction<decltype(&T::operator())> {};

template <class ReturnType, class... Args> struct AsFunction<ReturnType(Args...)> {
  using type = std::function<ReturnType(Args...)>;
};

template <class ReturnType, class... Args> struct AsFunction<ReturnType (*)(Args...)> {
  using type = std::function<ReturnType(Args...)>;
};

template <class Class, class ReturnType, class... Args>
struct AsFunction<ReturnType (Class::*)(Args...) const> {
  using type = std::function<ReturnType(Args...)>;
};

// Makes functions, function pointers and lambdas into a std::function.
template <class F> auto as_fn(F f) -> typename AsFunction<F>::type { return {f}; }

template <typename R, typename... Args, size_t... I>
Value call_native(const std::function<R(Args...)>& f, const std::vector<Value>& params,
                  std::index_sequence<I...>) {
  if constexpr (std::is_void_v<R>) {
    f(from_value<Args>(params.at(I))...);
    return {};
  } else {
    return to_value(f(from_value<Args>(params.at(I))...));
  }
}

template <typename R, typename... Args>
basic_function_fn make_basic_fn(const std::string& name, std::function<R(Args...)> f) {
  return [=](const std::vector<Value>& params) -> Value {
    if (params.size() != sizeof...(Args)) {
      throw BasicError(fmt::format("{}: expected {} argument(s), got {}", name, sizeof...(Args),
                                   params.size()));
    }
    return call_native(f, params, std::index_sequence_for<Args...>{});
  };
}

} // namespace detail

// Wraps a C++ function taking and returning int, bool, std::string or Value
// (or returning void) as a basic_function_fn. Arguments are converted to the
// declared types and the argument count is checked.
template <class F> basic_function_fn make_basic_fn(const std::string& name, F f) {
  return detail::make_basic_fn(name, detail::as_fn(f));
}

class Var {
public:
  Var(const std::string& n, const Value& v) : name_(n), value_(v) {}
  Value& value() { return value_; }
  Value& value(const Value& v) {
    value_ = v;
    return value_;
  }

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

class BasicFunction {
public:
  enum class Type { NATIVE, BASIC };

  // A function defined in a script (DEF ... ENDDEF).
  BasicFunction(const std::string& n, antlr4::ParserRuleContext* fn,
                const std::vector<std::string>& p)
      : name(n), type(Type::BASIC), def_fn(fn), params(p) {}

  BasicFunction(const std::string& n, const basic_function_fn& fn,
                const std::vector<std::string>& p)
      : name(n), type(Type::NATIVE), cpp_fn(fn), params(p) {}

  std::string name;
  Type type{Type::BASIC};
  // The DEF's parse tree node (a BasicParser::ProcedureDefinitionContext).
  antlr4::ParserRuleContext* def_fn{nullptr};
  // Source file that defines a BASIC function (for error messages).
  std::string filename;
  basic_function_fn cpp_fn;
  std::vector<std::string> params;
};

class Module {
public:
  explicit Module(const std::string& name) : name_(name) { scopes.emplace_back("<GLOBAL>"); }

  // Registers a native function that takes its arguments as a vector, with no
  // argument count checking.
  void native_functionl(const std::string& name, const basic_function_fn& fn) {
    functions.insert_or_assign(name, BasicFunction(name, fn, {}));
  }

  // Registers a native C++ function or lambda; arguments are converted to the
  // declared parameter types and the argument count is checked.
  template <class F> void native_function(const std::string& name, F f) {
    functions.insert_or_assign(name, BasicFunction(name, make_basic_fn(name, f), {}));
  }

  const std::string& name() const { return name_; }

private:
  friend class Context;
  friend class ExecutionVisitor;
  friend class FunctionDefVisitor;

  // Creates a variable in the current function scope or updates an existing
  // visible variable.
  void upsert(const std::string& name, const Value& value);
  // Gets a visible variable: one in the current function's scope, or a global.
  std::optional<Var> var(const std::string& name);
  bool has_var(const std::string& name) const;
  bool has_fn(const std::string& name) const;
  Var* find_var(const std::string& name);

  // scopes.front() is the module's global scope; each function call pushes
  // a new scope.
  std::deque<Scope> scopes;
  std::map<std::string, BasicFunction, wwiv::stl::ci_less> functions;
  // Modules imported using IMPORT "@module".
  std::set<std::string, wwiv::stl::ci_less> imported_modules;
  std::string name_;
};

} // namespace wwivbasic
