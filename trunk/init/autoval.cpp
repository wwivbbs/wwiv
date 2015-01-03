/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <curses.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <string>
#include <sys/stat.h>
#include <vector>

#include "init/init.h"
#include "initlib/input.h"
#include "initlib/listbox.h"
#include "bbs/wconstants.h"
#include "init/wwivinit.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "init/utility.h"

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::strings::StringPrintf;

static string create_autoval_line(int n) {
  char s3[81], ar[20], dar[20], r[20];
  valrec v = syscfg.autoval[n];
  strcpy(s3, restrict_string);
  for (int i = 0; i <= 15; i++) {
    if (v.ar & (1 << i)) {
      ar[i] = 'A' + i;
    } else {
      ar[i] = 32;
    }
    if (v.dar & (1 << i)) {
      dar[i] = 'A' + i;
    } else {
      dar[i] = 32;
    }
    if (v.restrict & (1 << i)) {
      r[i] = s3[i];
    } else {
      r[i] = 32;
    }
  }
  r[16] = 0;
  ar[16] = 0;
  dar[16] = 0;
  const string key = StringPrintf("ALT-F%d", n + 1);
  return StringPrintf("%-7s  %3d  %3d  %16s  %16s  %20s", key.c_str(), v.sl, v.dsl, ar, dar, r);
}

static void edit_autoval(int n) {
  out->Cls(ACS_CKBOARD);
  const string title = StringPrintf("Auto-validation data for: Alt-F%d", n + 1);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow(title, 7, 40));
  const int COL1_POSITION = 17;

  valrec v = syscfg.autoval[n];
  EditItems items{
    new NumberEditItem<uint8_t>(COL1_POSITION, 1, &v.sl),
    new NumberEditItem<uint8_t>(COL1_POSITION, 2, &v.dsl),
    new ArEditItem(COL1_POSITION, 3, &v.ar),
    new ArEditItem(COL1_POSITION, 4, &v.dar),
    new RestrictionsEditItem(COL1_POSITION, 5, &v.restrict),
  };
  items.set_curses_io(out, window.get());
  int y = 1;
  window->PutsXY(2, y++, "SL           : ");
  window->PutsXY(2, y++, "DSL          : ");
  window->PutsXY(2, y++, "AR           : ");
  window->PutsXY(2, y++, "DAR          : ");
  window->PutsXY(2, y++, "Restrictions : ");
  items.Run();
  syscfg.autoval[n] = v;
}

void autoval_levs() {
  bool done = false;
  do {
    out->Cls(ACS_CKBOARD);
    vector<ListBoxItem> items;
    for (int i = 0; i < 10; i++) {
      items.emplace_back(create_autoval_line(i));
    }
    CursesWindow* window(out->window());
    ListBox list(out, window, "Select AutoVal", static_cast<int>(floor(window->GetMaxX() * 0.99)), 
        static_cast<int>(floor(window->GetMaxY() * 0.8)), items, out->color_scheme());

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"} });
    ListBoxResult result = list.Run();

    if (result.type == ListBoxResultType::HOTKEY) {
    } else if (result.type == ListBoxResultType::SELECTION) {
      edit_autoval(result.selected);
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    }
  } while (!done);
  save_config();
}
