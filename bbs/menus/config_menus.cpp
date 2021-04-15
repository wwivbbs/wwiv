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

#include "bbs/acs.h"
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
#include "sdk/menus/menu_set.h"

#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::menus;
using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv::bbs::menus {

static bool ValidateMenuSet(const std::string& menu_set) {
  // ensure the entry point exists
  return File::Exists(FilePath(FilePath(a()->config()->menudir(), menu_set), "main.mnu.json"));
}

static std::map<int, menu_set_t> ListMenuDirs() {
  std::map<int, menu_set_t> result;

  bout.nl();
  bout << "|#1Available Menus Sets" << wwiv::endl
       << "|#9============================" << wwiv::endl;

  int num = 1;
  const auto menu_directory = a()->config()->menudir();
  auto menus = FindFiles(FilePath(menu_directory, "*"), FindFiles::FindFilesType::directories);

  for (const auto& d : menus) {
    MenuSet56 m(FilePath(menu_directory, d.name));
    if (!check_acs(m.menu_set.acs)) {
      VLOG(1) << "check acs failed for: " << d.name;
      continue;
    }
    bout.format("|#2#{} |#1{:<8.8} |#9{:<60.60}\r\n", num, m.menu_set.name, m.menu_set.description);
    result.emplace(num, m.menu_set);
    ++num;
  }
  bout.nl();
  bout.Color(0);
  return result;
}

/**
 * Sets a menuset to one specified by 'name', returning true on success and
 * false otherwise.
 */
static bool SetMenuSet(const menu_set_t& m) {
  if (!ValidateMenuSet(m.name)) {
    return false;
  }

  if (!check_acs(m.acs)) {
    return false;
  }

  a()->user()->set_menu_set(m.name);
  sysoplog() << fmt::format("Menu in use : {} - {}", a()->user()->menu_set(),
                           a()->user()->hotkeys() ? "Hot" : "Off");
  // Save current menu setup.
  return a()->WriteCurrentUser();
}

void ConfigUserMenuSet(const std::string& data) {
  if (!data.empty() && ValidateMenuSet(data)) {
    // Try to set the menu based on data first.
    MenuSet56 m(FilePath(a()->config()->menudir(), data));
    if (SetMenuSet(m.menu_set)) {
      // Success at setting the menuset, no need to query the caller.
      return;
    }
  }

  bout.cls();
  bout.litebar("Configure Menus");
  bout.printfile(MENUWEL_NOEXT);
  bool done = false;

  const auto r = ListMenuDirs();
  if (r.size() == 1) {
    if (const auto m = std::begin(r)->second; ValidateMenuSet(m.name)) {
      bout << "|#5You are currently using the only available menuset." << wwiv::endl << wwiv::endl;
      SetMenuSet(m);
      bout.pausescr();
      return;
    }
  }

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
      bout.nl(2);
      bout << "|#9Enter the menu set to use : ";
      auto sel = bin.input_number<int>(1, 1, r.size(), false);
      if (const auto m = r.at(sel); ValidateMenuSet(m.name)) {
        bout.nl();
        bout.format("|#9Menu Set : |#2{:<8.8} :  |#1{}|#0\r\n", m.name, m.description);
        bout << "|#5Use this menu set? ";
        if (bin.noyes()) {
          SetMenuSet(m);
          break;
        }
      }
      bout.nl();
      bout << "|#6That menu set does not exists. Resetting to the default menu set." << wwiv::endl;
      bout.pausescr();
      if (a()->user()->menu_set().empty()) {
        MenuSet56 wwiv_menu(FilePath(a()->config()->menudir(), "wwiv"));
        SetMenuSet(wwiv_menu.menu_set);
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

  bout.nl(2);
}

} // namespace wwiv
