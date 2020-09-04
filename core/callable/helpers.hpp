
#if !defined(CALLABLE_HELPERS_HPP_INCLUDED)
#define CALLABLE_HELPERS_HPP_INCLUDED

#include <cstddef>

namespace detail {

/** Remove reference and cv qualification */
template<typename T>
using remove_cvref_t = typename std::remove_cv< typename std::remove_reference<T>::type >::type;


/** Count the number of types given to the template */
template<typename... Types>
struct types_count;

template<>
struct types_count<> {
	static constexpr std::size_t value = 0;
};


template<typename Type, typename... Types>
struct types_count<Type, Types...> {
	static constexpr std::size_t value = types_count<Types...>::value + 1;
};


/** Get the nth type given to the template */
template<std::size_t n, typename... Types>
struct types_n;

template<std::size_t N, typename Type, typename... Types>
struct types_n<N, Type, Types...> : types_n<N-1, Types...> {
};

template<typename Type, typename... Types>
struct types_n<0, Type, Types...> {
	typedef Type type;
};


/** Test if a type is in a list given types */
template<typename Q, typename... Ts>
struct types_has;

template<typename Q>
struct types_has<Q> {
	static constexpr bool value = false;
};

template<typename Q, typename... Ts>
struct types_has<Q, Q, Ts...> {
	static constexpr bool value = true;
};

template<typename Q, typename T, typename... Ts>
struct types_has<Q, T, Ts...> : types_has<Q, Ts...> {
};


} // namespace detail

#endif // CALLABLE_HELPERS_HPP_INCLUDED

