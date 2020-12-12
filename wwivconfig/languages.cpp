/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include "wwivconfig/languages.h"

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
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

static constexpr int MAX_LANGUAGES = 100;

static void edit_lang(const std::string& base, languagerec& n) {
  EditItems items{};
  int y = 1;
  items.add(new Label("Language name:"), new StringEditItem<char*>(19, n.name, EditLineMode::ALL),
            "Display name for this language", 1, y);
  y++;
  items.add(new Label("Data Directory:"), new FilePathItem(60, base, n.dir),
            "Language specific gfiles directory for this language", 1, y);
  y ++;
  items.add(new Label("Menu Directory:"), new FilePathItem(60, base, n.mdir),
            "Not Used Yet. We use DataDir/menus.", 1, y);
  items.relayout_items_and_labels();
  items.Run("Language Configuration");
};

static uint8_t get_next_langauge_num(const vector<languagerec>& languages) {
  uint8_t max_num = 1;
  std::set<uint8_t> nums;
  for (auto i = 0; i < wwiv::stl::ssize(languages); i++) {
    const auto& l = languages[i];
    max_num = std::max<uint8_t>(max_num, l.num);
    nums.insert(l.num);
  }
  if (max_num < 250) {
    return max_num + 1;
  }

  // Getting close to uint8_t::max() so let's reuse.
  for (uint8_t i = 1; i < 255; i++) {
    if (nums.find(i) == nums.end()) {
      return i;
    }
  }

  // Impossible
  return 255;
}

void edit_languages(const wwiv::sdk::Config& config) {
  vector<languagerec> languages;
  {
    DataFile<languagerec> file(FilePath(config.datadir(), LANGUAGE_DAT));
    if (file) {
      file.ReadVector(languages, MAX_LANGUAGES);
    }
  }

  bool done = false;
  do {
    curses_out->Cls(ACS_CKBOARD);
    vector<ListBoxItem> items;
    for (auto i = 0; i < wwiv::stl::ssize(languages); i++) {
      items.emplace_back(fmt::format("{} {} ({})", i + 1, languages[i].name, languages[i].dir));
    }
    CursesWindow* window = curses_out->window();
    ListBox list(window, "Select Language", items);

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"}});
    auto result = list.Run();
    if (result.type == ListBoxResultType::HOTKEY) {
      switch (result.hotkey) {
      case 'D': {
        if (languages.size() > 1) {
          auto prompt = fmt::format("Delete '{}' ?", items[result.selected].text());
          auto yn = dialog_yn(window, prompt);
          if (!yn) {
            break;
          }
          erase_at(languages, result.selected);
          if (languages.size() == 1) {
            languages[0].num = 0;
          }
        } else {
          messagebox(window, "You must leave at least one language.");
        }
      } break;
      case 'I':
        if (languages.size() >= MAX_LANGUAGES) {
          messagebox(window, "Too many languages.");
          break;
        }

        auto prompt = fmt::format("Insert before which (1-{}) : ", languages.size() + 1);
        auto i = dialog_input_number(curses_out->window(), prompt, 1, size_int(languages) + 1);
        // N.B. i is one based, result.selected is 0 based.
        if (i <= 0 || i > ssize(languages) + 1) {
          break;
        }

        languagerec l{};
        memset(&l, 0, sizeof(languagerec));
        to_char_array(l.name, "English");
        to_char_array(l.dir, config.gfilesdir());
        to_char_array(l.mdir, config.gfilesdir());
        l.num = get_next_langauge_num(languages);

        if (i > ssize(languages)) {
          languages.push_back(l);
          edit_lang(config.root_directory(), languages.back());
        } else {
          auto it = languages.begin();
          std::advance(it, i - 1);
          auto new_lang = languages.insert(it, l);
          edit_lang(config.root_directory(), *new_lang);
        }
        break;
      }
    } else if (result.type == ListBoxResultType::SELECTION) {
      edit_lang(config.root_directory(), languages[result.selected]);
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    }
  } while (!done);

  {
    DataFile<languagerec> file(FilePath(config.datadir(), LANGUAGE_DAT),
                               File::modeWriteOnly | File::modeBinary | File::modeCreateFile |
                                   File::modeTruncate,
                               File::shareDenyReadWrite);
    if (file) {
      file.WriteVector(languages, MAX_LANGUAGES);
    }
  }
}
