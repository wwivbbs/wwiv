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
#include "sdk/menus/menu_set.h"

#include "core/findfiles.h"
#include "core/jsonfile.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/filenames.h"
#include "sdk/menus/menu.h"
#include "sdk/menus/menus_cereal.h"

#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv::sdk::menus {

///////////////////////////////////////////////////////////////////////
//
// MenuSet
//

MenuSet56::MenuSet56(std::filesystem::path dir)
    : menuset_dir_(std::move(dir)), initialized_(true) {
  if (!Load()) {
    // Create default empty Menuset from DESCRIPTION.ION
    MenuDescriptions descr(menuset_dir_.parent_path());
    const auto name = menuset_dir_.filename().string();
    menu_set.name = name;
    menu_set.description = descr.description(name);
    if (!Save()) {
      LOG(WARNING) << "Failed to save newly created MenuSet: " << menuset_dir_;
    }
  }
}

MenuSet56::MenuSet56() { 
  menu_set.name = "UNKNOWN";
}

MenuSet56& MenuSet56::operator=(const MenuSet56& o) {
  initialized_ = o.initialized();
  menuset_dir_ = o.menuset_dir_;
  menu_set = o.menu_set;
  return *this;
}

MenuSet56::~MenuSet56() = default;

bool MenuSet56::Load() {
  const auto dir = FilePath(menuset_dir_, "menuset.json");
  JsonFile f(dir, "menuset", menu_set, 1);
  return f.Load();
}

bool MenuSet56::Save() {
  const auto dir = FilePath(menuset_dir_, "menuset.json");
  JsonFile f(dir, "menuset", menu_set, 1);
  return f.Save();
}

///////////////////////////////////////////////////////////////////////
//
// MenuDescriptions: DESCRIPT.ION format
//

MenuDescriptions::MenuDescriptions(const std::filesystem::path& menupath) : menupath_(menupath) {
  TextFile file(FilePath(menupath, DESCRIPT_ION), "rt");
  if (file.IsOpen()) {
    std::string s;
    while (file.ReadLine(&s)) {
      StringTrim(&s);
      const auto space = s.find(' ');
      if (s.empty() || space == std::string::npos) {
        continue;
      }
      descriptions_.emplace(ToStringLowerCase(s.substr(0, space)),
                            ToStringLowerCase(s.substr(space + 1)));
    }
  }
}

MenuDescriptions::~MenuDescriptions() = default;

std::string MenuDescriptions::description(const std::string& name) const {
  if (contains(descriptions_, name)) {
    return descriptions_.at(name);
  }
  return {};
}

} // namespace wwiv
