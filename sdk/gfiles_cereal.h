/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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

#ifndef INCLUDED_SDK_GFILES_CEREAL_H
#define INCLUDED_SDK_GFILES_CEREAL_H

#include "core/cereal_utils.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/gfiles.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/conf/conf_set_cereal.h"

namespace wwiv::sdk {


template <class Archive>
void serialize (Archive& ar, gfile_t& d) {
  SERIALIZE(d, filename);
  SERIALIZE(d, daten);
  SERIALIZE(d, description);
}

template <class Archive>
void serialize(Archive & ar, gfile_dir_t& s) {
  SERIALIZE(s, name);
  SERIALIZE(s, filename);
  SERIALIZE(s, acs);
  SERIALIZE(s, maxfiles);
  SERIALIZE(s, files);
}


}


#endif
