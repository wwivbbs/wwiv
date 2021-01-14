/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
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
#include "wwivconfig/editors.h"

#include "core/datafile.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "localui/edit_items.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/wwiv_curses.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

static void edit_editor(editorrec& e) {
  constexpr uint8_t bbs_type_wwiv = 0;
  constexpr uint8_t bbs_type_quickbbs = 1;

  constexpr int fossil_type_none = 0;
  constexpr int fossil_type_netfoss = 1;
  constexpr int fossil_type_syncfoss = 2;

  const std::vector<std::pair<uint8_t, string>> bbs_types = {{bbs_type_wwiv, "WWIV"},
                                                             {bbs_type_quickbbs, "QuickBBS"}};
  const std::vector<std::pair<int, string>> fossil_types = {
      {fossil_type_none, "No"},
      {fossil_type_syncfoss, "SyncFoss"},
      {fossil_type_netfoss, "NetFoss"},
  };

  if (e.old_ansir != 0 && e.ansir == 0) {
    // update to wide ansir.
    e.ansir = e.old_ansir;
  }
  if (!(e.ansir & ansir_ansi)) {
    e.ansir |= ansir_netfoss;
    e.ansir |= ansir_no_DOS;
    e.ansir |= ansir_ansi;
    e.ansir |= ansir_temp_dir;
  }

  auto fossil_type = fossil_type_none;
  if (e.ansir & ansir_emulate_fossil) {
    fossil_type = fossil_type_syncfoss;
  } else if (e.ansir & ansir_netfoss) {
    fossil_type = fossil_type_netfoss;
  }

  auto y = 1;
  EditItems items{};
  items.add(new Label("Name:"), new StringEditItem<char*>(35, e.description, EditLineMode::ALL),
            "The name of this editor, will be displayed to users", 1, y);
  ++y;
  items.add(new Label("BBS Type:"), new ToggleEditItem<uint8_t>(bbs_types, &e.bbs_type),
            "Type of BBS to emulate with editor control files:  WWIV or QuickBBS", 1, y);
#ifndef _WIN32
  ++y;
  items.add(new Label("Use STDIO:"),
            new FlagEditItem<uint16_t>(ansir_stdio, "Yes", "No ", &e.ansir),
            "Use stdin/stdout from the door for I/O", 1, y);
  // Clear the FOSSIL flags if they are set accidentally.
  e.ansir &= ~ansir_netfoss;
  e.ansir &= ~ansir_emulate_fossil;
#else
  ++y;
  items.add(new Label("FOSSIL:"), new ToggleEditItem<int>(fossil_types, &fossil_type),
            "Should a FOSSIL be emulated through NetFoss, SyncFoss, or None", 1, y);
  // Clear the stdio flag if it's set accidentally.
  e.ansir &= ~ansir_stdio;
#endif
  ++y;
  items.add(new Label("Use Temp:"),
            new FlagEditItem<uint16_t>(ansir_temp_dir, "Yes", "No ", &e.ansir),
            "Launch using the temp directory as the current directory (recommended)", 1, y);
  y++;
  items.add(new Label("Remote:"), new CommandLineItem(66, e.filename),
            "Commandline to use when executing this editor remotely", 1, y);
  y++;
  items.add(new Label("Local:"), new CommandLineItem(66, e.filenamecon),
            "Commandline to use when executing this editor locally", 1, y);

  y += 2;
  items.add(new MultilineLabel(R""""(%1 = filename to edit   %N = Node Number
%2 = chars per line     %H = Socket Handle
%3 = lines per page     %I = Temp directory
%4 = max lines

Note: All Other Chain Parameters are allowed. See 
http://docs.wwivbbs.org/en/latest/chains/parameters for the full list.
)""""),
            1, y);
  items.relayout_items_and_labels();
  items.Run("External Editor Configuration");

  switch (fossil_type) {
  case fossil_type_netfoss: {
    e.ansir |= ansir_no_DOS;
    e.ansir |= ansir_netfoss;
    e.ansir &= ~ansir_emulate_fossil;
  } break;
  case fossil_type_syncfoss: {
    e.ansir |= ansir_no_DOS;
    e.ansir &= ~ansir_netfoss;
    e.ansir |= ansir_emulate_fossil;
  } break;
  case fossil_type_none:
  default: {
    e.ansir &= ~ansir_netfoss;
    e.ansir &= ~ansir_emulate_fossil;
  }
  }
}

void extrn_editors(const wwiv::sdk::Config& config) {
  vector<editorrec> editors;
  if (auto file = DataFile<editorrec>(FilePath(config.datadir(), EDITORS_DAT))) {
    file.ReadVector(editors, 10);
    file.Close();
  }

  auto done = false;
  do {
    curses_out->Cls(ACS_CKBOARD);
    vector<ListBoxItem> items;
    for (size_t i = 0; i < editors.size(); i++) {
      items.emplace_back(fmt::format("{}. {}", i + 1, editors[i].description));
    }
    auto* window = curses_out->window();
    ListBox list(window, "Select Editor", items);

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"}});
    auto result = list.Run();
    if (result.type == ListBoxResultType::HOTKEY) {
      switch (result.hotkey) {
      case 'D': {
        if (!editors.empty()) {
          auto prompt = fmt::format("Delete '{}' ?", items[result.selected].text());
          bool yn = dialog_yn(window, prompt);
          if (!yn) {
            break;
          }
          erase_at(editors, result.selected);
        }
      } break;
      case 'I': {
        if (editors.size() >= 10) {
          messagebox(curses_out->window(), "Too many editors.");
          break;
        }
        auto prompt = fmt::format("Insert before which (1-{}) : ", editors.size() + 1);
        auto i = dialog_input_number(curses_out->window(), prompt, 1, size_int(editors) + 1);
        editorrec e{};
        memset(&e, 0, sizeof(editorrec));
        // N.B. i is one based, result.selected is 0 based.
        if (i <= 0 || i > wwiv::stl::ssize(editors) + 1) {
          break;
        }
        if (i > wwiv::stl::ssize(editors)) {
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

  DataFile<editorrec> editors_dat(FilePath(config.datadir(), EDITORS_DAT),
                                  File::modeReadWrite | File::modeBinary | File::modeCreateFile |
                                      File::modeTruncate,
                                  File::shareDenyReadWrite);
  if (editors_dat) {
    editors_dat.WriteVector(editors);
  }
}
