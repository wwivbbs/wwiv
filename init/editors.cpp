/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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

#include "init/init.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/wwivport.h"
#include "init/utility.h"
#include "init/wwivinit.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "sdk/filenames.h"

using std::unique_ptr;
using std::string;
using std::vector;
using wwiv::core::DataFile;
using namespace wwiv::stl;
using namespace wwiv::strings;

static void edit_editor(editorrec& e) {
  out->Cls(ACS_CKBOARD);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow("External Editor Configuration", 17, 78));

  const vector<std::pair<uint8_t, string>> bbs_types = {{0, "WWIV"}, {1,"QuickBBS"}};
  const int COL1_POSITION = 22;

  if (!(e.ansir & ansir_ansi)) {
    e.ansir |= ansir_emulate_fossil;
    e.ansir |= ansir_no_DOS;
    e.ansir |= ansir_ansi;
  }

  EditItems items{
    new StringEditItem<char*>(COL1_POSITION, 1, 35, e.description, false),
    new ToggleEditItem<uint8_t>(COL1_POSITION, 2, bbs_types, &e.bbs_type),
    new FlagEditItem<uint8_t>(COL1_POSITION, 3, ansir_no_DOS, "No ", "Yes", &e.ansir),
    new FlagEditItem<uint8_t>(COL1_POSITION, 4, ansir_emulate_fossil, "Yes", "No ", &e.ansir),
    new CommandLineItem(2, 6, 75, e.filename),
    new CommandLineItem(2, 9, 75, e.filenamecon)
  };
  items.set_curses_io(out, window.get());

  int y = 1;
  window->PutsXY(2, y++, "Description       : ");
  window->PutsXY(2, y++, "BBS Type          : ");
  window->PutsXY(2, y++, "Use DOS Interrupts: ");
  window->PutsXY(2, y++, "Emulate FOSSIL    : ");
  window->PutsXY(2, y++, "Filename to run remotely:");
  y+=2;
  window->PutsXY(2, y++, "Filename to run locally:");
  y+=2;
  window->PutsXY(2, y++, "%1 = filename to edit");
  window->PutsXY(2, y++, "%2 = chars per line");
  window->PutsXY(2, y++, "%3 = lines per page");
  window->PutsXY(2, y++, "%4 = max lines");
  items.Run();
}

void extrn_editors() {
  vector<editorrec> editors;
  DataFile<editorrec> file (syscfg.datadir, EDITORS_DAT);
  if (file) {
    file.ReadVector(editors, 10);
    file.Close();
  }

  bool done = false;
  do {
    out->Cls(ACS_CKBOARD);
    vector<ListBoxItem> items;
    for (size_t i = 0; i < editors.size(); i++) {
      items.emplace_back(StringPrintf("%d. %s", i + 1, editors[i].description));
    }
    CursesWindow* window = out->window();
    ListBox list(out, window, "Select Editor", static_cast<int>(floor(window->GetMaxX() * 0.8)), 
        static_cast<int>(floor(window->GetMaxY() * 0.8)), items, out->color_scheme());

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"} });
    ListBoxResult result = list.Run();
    if (result.type == ListBoxResultType::HOTKEY) {
      switch (result.hotkey) {
        case 'D': {
          if (editors.size()) {
            string prompt = StringPrintf("Delete '%s'", items[result.selected].text().c_str());
            bool yn = dialog_yn(window, prompt);
            if (!yn) {
              break;
            }
            erase_at(editors, result.selected);
          }
        } break;
        case 'I': {
          if (editors.size() >= 10) {
            messagebox(out->window(), "Too many editors.");
            break;
          }
          string prompt = StringPrintf("Insert before which (1-%d) : ", editors.size() + 1);
          size_t i = dialog_input_number(out->window(), prompt, 1, editors.size() + 1);
          editorrec e;
          memset(&e, 0, sizeof(editorrec));
          // N.B. i is one based, result.selected is 0 based.
          if (i <= 0 || i > editors.size() + 1) {
            break;
          } else if (i > editors.size()) {
            editors.push_back(e);
            edit_editor(editors.back());
          } else {
            auto it = editors.begin();
            std::advance(it, i - 1);
            auto new_editor_it = editors.insert(it, e);
            edit_editor(*new_editor_it);
          }
        } break;
      }
    } else if (result.type == ListBoxResultType::SELECTION) {
      edit_editor(editors[result.selected]);
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    }
  } while (!done);

  DataFile<editorrec> editors_dat(syscfg.datadir, EDITORS_DAT,
    File::modeReadWrite|File::modeBinary|File::modeCreateFile|File::modeTruncate,
    File::shareDenyReadWrite);
  if (editors_dat) {
    editors_dat.WriteVector(editors);
  }
}
