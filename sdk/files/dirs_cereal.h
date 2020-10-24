/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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

#ifndef INCLUDED_SDK_FILES_DIRS_CEREAL_H
#define INCLUDED_SDK_FILES_DIRS_CEREAL_H

#include "core/cereal_utils.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/uuid_cereal.h"
#include "sdk/files/dirs.h"

namespace wwiv::sdk::files {


template <class Archive>
void serialize (Archive& ar, dir_area_t& d) {
  SERIALIZE(d, area_tag);
  SERIALIZE(d, net_uuid);
}

template <class Archive>
void serialize(Archive & ar, directory_55_t& s) {
  SERIALIZE(s, name);
  SERIALIZE(s, filename);
  SERIALIZE(s, path);
  SERIALIZE(s, dsl);
  SERIALIZE(s, age);
  SERIALIZE(s, dar);
  SERIALIZE(s, maxfiles);
  SERIALIZE(s, mask);
  SERIALIZE(s, area_tags);
}

template <class Archive>
void serialize(Archive & ar, directory_t& s) {
  SERIALIZE(s, name);
  SERIALIZE(s, filename);
  SERIALIZE(s, path);
  SERIALIZE(s, acs);
  SERIALIZE(s, maxfiles);
  SERIALIZE(s, mask);
  SERIALIZE(s, area_tags);
}


}


#endif
