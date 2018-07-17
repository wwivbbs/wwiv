/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#include "wwivconfig/editors.h"

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

#include "wwivconfig/wwivconfig.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/wwivport.h"
#include "wwivconfig/utility.h"
#include "wwivconfig/wwivinit.h"
#include "localui/wwiv_curses.h"
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
  const vector<std::pair<uint8_t, string>> bbs_types = {{0, "WWIV"}, {1,"QuickBBS"}};
  constexpr int LABEL1_POSITION = 2;
  constexpr int LABEL1_WIDTH = 29;
  constexpr int COL1_POSITION = LABEL1_POSITION + LABEL1_WIDTH + 1;

  if (!(e.ansir & ansir_ansi)) {
    e.ansir |= ansir_emulate_fossil;
    e.ansir |= ansir_no_DOS;
    e.ansir |= ansir_ansi;
    e.ansir |= ansir_stdio;
    e.ansir |= ansir_temp_dir;
  }

  int y = 1;
  EditItems items{};
  items.add(new StringEditItem<char*>(COL1_POSITION, y++, 35, e.description, false));
  items.add(new ToggleEditItem<uint8_t>(COL1_POSITION, y++, bbs_types, &e.bbs_type));
  items.add(new FlagEditItem<uint8_t>(COL1_POSITION, y++, ansir_no_DOS, "No ", "Yes", &e.ansir));
  items.add(new FlagEditItem<uint8_t>(COL1_POSITION, y++, ansir_emulate_fossil, "Yes", "No ", &e.ansir));
  items.add(new FlagEditItem<uint8_t>(COL1_POSITION, y++, ansir_stdio, "Yes", "No ", &e.ansir));
  items.add(new FlagEditItem<uint8_t>(COL1_POSITION, y++, ansir_temp_dir, "Yes", "No ", &e.ansir));
  y++;
  items.add(new CommandLineItem(LABEL1_POSITION, y++, 75, e.filename));
  y += 2;
  items.add(new CommandLineItem(LABEL1_POSITION, y++, 75, e.filenamecon));

  y = 1;
  items.add_labels({new Label(2, y++, LABEL1_WIDTH, "Description:"),
                    new Label(2, y++, LABEL1_WIDTH, "BBS Type:"),
                    new Label(2, y++, LABEL1_WIDTH, "Use DOS Interrupts:"),
                    new Label(2, y++, LABEL1_WIDTH, "Emulate FOSSIL:"),
                    new Label(2, y++, LABEL1_WIDTH, "Use STDIO:"),
                    new Label(2, y++, LABEL1_WIDTH, "Temp Directory Working Dir:"),
                    new Label(2, y++, LABEL1_WIDTH, "Filename to run remotely:")});
  y+=2;
  items.add_labels({new Label(2, y++, LABEL1_WIDTH, "Filename to run locally:")});
  y+=2;
  items.add_labels(
      {new Label(2, y++, "%1 = filename to edit   %N = Node Number "),
       new Label(2, y++, "%2 = chars per line     %H = Socket Handle"),
       new Label(2, y++, "%3 = lines per page     %I = Temp directory"),
       new Label(2, y++, "%4 = max lines          Note: All Other Chain Parameters are allowed."),
       new Label(2, y++, "See http://docs.wwivbbs.org/en/latest/chains/parameters for the full list.")});
  items.Run("External Editor Configuration");
}

void extrn_editors(const wwiv::sdk::Config& config) {
  vector<editorrec> editors;
  DataFile<editorrec> file (config.datadir(), EDITORS_DAT);
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
    ListBox list(window, "Select Editor", items);

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
          editorrec e{};
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

  DataFile<editorrec> editors_dat(config.datadir(), EDITORS_DAT,
    File::modeReadWrite|File::modeBinary|File::modeCreateFile|File::modeTruncate,
    File::shareDenyReadWrite);
  if (editors_dat) {
    editors_dat.WriteVector(editors);
  }
}
