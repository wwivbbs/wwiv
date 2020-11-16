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
#ifndef INCLUDED_SDK_CONF_CEREAL_H
#define INCLUDED_SDK_CONF_CEREAL_H

#include "core/cereal_utils.h"
#include "sdk/key_cereal.h"
#include "sdk/conf/conf.h"

namespace wwiv::sdk {

template <class Archive> void serialize(Archive& ar, conference_t& s) { 
  SERIALIZE(s, key);
  SERIALIZE(s, conf_name);
  SERIALIZE(s, acs);
}

template <class Archive> void serialize(Archive& ar, conference_file_t& s) { 
  SERIALIZE(s, subs);
  SERIALIZE(s, dirs);
}

}


#endif
