# callable.hpp

A C++ 11 template for type traits of callables (like functions, objects
with `operator()`, lambda functions, ...).

The template, `callable_traits`, can be used to determine
useful properties of callable objects, like return type, number of
arguments and types of the individual arguments.

It is especially useful in template functions/... that handle callable
objects, for example as callbacks. `callable_traits` allows the code 
to treat different types of callables uniformly without wrapping them
in a `std::function<>`.

## Features
### Callables

Callables are things that can be called with `()`:

- Functions
- Function pointers
- Objects with `operator()`
- `std::function<...>`
- Lambda functions

`callable_traits` can be used to treat all these different kinds of 
callables uniformly in a template function.

### Traits

`callable_traits` deduces the following properties from the
type `T` of a callable:

- The return type

        typename callable_traits<T>::return_type

- The number of arguments

        callable_traits<T>::argc

- The individual argument types

        typename callable_traits<T>::template argument_type<N>

- A function type representing the call:

        typename callable_traits<T>::function_type

### Example

    template<typename Callable>
    void metainfo(Callable f) {
       std::cout << callable_traits<Callable>::argc << " arguments" << std:endl;
    }

## Compilation

### Library

The library is header only, so it doesn't need to be compiled. Just
include the header file when you need it.

### Test cases

To compile the test cases, you need:

- SCons as a build tool
- Boost.Test for the test cases

With these tools you can compile `callable_test` by running scons in the
main directory:

    scons

## License

MIT. See `LICENSE.txt` for more details.

