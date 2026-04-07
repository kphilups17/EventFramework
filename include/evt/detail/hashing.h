#pragma once

#include <tuple>


// Included to support generic hashing of tuples with hash-able types
// Solution adapted from:
// https://stackoverflow.com/questions/7110301/generic-hash-for-tuples-in-unordered-map-unordered-set


namespace evt::detail {


// Type trait to determine if a type is a std::tuple.
template <typename _T> struct is_tuple_f;
template <typename _T> constexpr bool is_tuple = is_tuple_f<_T>::value;


// Hash a variadic list of values, using a fold expression.
// https://en.cppreference.com/w/cpp/language/fold
template <typename ..._Values>
inline std::size_t hashValues(_Values const &...values) {
  std::size_t seed = 0;
  ((seed ^= std::hash<_Values>()(values) + 0x9e3779b9 + (seed << 6) + (seed >> 2)), ...);
  return seed;
}


// Hash a std::tuple by hashing it's values, std::apply will call a function with the tuple expanded into function arguments.
// https://en.cppreference.com/w/cpp/utility/apply
template <typename ..._Values>
inline std::size_t hashTuple(std::tuple<_Values...> const &in_tuple) {
  return std::apply(hashValues<_Values...>, in_tuple);
}


// Hash a value.  If it is a tuple, use hashTuple, otherwise use std::hash.
template <typename _Value>
inline std::size_t hash(_Value &&value) {
  using Value = std::decay_t<decltype(value)>;
  if constexpr (is_tuple<Value>) {
    return hashTuple(value);
  }
  else {
    return std::hash<Value>{}(value);
  }
}


// Implementation of the is_tuple type trait.

template <typename _T>
struct is_tuple_f {
  static constexpr bool value = false;
};

template <typename ..._Ts>
struct is_tuple_f<std::tuple<_Ts...>> {
  static constexpr bool value = true;
};


} // namespace evt::detail
