
#if !defined(CALLABLE_MEMBER_FUNCTION_HPP_INCLUDED)
#define CALLABLE_MEMBER_FUNCTION_HPP_INCLUDED

#include "helpers.hpp"

namespace detail {

// Tags for member function qualifiers
struct const_tag {};
struct volatile_tag {};
struct lref_tag {};
struct rref_tag {};
struct noexcept_tag {};

namespace {

template<typename Class, typename Func, typename... Qual>
struct member_function_traits_q : function_traits<Func> {
	typedef Class class_type;
	static constexpr bool is_const = types_has<const_tag, Qual...>::value;
	static constexpr bool is_volatile = types_has<volatile_tag, Qual...>::value;
	static constexpr bool is_lref = types_has<lref_tag, Qual...>::value;
	static constexpr bool is_rref = types_has<rref_tag, Qual...>::value;
#if __cpp_noexcept_function_type
	static constexpr bool is_noexcept = types_has<noexcept_tag, Qual...>::value;
#endif
};

// We need these until C++17 in case someone takes the address of one
// of those static variables or passses it by reference to a function
template<typename Class, typename Func, typename... Qual>
const bool member_function_traits_q<Class, Func, Qual...>::is_const;
template<typename Class, typename Func, typename... Qual>
const bool member_function_traits_q<Class, Func, Qual...>::is_volatile;
template<typename Class, typename Func, typename... Qual>
const bool member_function_traits_q<Class, Func, Qual...>::is_lref;
template<typename Class, typename Func, typename... Qual>
const bool member_function_traits_q<Class, Func, Qual...>::is_rref;
#if __cpp_noexcept_function_type
template<typename Class, typename Func, typename... Qual>
const bool member_function_traits_q<Class, Func, Qual...>::is_noexcept;
#endif

} // namespace


template<typename MemFun>
struct member_function_traits;

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...)>
	: member_function_traits_q<Class, Ret (Args...)> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const>
	: member_function_traits_q<Class, Ret (Args...), const_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) volatile>
	: member_function_traits_q<Class, Ret (Args...), volatile_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const volatile>
	: member_function_traits_q<Class, Ret (Args...), const_tag, volatile_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) &>
	: member_function_traits_q<Class, Ret (Args...), lref_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const &>
	: member_function_traits_q<Class, Ret (Args...), const_tag, lref_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) volatile &>
	: member_function_traits_q<Class, Ret (Args...), volatile_tag, lref_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const volatile &>
	: member_function_traits_q<Class, Ret (Args...), const_tag, volatile_tag, lref_tag> {
};


template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) &&>
	: member_function_traits_q<Class, Ret (Args...), rref_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const &&>
	: member_function_traits_q<Class, Ret (Args...), const_tag, rref_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) volatile &&>
	: member_function_traits_q<Class, Ret (Args...), volatile_tag, rref_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const volatile &&>
	: member_function_traits_q<Class, Ret (Args...), const_tag, volatile_tag, rref_tag> {
};


#if __cpp_noexcept_function_type
template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) noexcept>
	: member_function_traits_q<Class, Ret (Args...), noexcept_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const noexcept>
	: member_function_traits_q<Class, Ret (Args...), const_tag, noexcept_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) volatile noexcept>
	: member_function_traits_q<Class, Ret (Args...), volatile_tag, noexcept_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const volatile noexcept>
	: member_function_traits_q<Class, Ret (Args...), const_tag, volatile_tag, noexcept_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) & noexcept>
	: member_function_traits_q<Class, Ret (Args...), lref_tag, noexcept_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const & noexcept>
	: member_function_traits_q<Class, Ret (Args...), const_tag, lref_tag, noexcept_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) volatile & noexcept>
	: member_function_traits_q<Class, Ret (Args...), volatile_tag, lref_tag, noexcept_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const volatile & noexcept>
	: member_function_traits_q<Class, Ret (Args...), const_tag, volatile_tag, lref_tag, noexcept_tag> {
};


template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) && noexcept>
	: member_function_traits_q<Class, Ret (Args...), rref_tag, noexcept_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const && noexcept>
	: member_function_traits_q<Class, Ret (Args...), const_tag, rref_tag, noexcept_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) volatile && noexcept>
	: member_function_traits_q<Class, Ret (Args...), volatile_tag, rref_tag, noexcept_tag> {
};

template<typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const volatile && noexcept>
	: member_function_traits_q<Class, Ret (Args...), const_tag, volatile_tag, rref_tag, noexcept_tag> {
};
#endif // __cpp_noexcept_function_type

} // namespace detail


template<typename MemFunPtr>
struct member_function_traits : detail::member_function_traits<detail::remove_cvref_t<MemFunPtr>> {
};

#endif // CALLABLE_MEMBER_FUNCTION_HPP_INCLUDED

