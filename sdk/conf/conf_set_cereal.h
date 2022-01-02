/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
/*                                                                        */
/**************************************************************************/
#ifndef INCLUDED_SDK_CONF_CEREAL_H
#define INCLUDED_SDK_CONF_CEREAL_H

#include "core/cereal_utils.h"
#include "sdk/conf/conf_set.h"
#include <cereal/specialize.hpp>

CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(wwiv::sdk::conf_set_t, specialization::non_member_load_save_minimal);

namespace cereal {

template <class Archive>
std::string save_minimal(Archive const&, const wwiv::sdk::conf_set_t& t) {
  return t.to_string();
}

template <class Archive>
void load_minimal(Archive const&, wwiv::sdk::conf_set_t& t, const std::string& s) {
  t.from_string(s);
}

}


#endif
