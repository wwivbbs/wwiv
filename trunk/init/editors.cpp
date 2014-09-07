/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include "init/editors.h"

#include <curses.h>
#include <cmath>
#include <cstdint>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "init/ifcns.h"
#include "init/init.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "init/utility.h"
#include "init/wwivinit.h"
#include "initlib/input.h"
#include "initlib/listbox.h"

using std::auto_ptr;
using std::string;
using std::vector;
using wwiv::strings::StringPrintf;

void edit_editor(int n) {
  out->Cls(ACS_CKBOARD);
  auto_ptr<CursesWindow> window(out->CreateBoxedWindow("External Editor Configuration", 15, 78));

  const vector<string> bbs_types = { "WWIV    ", "QuickBBS" };
  const int COL1_POSITION = 20;
  editorrec c = editors[n];

  EditItems items{
    new StringEditItem<char*>(COL1_POSITION, 1, 35, c.description, false),
    new ToggleEditItem<uint8_t>(COL1_POSITION, 2, bbs_types, &c.bbs_type),
    new StringEditItem<char*>(2, 4, 75, c.filename, false),
    new StringEditItem<char*>(2, 7, 75, c.filenamecon, false)
  };
  items.set_curses_io(out, window.get());

  int y = 1;
  window->PrintfXY(2, y++, "Description     : %s", c.description);
  window->PrintfXY(2, y++, "BBS Type        : %s", bbs_types.at(c.bbs_type).c_str());
  window->PrintfXY(2, y++, "Filename to run remotely:", c.filename);
  y+=2;
  window->PrintfXY(2, y++, "Filename to run locally:", c.filenamecon);
  y+=2;
  window->PrintfXY(2, y++, "%%1 = filename to edit");
  window->PrintfXY(2, y++, "%%2 = chars per line");
  window->PrintfXY(2, y++, "%%3 = lines per page");
  window->PrintfXY(2, y++, "%%4 = max lines");
  window->PrintfXY(2, y++, "%%5 = instance number");
  items.Run();

  editors[n] = c;
}

void extrn_editors() {
  bool done = false;
  do {
    out->Cls(ACS_CKBOARD);
    vector<ListBoxItem> items;
    for (int i = 0; i < initinfo.numeditors; i++) {
      items.emplace_back(StringPrintf("%d. %s", i + 1, editors[i].description));
    }
    CursesWindow* window = out->window();
    ListBox list(out, window, "Select Editor", static_cast<int>(floor(window->GetMaxX() * 0.8)), 
        static_cast<int>(floor(window->GetMaxY() * 0.8)), items, out->color_scheme());

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"D", "Delete"}, {"I", "Insert"} });
    ListBoxResult result = list.Run();
    if (result.type == ListBoxResultType::HOTKEY) {
      switch (result.hotkey) {
        case 'D': {
          if (initinfo.numeditors) {
            string prompt = StringPrintf("Delete '%s'", items[result.selected].text().c_str());
            bool yn = dialog_yn(window, prompt);
            if (!yn) {
              break;
            }
            for (int i1 = result.selected; i1 < initinfo.numeditors; i1++) {
              editors[i1] = editors[i1 + 1];
            }
            --initinfo.numeditors;
          }
        } break;
        case 'I': {
          if (initinfo.numeditors >= 10) {
            messagebox(out->window(), "Too many editors.");
            break;
          }
          string prompt = StringPrintf("Insert before which (1-%d) : ", initinfo.numeditors + 1);
          int i = dialog_input_number(out->window(), prompt, 1, initinfo.numeditors);
          if (i > 0 && i <= initinfo.numeditors + 1) {
            for (int i1 = initinfo.numeditors; i1 > i - 1; i1--) {
              editors[i1] = editors[i1 - 1];
            }
            ++initinfo.numeditors;
            memset(&editors[i-1], 0, sizeof(editorrec));
            edit_editor(i - 1);
          }
        } break;
      }
    } else if (result.type == ListBoxResultType::SELECTION) {
      edit_editor(result.selected);
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    }
  } while (!done);

  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%seditors.dat", syscfg.datadir);
  int hFile = open(szFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  write(hFile, editors, initinfo.numeditors * sizeof(editorrec));
  close(hFile);
}
