/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2020-2021, WWIV Software Services             */
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
#ifndef INCLUDED_COMMON_INPUT_RANGE_H
#define INCLUDED_COMMON_INPUT_RANGE_H

#include "common/input.h"
#include "core/strings.h"
#include "common/context.h"
#include <optional>
#include <set>
#include <string>

namespace wwiv::common {

std::vector<std::string> split_distinct_ranges(std::string s);

bool is_consecutive_range(const std::string& text);


template<typename T> std::set<T> expand_consecutive_range(const std::string& text) {
  if (!is_consecutive_range(text)) {
    return {};
  }
  auto v = strings::SplitString(text, "-", true);
  if (v.size() != 2) {
    return {};
  }
  if constexpr (std::is_same_v<char, T>) {
    if (v.at(0).size() != 1 || v.at(1).size() != 1) {
      return {};
    }
    const auto start = v.at(0).front();
    const auto end = v.at(1).front();
    std::set<T> out;
    for (auto i = start; i <= end; ++i) {
      out.emplace(i);
    }
    return out;
  } else if constexpr (std::is_integral_v<T>) {
    const auto start = strings::to_number<T>(v.at(0));
    const auto end = strings::to_number<T>(v.at(1));
    std::set<T> out;
    for (auto i = start; i <= end; ++i) {
      out.emplace(i);
    }
    return out;
  } else {
    return {};
  }
}

template<typename T>
std::set<T> expand_ranges(const std::string& text, const std::set<T>& range) {
  const auto v = split_distinct_ranges(text);
  if (v.empty()) {
    return {};
  }
  std::set<T> out;
  for (const auto& r : v) {
    if (r.empty()) {
      continue;
    }
    if (is_consecutive_range(r)) {
      const auto r1 = expand_consecutive_range<T>(r);
      out.insert(std::begin(r1), std::end(r1));
    } else {
      if constexpr (std::is_same_v<char, T>) {
        out.insert(r.front());
      } else if constexpr (std::is_integral_v<T>) {
        out.insert(strings::to_number<T>(r));
      }
    }
  }

  std::set<T> intersection;
  std::set_intersection(std::begin(out), std::end(out), 
    std::begin(range), std::end(range),
           std::inserter(intersection, std::begin(intersection)));
  return intersection;
}

template<typename T>
std::optional<std::set<T>> input_range(Input& in, int maxlen, const std::set<T>& range) {
  const auto s = in.input_text("", maxlen);
  if (s.empty()) {
    return std::nullopt;
  }
  return expand_ranges(s, range);
}

} // namespace


#endif
