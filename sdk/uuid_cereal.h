/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2020-2022, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_UUID_CEREAL_H__
#define __INCLUDED_SDK_UUID_CEREAL_H__

#include "cereal/cereal.hpp"
#include "core/uuid.h"

CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(wwiv::core::uuid_t, cereal::specialization::non_member_load_save_minimal);

namespace cereal {

template <class Archive>
inline std::string save_minimal(Archive const&, const wwiv::core::uuid_t& a) {
  return a.to_string();
}

template <class Archive>
inline void load_minimal(Archive const&, wwiv::core::uuid_t& a, const std::string& s) {
  const auto o = wwiv::core::uuid_t::from_string(s);
  a = o.value_or(wwiv::core::uuid_t());
}

}
#endif // __INCLUDED_SDK_UUID_CEREAL_H__
