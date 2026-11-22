#pragma once

// Exposing C++ objects to scripts. A binding describes a C++ class's
// properties and methods; native functions turn C++ objects into script
// values with wrap(), and back with unwrap():
//
//   struct Header { int number; std::string subject; };
//
//   auto& msgs = interp.add_module("msgs");
//   auto& header = msgs.class_<Header>("header");
//   header.property("number", &Header::number)
//         .property("subject", &Header::subject, &Header::subject)   // read-write
//         .method("summary", [](const Header& h, int width) { ... });
//   msgs.native_function("FIRST", [&]() { return header.wrap(Header{1, "Hi"}); });
//
// Scripts then use h = msgs.first() : print h.number; h.subject; h.summary(20).
// Member names are case-insensitive.

#include "wwivbasic/value.h"
#include "core/stl.h"
#include "fmt/format.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace wwivbasic {

// Converts a script Value into the C++ type a native function expects, and a
// native function's result back into a Value. Defined in module.h.
template <typename T> T from_value(const Value& v);
template <typename R> Value to_value(R&& r);

// A bound class: its name and how to read, write and call its members. The
// object itself is passed as a void* that points to the bound C++ type.
struct ClassInfo {
  using Getter = std::function<Value(void*)>;
  using Setter = std::function<void(void*, const Value&)>;
  using Method = std::function<Value(void*, const std::vector<Value>&)>;

  struct Property {
    Getter get;
    // Empty for read-only properties.
    Setter set;
  };

  std::string name;
  std::map<std::string, Property, wwiv::stl::ci_less> properties;
  std::map<std::string, Method, wwiv::stl::ci_less> methods;
};

// The payload of an OBJECT value: the C++ object (kept alive while any script
// value refers to it) and its class.
struct ObjectData {
  std::shared_ptr<void> self;
  std::shared_ptr<const ClassInfo> cls;
};

namespace detail {

// Member function pointer traits: turns &T::f into a lambda taking (T&, args).
// One specialization per const/noexcept combination.
template <class M> struct MemberFunction;

template <class C, class R, class... Args> struct MemberFunctionBase {
  template <class F> static auto as_lambda(F f) {
    return [f](C& o, Args... args) -> R { return (o.*f)(std::forward<Args>(args)...); };
  }
};

template <class C, class R, class... Args>
struct MemberFunction<R (C::*)(Args...)> : MemberFunctionBase<C, R, Args...> {};
template <class C, class R, class... Args>
struct MemberFunction<R (C::*)(Args...) const> : MemberFunctionBase<C, R, Args...> {};
template <class C, class R, class... Args>
struct MemberFunction<R (C::*)(Args...) noexcept> : MemberFunctionBase<C, R, Args...> {};
template <class C, class R, class... Args>
struct MemberFunction<R (C::*)(Args...) const noexcept> : MemberFunctionBase<C, R, Args...> {};

// Callable traits for lambdas and function pointers: result, arguments, arity.
template <class R, class... Args> struct CallableBase {
  using Result = R;
  using ArgsTuple = std::tuple<Args...>;
  static constexpr size_t arity = sizeof...(Args);
};

template <class F> struct Callable : Callable<decltype(&F::operator())> {};

template <class C, class R, class... Args>
struct Callable<R (C::*)(Args...)> : CallableBase<R, Args...> {};
template <class C, class R, class... Args>
struct Callable<R (C::*)(Args...) const> : CallableBase<R, Args...> {};
template <class C, class R, class... Args>
struct Callable<R (C::*)(Args...) noexcept> : CallableBase<R, Args...> {};
template <class C, class R, class... Args>
struct Callable<R (C::*)(Args...) const noexcept> : CallableBase<R, Args...> {};
template <class R, class... Args> struct Callable<R (*)(Args...)> : CallableBase<R, Args...> {};
template <class R, class... Args>
struct Callable<R (*)(Args...) noexcept> : CallableBase<R, Args...> {};

// Calls f(self, args converted from params...). The first parameter of f is
// the object; the rest are converted with from_value.
template <class Self, class F, size_t... I>
Value call_with_self(F& f, Self& self, const std::vector<Value>& params,
                     std::index_sequence<I...>) {
  using Traits = Callable<F>;
  using R = typename Traits::Result;
  using Args = typename Traits::ArgsTuple;
  // The script arguments start after the object.
  [[maybe_unused]] auto arg = [&](auto index) {
    using A = std::decay_t<std::tuple_element_t<decltype(index)::value + 1, Args>>;
    return from_value<A>(params[decltype(index)::value]);
  };
  if constexpr (std::is_void_v<R>) {
    f(self, arg(std::integral_constant<size_t, I>{})...);
    return {};
  } else {
    return to_value(f(self, arg(std::integral_constant<size_t, I>{})...));
  }
}

} // namespace detail

// Untyped base so a Module can own bindings of any type.
class ClassBindingBase {
public:
  virtual ~ClassBindingBase() = default;
};

// Describes how scripts see a C++ class T. Create one with
// Module::class_<T>(name); it lives as long as the module.
template <class T> class ClassBinding : public ClassBindingBase {
public:
  explicit ClassBinding(const std::string& name) : info_(std::make_shared<ClassInfo>()) {
    info_->name = name;
  }

  const std::string& name() const { return info_->name; }

  // A read-only property. `getter` is a data member (&T::x), a const member
  // function (&T::name), or a callable taking const T& (or T&).
  template <class G> ClassBinding& property(const std::string& name, G getter) {
    info_->properties.insert_or_assign(name, ClassInfo::Property{make_getter(getter), {}});
    return *this;
  }

  // A read-write property. `setter` is a data member (&T::x), a member
  // function taking one value (&T::set_name), or a callable taking (T&, value).
  template <class G, class S>
  ClassBinding& property(const std::string& name, G getter, S setter) {
    info_->properties.insert_or_assign(
        name, ClassInfo::Property{make_getter(getter), make_setter(name, setter)});
    return *this;
  }

  // A method. `f` is a member function (&T::count) or a callable whose first
  // parameter is the object (T& or const T&); its other parameters are the
  // script's arguments, converted like native_function's.
  template <class F> ClassBinding& method(const std::string& name, F f) {
    if constexpr (std::is_member_function_pointer_v<F>) {
      return method(name, detail::MemberFunction<F>::as_lambda(f));
    } else {
      using Traits = detail::Callable<F>;
      static_assert(Traits::arity >= 1, "A method's first parameter must be the object");
      static constexpr size_t nargs = Traits::arity - 1;
      info_->methods.insert_or_assign(
          name, [name, cls = info_->name, f](void* p, const std::vector<Value>& params) mutable {
            if (params.size() != nargs) {
              throw BasicError(fmt::format("{}.{}: expected {} argument(s), got {}", cls, name,
                                           nargs, params.size()));
            }
            try {
              return detail::call_with_self(f, *static_cast<T*>(p), params,
                                            std::make_index_sequence<nargs>{});
            } catch (const ArgumentError& e) {
              throw BasicError(fmt::format("{}.{}: {}", cls, name, e.what()));
            }
          });
      return *this;
    }
  }

  // Makes a script value that refers to `object`. The object stays alive while
  // scripts hold it. A null pointer is a script error; to return "nothing",
  // return an empty Value() instead.
  Value wrap(std::shared_ptr<T> object) const {
    if (!object) {
      throw BasicError(fmt::format("{}: no object (null)", info_->name));
    }
    auto data = std::make_shared<ObjectData>();
    data->self = std::move(object);
    data->cls = info_;
    return Value(data);
  }

  // Makes a script value holding a copy of `object`.
  Value wrap(T object) const { return wrap(std::make_shared<T>(std::move(object))); }

  // The C++ object behind a script value, or nullptr if the value isn't an
  // object of this class.
  std::shared_ptr<T> unwrap(const Value& v) const {
    if (!v.is_object()) {
      return nullptr;
    }
    const auto data = v.object("unwrap");
    if (data->cls != info_) {
      return nullptr;
    }
    return std::static_pointer_cast<T>(data->self);
  }

private:
  template <class G> static ClassInfo::Getter make_getter(G getter) {
    if constexpr (std::is_member_object_pointer_v<G>) {
      return [getter](void* p) { return to_value(static_cast<T*>(p)->*getter); };
    } else if constexpr (std::is_member_function_pointer_v<G>) {
      return [getter](void* p) { return to_value((static_cast<T*>(p)->*getter)()); };
    } else {
      return [getter](void* p) { return to_value(getter(*static_cast<T*>(p))); };
    }
  }

  template <class S> static ClassInfo::Setter make_setter(const std::string&, S setter) {
    if constexpr (std::is_member_object_pointer_v<S>) {
      using V = std::decay_t<decltype(std::declval<T&>().*setter)>;
      return [setter](void* p, const Value& v) { static_cast<T*>(p)->*setter = from_value<V>(v); };
    } else if constexpr (std::is_member_function_pointer_v<S>) {
      return make_setter(std::string(), detail::MemberFunction<S>::as_lambda(setter));
    } else {
      using Traits = detail::Callable<S>;
      static_assert(Traits::arity == 2, "A setter takes the object and one value");
      using V = std::decay_t<std::tuple_element_t<1, typename Traits::ArgsTuple>>;
      return [setter](void* p, const Value& v) mutable {
        setter(*static_cast<T*>(p), from_value<V>(v));
      };
    }
  }

  std::shared_ptr<ClassInfo> info_;
};

} // namespace wwivbasic
