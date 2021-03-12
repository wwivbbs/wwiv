/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                  Copyright (C)2021, WWIV Software Services             */
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
#include "common/language.h"

#include "core/inifile.h"

wwiv::common::Language::Language(const std::filesystem::path& menu_path,
                                 const std::filesystem::path& gfiles)
    : menu_path_(menu_path), gfiles_(gfiles), mi(menu_path, {"lang"}), gi(gfiles, {"lang"}) {
}

std::string wwiv::common::Language::value(const std::string& key) const {
  if (auto v = mi.value<std::string>(key); !v.empty()) {
    return v;
  }
  return gi.value<std::string>(key);
}
