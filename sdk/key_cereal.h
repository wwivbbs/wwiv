/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                  Copyright (C)2020, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_KEY_CEREAL_H
#define INCLUDED_SDK_KEY_CEREAL_H

// ReSharper disable once CppUnusedIncludeDirective
#include "core/cereal_utils.h"
#include "sdk/key.h"

CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(wwiv::sdk::key_t, specialization::non_member_load_save_minimal);

namespace cereal {

template <class Archive>
std::string save_minimal(Archive const&, const wwiv::sdk::key_t& t) {
  if (t.key_ == 0) {
    return {};
  }
  return wwiv::strings::StrCat(t.key_);
}

template <class Archive>
void load_minimal(Archive const&, wwiv::sdk::key_t& t, const std::string& s) {
  t.key(s.empty() ? 0 : s.front());
}

}

#endif
