/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#ifndef INCLUDED_SDK_SUBS_CEREAL_H
#define INCLUDED_SDK_SUBS_CEREAL_H

#include "core/cereal_utils.h"
#include "sdk/subxtr.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/conf/conf_set_cereal.h"

namespace wwiv::sdk {

template <class Archive>
void serialize(Archive & ar, subboard_network_data_t& s) {
  SERIALIZE(s, stype);
  SERIALIZE(s, flags);
  SERIALIZE(s, net_num);
  SERIALIZE(s, host);
  SERIALIZE(s, category);
}

template <class Archive> void serialize(Archive& ar, subboard_t& s) { 
  SERIALIZE(s, name);
  SERIALIZE(s, filename);
  SERIALIZE(s, key);
  SERIALIZE(s, read_acs);
  SERIALIZE(s, post_acs);
  SERIALIZE(s, anony);
  SERIALIZE(s, maxmsgs);
  SERIALIZE(s, storage_type);
  SERIALIZE(s, nets);
  SERIALIZE(s, conf);
}

template <class Archive> void serialize(Archive& ar, subboard_52_t& s) { 
  SERIALIZE(s, name);
  SERIALIZE(s, filename);
  SERIALIZE(s, key);
  SERIALIZE(s, readsl);
  SERIALIZE(s, postsl);
  SERIALIZE(s, anony);
  SERIALIZE(s, age);
  SERIALIZE(s, maxmsgs);
  SERIALIZE(s, ar);
  SERIALIZE(s, storage_type);
  SERIALIZE(s, nets);
}


}


#endif
