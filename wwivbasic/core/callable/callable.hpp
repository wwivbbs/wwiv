
#if !defined(CALLABLE_CALLABLE_HPP_INCLUDED)
#define CALLABLE_CALLABLE_HPP_INCLUDED

#include <stddef.h>
#include <functional>
#include "helpers.hpp"
#include "function.hpp"
#include "member_function.hpp"
#include "functor.hpp"

// There are three basic kinds of callable types
// function types
struct function_tag {};
// function pointer types
struct function_ptr_tag {};
// classes with operator()
struct functor_tag {};


namespace detail {

/** Define traits for a operator() member function pointer type */

// classes with operator()
template<typename Callable>
struct callable_traits : functor_traits<Callable> {
	typedef functor_tag callable_category;
};

// functions
template<typename Ret, typename... Args>
struct callable_traits<Ret (Args...)> : function_traits<Ret (Args...)> {
	typedef function_tag callable_category;
};

// function pointers
template<typename Ret, typename... Args>
struct callable_traits<Ret (*)(Args...)> : function_traits<Ret (Args...)> {
	typedef function_ptr_tag callable_category;
};

} // namespace detail


// Main template

/** Traits for a callable (function/functor/lambda/...) */
template<typename Callable>
struct callable_traits : detail::callable_traits<detail::remove_cvref_t<Callable>> {
};


/** Convert a callable to a std::function<> */
template<typename Callable>
std::function<typename callable_traits<Callable>::function_type> to_stdfunction(Callable fun) {
	std::function<typename callable_traits<Callable>::function_type> stdfun(std::forward<Callable>(fun));
	return stdfun;
}

#endif // CALLABLE_CALLABLE_HPP_INCLUDED

