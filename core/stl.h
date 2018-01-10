/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2017, WWIV Software Services           */
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
#ifndef __INCLUDED_WWIV_CORE_STL_H__
#define __INCLUDED_WWIV_CORE_STL_H__
#pragma once

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iterator>
#include <map>
#include <functional>
#include <string>

#include "core/log.h"
#include "core/strings.h"

namespace wwiv {
namespace stl {

template <typename C>
bool contains(C const& container, typename C::const_reference key) {
  return std::find(std::begin(container), std::end(container), key) != std::end(container);
}

template <typename K, typename V, typename C, typename A>
bool contains(std::map<K, V, C, A> const& m, K const& key) {
  return m.find(key) != m.end();
}

// Partial specicialization for maps with string keys (allows using const char* for lookup values)
template <typename V, typename C, typename A>
bool contains(std::map<std::string, V, C, A> const& m, const std::string& key) {
  return m.find(key) != m.end();
}

// Partial specicialization for maps with const string keys.
template <typename V, typename C, typename A>
bool contains(std::map<const std::string, V, C, A> const& m, const std::string& key) {
  return m.find(key) != m.end();
}

struct ci_less {
  bool operator() (const std::string& left, const std::string& right) const {
	  return strcasecmp(left.c_str(), right.c_str()) < 0;
  }
};

template <typename C>
typename C::mapped_type get_or_default(C c,
    const typename C::key_type& key,
    const typename C::mapped_type default_value) {
  typename C::const_iterator iter = c.find(key);
  if (iter == std::end(c)) {
    return default_value;
  }
  return iter->second;
}

template <typename C>
int32_t size_int32(C c) {
  const auto size = c.size();
  CHECK_LE(size, static_cast<typename C::size_type>(std::numeric_limits<int32_t>::max()));
  return static_cast<int32_t>(size);
}

template <typename C>
int16_t size_int16(C c) {
  const auto size = c.size();
  CHECK_LE(size, static_cast<typename C::size_type>(std::numeric_limits<int16_t>::max()));
  return static_cast<int16_t>(size);
}

template <typename C>
int8_t size_int8(C c) {
  const auto size = c.size();
  CHECK_LE(size, static_cast<typename C::size_type>(std::numeric_limits<int8_t>::max()));
  return static_cast<int8_t>(size);
}

template <typename C>
signed int size_int(C c) {
  return wwiv::stl::size_int32(c);
}

template <typename C, typename S = std::size_t, typename R>
bool insert_at(C& c, S pos, R r) {
  std::size_t n = static_cast<decltype(c.size())>(pos);

  if (n < 0 || n > c.size()) {
    return false;
  }
  auto it = c.begin();
  std::advance(it, n);
  c.insert(it, r);
  return true;
}

template <typename C, typename S = std::size_t>
bool erase_at(C& c, S on) {
  std::size_t n = static_cast<decltype(c.size())>(on);
  if (n >= c.size()) {
    return false;
  }
  auto it = c.begin();
  std::advance(it, n);
  c.erase(it);
  return true;
}

}  // namespace stl
}  // namespace wwiv

#endif  // __INCLUDED_WWIV_CORE_STL_H__
