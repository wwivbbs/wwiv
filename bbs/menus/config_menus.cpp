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
#include "bbs/menus/config_menus.h"

#include "bbs/bbs.h"
#include "bbs/mmkey.h"
#include "bbs/sysoplog.h"
#include "common/com.h"
#include "common/input.h"
#include "common/output.h"
#include "core/findfiles.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "sdk/filenames.h"
#include "sdk/menus/menu.h"

#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv::bbs::menus {

static bool ValidateMenuSet(const std::string& menu_set) {
  // ensure the entry point exists
  return File::Exists(FilePath(FilePath(a()->config()->menudir(), menu_set), "main.mnu.json"));
}

static std::map<int, std::string> ListMenuDirs() {
  std::map<int, std::string> result;
  const auto menu_directory = a()->config()->menudir();
  const MenuDescriptions descriptions(menu_directory);

  bout.nl();
  bout << "|#1Available Menus Sets" << wwiv::endl
       << "|#9============================" << wwiv::endl;

  int num = 1;
  auto menus = FindFiles(FilePath(menu_directory, "*"), FindFiles::FindFilesType::directories);

  for (const auto& m : menus) {
    const auto& filename = m.name;
    const std::string description = descriptions.description(filename);
    bout.bprintf("|#2#%d |#1%-8.8s |#9%-60.60s\r\n", num, filename, description);
    result.emplace(num, filename);
    ++num;
  }
  bout.nl();
  bout.Color(0);
  return result;
}

void MenuSysopLog(const std::string& msg) {
  const std::string log_message = StrCat("*MENU* : ", msg);
  sysoplog() << log_message;
  bout << log_message << wwiv::endl;
}

void ConfigUserMenuSet() {
  bout.cls();
  bout.litebar("Configure Menus");
  bout.printfile(MENUWEL_NOEXT);
  bool done = false;
  while (!done) {
    bout.nl();
    bout << "|#11|#9) Menuset      :|#2 " << a()->user()->menu_set() << wwiv::endl;
    bout << "|#12|#9) Use hot keys :|#2 " << (a()->user()->hotkeys() ? "Yes" : "No ") << wwiv::endl;
    bout.nl();
    bout << "|#9(|#2Q|#9=|#1Quit|#9) : ";
    const auto chKey = onek("Q12?");

    switch (chKey) {
    case 'Q':
      done = true;
      break;
    case '1': {
      auto r = ListMenuDirs();
      bout.nl(2);
      bout << "|#9Enter the menu set to use : ";
      auto sel = bin.input_number<int>(1, 1, r.size(), false);
      if (const auto menuSetName = r.at(sel); ValidateMenuSet(menuSetName)) {
        MenuDescriptions descriptions(a()->config()->menudir());
        bout.nl();
        bout << "|#9Menu Set : |#2" << menuSetName << " :  |#1"
             << descriptions.description(menuSetName) << wwiv::endl;
        bout << "|#5Use this menu set? ";
        if (bin.noyes()) {
          a()->user()->set_menu_set(menuSetName);
          break;
        }
      }
      bout.nl();
      bout << "|#6That menu set does not exists, resetting to the default menu set" << wwiv::endl;
      bout.pausescr();
      if (a()->user()->menu_set().empty()) {
        a()->user()->set_menu_set("wwiv");
      }
    } break;
    case '2':
      a()->user()->set_hotkeys(!a()->user()->hotkeys());
      break;
    case '?':
      bout.printfile(MENUWEL_NOEXT);
      continue; // bypass the below cls()
    }
    bout.cls();
  }

  // Save current menu setup.
  a()->WriteCurrentUser();

  MenuSysopLog(fmt::format("Menu in use : {} - {}", a()->user()->menu_set(),
                            a()->user()->hotkeys() ? "Hot" : "Off"));
  bout.nl(2);
}


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
  return "";
}

bool MenuDescriptions::set_description(const std::string& name, const std::string& description) {
  descriptions_[name] = description;

  TextFile file(FilePath(menupath_, DESCRIPT_ION), "wt");
  if (!file.IsOpen()) {
    return false;
  }

  for (const auto& iter : descriptions_) {
    file.Write(fmt::format("{} {}", iter.first, iter.second));
  }
  return true;
}


} // namespace wwiv
