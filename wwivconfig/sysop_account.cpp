/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2021, WWIV Software Services           */
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
#include "wwivconfig/sysop_account.h"

#include "core/file.h"
#include "core/inifile.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "localui/edit_items.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/wwiv_curses.h"
#include "sdk/names.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/vardec.h"
#include "wwivconfig/utility.h"
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::local::ui;
using namespace wwiv::sdk;
using namespace wwiv::strings;

void create_sysop_account(Config& config) {
  curses_out->Cls(ACS_CKBOARD);

  std::vector<uint8_t> newuser_colors{7, 11, 14, 13, 31, 10, 12, 9, 5, 3};
  std::vector<uint8_t> newuser_bwcolors{7, 15, 15, 15, 112, 15, 15, 7, 7, 7};

  IniFile ini(FilePath(config.root_directory(), "wwiv.ini"), {"WWIV"});
  if (ini.IsOpen()) {
    for (auto i = 0; i < 10; i++) {
      {
        const auto key_name = fmt::format("{}[{}]", "NUCOLOR", i);
        auto num = ini.value<uint8_t>(key_name);
        if (num != 0) {
          newuser_colors[i] = num;
        }
      }
      {
        const auto key_name = fmt::format("{}[{}]", "NUCOLORBW", i);
        auto num = ini.value<uint8_t>(key_name);
        if (num != 0) {
          newuser_bwcolors[i] = num;
        }
      }
    }
  }
  UserManager usermanager(config);

  auto y = 1;

  User u{};
  u.ZeroUserData();
  User::CreateNewUserRecord(&u, config.newuser_sl(), config.newuser_dsl(),
                            config.newuser_restrict(), config.newuser_gold(), newuser_colors,
                            newuser_bwcolors);
  EditItems items{};
  items.add(new Label("Sysop Name/Handle:"),
            new StringEditItem<unsigned char*>(30, u.data.name, EditLineMode::UPPER_ONLY), 1, y);
  ++y;
  items.add(new Label("Real Name:"),
            new StringEditItem<unsigned char*>(20, u.data.realname, EditLineMode::ALL), 1,
            y);
  ++y;
  items.add(new Label("Password:"),
            new StringEditItem<char*>(8, u.data.pw, EditLineMode::UPPER_ONLY), 1, y);
  ++y;
  items.add(new Label("BBS Phone Number:"),
            new StringEditItem<char*>(12, u.data.phone, EditLineMode::UPPER_ONLY), 1, y);

  items.relayout_items_and_labels();
  items.Run("Create Sysop Account");

  u.data.sl = 255;
  u.data.dsl = 255;
  u.data.restrict = 0;
  u.set_flag(User::flag_ansi);
  u.set_flag(User::status_color);
  u.set_flag(User::extraColor);
  // Enable the internal FSED and full screen reader for the sysop.
  u.default_editor(0xff);
  u.set_flag(User::fullScreenReader);
  usermanager.writeuser(&u, 1);

  {
    Names names(config);
    names.Load();
    names.Add(u.name(), 1);
    names.Save();
  }

  if (statusrec_t statusrec{}; read_status(config.datadir(), statusrec)) {
    statusrec.users++;
    save_status(config.datadir(), statusrec);
  }
}
