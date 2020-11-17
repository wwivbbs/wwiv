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
/**************************************************************************/
#ifndef INCLUDED_SDK_CONF_CONF_SET_H
#define INCLUDED_SDK_CONF_CONF_SET_H

#include "core/stl.h"
#include <set>

namespace wwiv::sdk {

class conf_set_t final {
public:
  conf_set_t() = default;
  ~conf_set_t() = default;

  void insert(char k) { data_.insert(k); }
  void erase(char k) { data_.erase(k); }
  void toggle(char k) {
    if (data_.find(k) != std::end(data_)) {
      erase(k);
    } else {
      insert(k);
    }
  }
  bool contains(char k) const  { return stl::contains(data_, k); }

  std::set<char> data_;
};
}

#endif
