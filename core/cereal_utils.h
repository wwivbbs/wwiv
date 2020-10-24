/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2020, WWIV Software Services             */
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
/**************************************************************************/
#ifndef INCLUDED_SDK_CEREAL_UTILS_H
#define INCLUDED_SDK_CEREAL_UTILS_H

// ReSharper disable CppUnusedIncludeDirective
#include <cereal/access.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/set.hpp>
#include <stdexcept>
#include <vector>

#include "core/stl.h"

namespace cereal {

#define SERIALIZE(n, field)                                                                        \
  do {                                                                                             \
    try {                                                                                          \
      ar(cereal::make_nvp(#field, (n).field));                                                     \
    } catch (const cereal::Exception&) {                                                           \
      ar.setNextName(nullptr);                                                                     \
    }                                                                                              \
  } while (false)

#define SERIALIZE_NVP(name, field)                                                                        \
  do {                                                                                             \
    try {                                                                                          \
      ar(cereal::make_nvp(name, field));                                                     \
    } catch (const cereal::Exception&) {                                                           \
      ar.setNextName(nullptr);                                                                     \
    }                                                                                              \
  } while (false)


template <typename T>
std::string to_enum_string(const T& t, const std::vector<std::string>& names) {
  try {
    return names.at(static_cast<int>(t));
  } catch (std::out_of_range&) {
    return names.at(0);
  }
}

template <typename T>
T from_enum_string(const std::string& v, const std::vector<std::string>& names) {
  try {
    for (auto i = 0; i < wwiv::stl::ssize(names); i++) {
      if (v == names.at(i)) {
        return static_cast<T>(i);
      }
    }
  } catch (std::out_of_range&) {
    // NOP
  }
  return static_cast<T>(0);
}

} // namespace cereal

#endif