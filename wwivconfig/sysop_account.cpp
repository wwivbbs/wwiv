/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2017, WWIV Software Services           */
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

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <sys/stat.h>

#include "core/inifile.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/wwiv_curses.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/networks.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "wwivconfig/subacc.h"
#include "wwivconfig/utility.h"
#include "wwivconfig/wwivconfig.h"
#include "sdk/vardec.h"

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static const int COL1_LINE = 2;
static const int COL1_POSITION = 22;

void create_sysop_account(wwiv::sdk::Config& config) {
  out->Cls(ACS_CKBOARD);
  // unique_ptr<CursesWindow> window(out->CreateBoxedWindow("System Configuration", 8, 54));

  std::vector<uint8_t> newuser_colors{7, 11, 14, 13, 31, 10, 12, 9, 5, 3};
  std::vector<uint8_t> newuser_bwcolors{7, 15, 15, 15, 112, 15, 15, 7, 7, 7};

  IniFile ini(FilePath(config.root_directory(), "wwiv.ini"), {"WWIV"});
  if (ini.IsOpen()) {
    for (int i = 0; i < 10; i++) {
      {
        const string key_name = StringPrintf("%s[%d]", "NUCOLOR", i);
        uint8_t num = ini.value<uint8_t>(key_name);
        if (num != 0) {
          newuser_colors[i] = num;
        }
      }
      {
        const string key_name = StringPrintf("%s[%d]", "NUCOLORBW", i);
        uint8_t num = ini.value<uint8_t>(key_name);
        if (num != 0) {
          newuser_bwcolors[i] = num;
        }
      }
    }
  }
  UserManager usermanager(config);

  int y = 1;

  User u = {};
  u.ZeroUserData();
  User::CreateNewUserRecord(&u, config.newuser_sl(), config.newuser_dsl(),
                            config.newuser_restrict(), config.newuser_gold(), newuser_colors,
                            newuser_bwcolors);
  constexpr int LABEL_WIDTH = 19;
  EditItems items{};
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Sysop Name/Handle:"),
      new StringEditItem<unsigned char*>(COL1_POSITION, 1, 30, u.data.name, EditLineMode::UPPER_ONLY));
  ++y;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Real Name:"),
      new StringEditItem<unsigned char*>(COL1_POSITION, 2, 20, u.data.realname, EditLineMode::UPPER_ONLY));
  ++y;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Password:"),
            new StringEditItem<char*>(COL1_POSITION, 3, 8, u.data.pw, EditLineMode::UPPER_ONLY));
  ++y;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "BBS Phone Number:"),
            new StringEditItem<char*>(COL1_POSITION, 4, 12, u.data.phone, EditLineMode::UPPER_ONLY));
  items.Run("Create Sysop Account");

  u.data.sl = 255;
  u.data.dsl = 255;
  u.data.restrict = 0;
  u.SetStatusFlag(User::ansi);
  u.SetStatusFlag(User::status_color);
  usermanager.writeuser(&u, 1);

  {
    wwiv::sdk::Names names(config);
    names.Load();
    names.Add(u.GetName(), 1);
    names.Save();
  }

  statusrec_t statusrec{};
  if (read_status(config.datadir(), statusrec)) {
    statusrec.users++;
    save_status(config.datadir(), statusrec);
  }
}
