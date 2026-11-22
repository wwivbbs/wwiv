#pragma once

// Modules hold the functions scripts can call. Embedders add native (C++)
// functions to a module with native_function or native_functionl; see
// interpreter.h for how to create modules.

#include "wwivbasic/object.h"
#include "wwivbasic/value.h"
#include "core/stl.h"
#include "fmt/format.h"

#include <deque>
#include <functional>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
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

namespace detail {

// Throws unless v can be converted to the scalar type `expected`: lists,
// iterators and objects can't be used as numbers, strings or booleans.
inline void check_scalar_argument(const Value& v, const char* expected) {
  if (v.is_list() || v.is_iterator() || v.is_object()) {
    throw ArgumentError(fmt::format("expected {}, got {}", expected, v.a_type_name()));
  }
}

// True for integer types that fit in a script INTEGER (a 32-bit int).
template <class D> constexpr bool fits_in_int() {
  if constexpr (!std::is_integral_v<D>) {
    return false;
  } else {
    return std::cmp_less_equal(std::numeric_limits<D>::max(), std::numeric_limits<int>::max()) &&
           std::cmp_greater_equal(std::numeric_limits<D>::min(), std::numeric_limits<int>::min());
  }
}

} // namespace detail

// Converts a script Value into the C++ type a native function expects.
// Integer types wider than int (int64_t, time_t, uint32_t) are converted
// through a double and clamped to the type's range.
template <typename T> T from_value(const Value& v) {
  using D = std::decay_t<T>;
  if constexpr (std::is_same_v<D, Value>) {
    return v;
  } else if constexpr (std::is_same_v<D, bool>) {
    detail::check_scalar_argument(v, "a BOOLEAN");
    return v.toBool();
  } else if constexpr (detail::fits_in_int<D>()) {
    detail::check_scalar_argument(v, "an INTEGER");
    return static_cast<D>(v.toInt());
  } else if constexpr (std::is_integral_v<D>) {
    detail::check_scalar_argument(v, "an INTEGER");
    const auto d = v.toDouble();
    if (std::isnan(d) || d <= static_cast<double>(std::numeric_limits<D>::min())) {
      return std::numeric_limits<D>::min();
    }
    if (d >= static_cast<double>(std::numeric_limits<D>::max())) {
      return std::numeric_limits<D>::max();
    }
    return static_cast<D>(d);
  } else if constexpr (std::is_floating_point_v<D>) {
    detail::check_scalar_argument(v, "a REAL");
    return static_cast<D>(v.toDouble());
  } else if constexpr (std::is_same_v<D, std::string>) {
    detail::check_scalar_argument(v, "a STRING");
    return v.toString();
  } else {
    static_assert(sizeof(D) == 0, "Unsupported native function parameter type");
  }
}

namespace detail {
template <class T> struct is_vector : std::false_type {};
template <class T, class A> struct is_vector<std::vector<T, A>> : std::true_type {};
} // namespace detail

// Converts a native function's return value into a script Value. Integers
// that don't fit in an INTEGER become REALs. A std::vector becomes a LIST of
// its converted items.
template <typename R> Value to_value(R&& r) {
  using D = std::decay_t<R>;
  if constexpr (std::is_same_v<D, Value>) {
    return r;
  } else if constexpr (std::is_same_v<D, bool>) {
    return Value(static_cast<bool>(r));
  } else if constexpr (detail::fits_in_int<D>()) {
    return Value(static_cast<int>(r));
  } else if constexpr (std::is_integral_v<D> || std::is_floating_point_v<D>) {
    // Whole results that fit become INTEGERs, as in arithmetic.
    return Value::number(static_cast<double>(r));
  } else if constexpr (detail::is_vector<D>::value) {
    std::vector<Value> items;
    items.reserve(r.size());
    // Convert each item to the element type first, so std::vector<bool>'s
    // proxy references become bools.
    for (auto&& item : r) {
      items.push_back(to_value(static_cast<typename D::value_type>(item)));
    }
    return Value::make_list(std::move(items));
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

// noexcept functions and lambdas.
template <class ReturnType, class... Args> struct AsFunction<ReturnType (*)(Args...) noexcept> {
  using type = std::function<ReturnType(Args...)>;
};

template <class Class, class ReturnType, class... Args>
struct AsFunction<ReturnType (Class::*)(Args...) const noexcept> {
  using type = std::function<ReturnType(Args...)>;
};

// Mutable lambdas.
template <class Class, class ReturnType, class... Args>
struct AsFunction<ReturnType (Class::*)(Args...)> {
  using type = std::function<ReturnType(Args...)>;
};

// Makes functions, function pointers and lambdas into a std::function.
template <class F> auto as_fn(F f) -> typename AsFunction<F>::type { return {f}; }

template <typename R, typename... Args, size_t... I>
Value call_native(const std::function<R(Args...)>& f, const std::vector<Value>& params,
                  std::index_sequence<I...>) {
  if constexpr (std::is_void_v<R>) {
    f(from_value<std::decay_t<Args>>(params.at(I))...);
    return {};
  } else {
    return to_value(f(from_value<std::decay_t<Args>>(params.at(I))...));
  }
}

template <typename R, typename... Args>
basic_function_fn make_basic_fn(const std::string& name, std::function<R(Args...)> f) {
  return [=](const std::vector<Value>& params) -> Value {
    if (params.size() != sizeof...(Args)) {
      throw BasicError(fmt::format("{}: expected {} argument(s), got {}", name, sizeof...(Args),
                                   params.size()));
    }
    try {
      return call_native(f, params, std::index_sequence_for<Args...>{});
    } catch (const ArgumentError& e) {
      throw BasicError(fmt::format("{}: {}", name, e.what()));
    }
  };
}

} // namespace detail

// Wraps a C++ function taking and returning int, double, bool, std::string or Value
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

  // Describes a C++ class T for scripts (see object.h). The binding is owned
  // by the module and lives as long as the interpreter. `name` is the class
  // name scripts see when printing an object or in errors.
  template <class T> ClassBinding<T>& class_(const std::string& name) {
    auto binding = std::make_unique<ClassBinding<T>>(name);
    auto& result = *binding;
    classes_.push_back(std::move(binding));
    return result;
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
  std::vector<std::unique_ptr<ClassBindingBase>> classes_;
};

} // namespace wwivbasic
