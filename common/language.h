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
#ifndef INCLUDED_COMMON_LANGUAGE_H
#define INCLUDED_COMMON_LANGUAGE_H

#include "core/inifile.h"

#include <filesystem>
#include <string>

namespace  wwiv::common {

class Language {
public:
  Language(const std::filesystem::path& menu_path, const std::filesystem::path& gfiles);
  [[nodiscard]] std::string value(const std::string& key) const;
  [[nodiscard]] std::string value(const std::string& key, const std::string& default_value) const;

private:
  core::IniFile mi;
  core::IniFile gi;
};

}

#endif