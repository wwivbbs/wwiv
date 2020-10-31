/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2020, WWIV Software Services           */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#ifndef INCLUDED_WWIV_CORE_STL_H
#define INCLUDED_WWIV_CORE_STL_H

#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include <algorithm>
#include <iterator>
#include <map>
#include <string>

namespace wwiv::stl {

template <typename C>
bool contains(C const& container, typename C::const_reference key) {
  return std::find(std::begin(container), std::end(container), key) != std::end(container);
}

template <typename K, typename V, typename C, typename A>
bool contains(std::map<K, V, C, A> const& m, K const& key) {
  return m.find(key) != std::end(m);
}

// Partial specialization for maps with string keys (allows using const char* for lookup values)
template <typename V, typename C, typename A>
bool contains(std::map<std::string, V, C, A> const& m, const std::string& key) {
  return m.find(key) != std::end(m);
}

// Partial specialization for maps with const string keys.
template <typename V, typename C, typename A>
bool contains(std::map<const std::string, V, C, A> const& m, const std::string& key) {
  return m.find(key) != std::end(m);
}

struct ci_less {
  bool operator()(const std::string& left, const std::string& right) const {
    return strcasecmp(left.c_str(), right.c_str()) < 0;
  }
};

template <typename C>
typename C::mapped_type get_or_default(C const& c,
                                       const typename C::key_type& key,
                                       const typename C::mapped_type default_value) {
  typename C::const_iterator iter = c.find(key);
  if (iter == std::end(c)) {
    return default_value;
  }
  return iter->second;
}

template <typename C>
int32_t size_int32(const C& c) {
  const auto size = c.size();
  CHECK_LE(size, static_cast<typename C::size_type>(std::numeric_limits<int32_t>::max()));
  return static_cast<int32_t>(size);
}

template <typename C>
uint32_t size_uint32(const C& c) {
  const auto size = c.size();
  CHECK_LE(size, static_cast<typename C::size_type>(std::numeric_limits<uint32_t>::max()));
  return static_cast<uint32_t>(size);
}

template <typename C>
int16_t size_int16(C const& c) {
  const auto size = c.size();
  CHECK_LE(size, static_cast<typename C::size_type>(std::numeric_limits<int16_t>::max()));
  return static_cast<int16_t>(size);
}

template <typename C>
int8_t size_int8(C const& c) {
  const auto size = c.size();
  CHECK_LE(size, static_cast<typename C::size_type>(std::numeric_limits<int8_t>::max()));
  return static_cast<int8_t>(size);
}

// From https://en.cppreference.com/w/cpp/iterator/size (The C++20 version)
template <class C>
constexpr auto ssize(const C& c) 
    -> std::common_type_t<std::ptrdiff_t,
                          std::make_signed_t<decltype(c.size())>> 
{
    using R = std::common_type_t<std::ptrdiff_t, std::make_signed_t<decltype(c.size())>>;
    return static_cast<R>(c.size());
}

template <typename C>
int size_int(const C& c) {
  return static_cast<int>(ssize(c));
}

template <typename C, typename S = std::size_t, typename R>
bool insert_at(C& c, S pos, R r) {
  auto n = static_cast<decltype(c.size())>(pos);

  if (n < 0 || n > c.size()) {
    return false;
  }
  auto it = std::begin(c);
  std::advance(it, n);
  c.insert(it, r);
  return true;
}

template <typename C, typename S = std::size_t>
bool erase_at(C& c, S on) {
  auto n = static_cast<decltype(c.size())>(on);
  if (n >= c.size()) {
    return false;
  }
  auto it = std::begin(c);
  std::advance(it, n);
  c.erase(it);
  return true;
}

// Enum has function. This is needed for GCC < 6.0
// GCC 6+ can use hash types w/o specifying a hash function
// as the 3rd template type.
struct enum_hash {
  template <typename T> typename std::enable_if<std::is_enum<T>::value, std::size_t>::type
  operator ()(T const value) const { return static_cast<std::size_t>(value); }
};

// Specialization for std::map
template <typename K, typename V, typename C, class A=std::allocator<C>>
V at(std::map<K, V, C, A> const& container, K const& pos) {
  try {
    return container.at(pos);
  } catch (const std::out_of_range&) {
    LOG(ERROR) << "Caught std::out_of_range on pos: " << pos;
    LOG(ERROR) << wwiv::os::stacktrace();
    DLOG(FATAL) << "Terminating in debug build.";
    throw;
  }
}

template <typename C, class Allocator=std::allocator<C>>
typename C::const_reference at(C const& container, typename C::size_type pos) {
  try {
    return container.at(pos);
  } catch (const std::out_of_range&) {
    LOG(ERROR) << "Caught std::out_of_range on pos: " << pos;
    LOG(ERROR) << wwiv::os::stacktrace();
    DLOG(FATAL) << "Terminating in debug build.";
    throw;
  }
}

} // namespace


#endif