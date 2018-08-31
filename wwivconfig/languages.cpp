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
#include "wwivconfig/languages.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <memory>
#include <string>
#include <set>
#include <vector>
#include <sys/stat.h>

#include "core/datafile.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/stl.h"
#include "core/wwivport.h"
#include "wwivconfig/wwivconfig.h"
#include "wwivconfig/utility.h"
#include "sdk/vardec.h"
#include "localui/wwiv_curses.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

static constexpr int MAX_LANGUAGES = 100;

static void edit_lang(const std::string& bbsdir, languagerec& n) {
  constexpr int LABEL1_POSITION = 2;
  constexpr int LABEL1_WIDTH = 15;
  constexpr int COL1_POSITION = LABEL1_POSITION + LABEL1_WIDTH + 1;

  EditItems items{};
  int y = 1;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "Language name:"),
            new StringEditItem<char*>(COL1_POSITION, y, 19, n.name, EditLineMode::ALL));
  y++;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "Data Directory:"),
            new FilePathItem(LABEL1_POSITION, y + 1, 75, bbsdir, n.dir));
  y += 2;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH , "Menu Directory:"),
            new FilePathItem(LABEL1_POSITION, y+1, 75, bbsdir, n.mdir));
  items.Run("Language Configuration");
};

static uint8_t get_next_langauge_num(const vector<languagerec>& languages) {
  uint8_t max_num = 1;
  std::set<uint8_t> nums;
  for (std::size_t i = 0; i < languages.size(); i++) {
    const auto& l = languages[i];
    max_num = std::max<uint8_t>(max_num, l.num);
    nums.insert(l.num);
  }
  if (max_num < 250) {
    return max_num + 1;
  }

  // Getting close to uint8_t::max() so let's reuse.
  for (uint8_t i=1; i<255; i++) {
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
    out->Cls(ACS_CKBOARD);
    vector<ListBoxItem> items;
    for (std::size_t i = 0; i < languages.size(); i++) {
      items.emplace_back(StringPrintf("%d. %s (%s)", i + 1, languages[i].name, languages[i].dir));
    }
    CursesWindow* window = out->window();
    ListBox list(window, "Select Language", items);

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"} });
    ListBoxResult result = list.Run();
    if (result.type == ListBoxResultType::HOTKEY) {
      switch (result.hotkey) {
      case 'D': {
        if (languages.size() > 1) {
          string prompt = StringPrintf("Delete '%s' ?", items[result.selected].text().c_str());
          bool yn = dialog_yn(window, prompt);
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

        string prompt = StringPrintf("Insert before which (1-%d) : ", languages.size() + 1);
        size_t i = dialog_input_number(out->window(), prompt, 1, languages.size() + 1);
        // N.B. i is one based, result.selected is 0 based.
        if (i <= 0 || i > languages.size() + 1) {
          break;
        } 
        
        languagerec l;
        memset(&l, 0, sizeof(languagerec));
        to_char_array(l.name, "English");
        to_char_array(l.dir, config.gfilesdir());
        to_char_array(l.mdir, config.gfilesdir());
        l.num = get_next_langauge_num(languages);

        if (i > languages.size()) {
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
      File::modeWriteOnly | File::modeBinary | File::modeCreateFile | File::modeTruncate, File::shareDenyReadWrite);
    if (file) {
      file.WriteVector(languages, MAX_LANGUAGES);
    }
  }
}
