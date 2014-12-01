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
#include "init/languages.h"

#include <curses.h>
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
#include <sys/stat.h>

#include "core/strings.h"
#include "core/file.h"
#include "core/wwivport.h"
#include "init/ifcns.h"
#include "init/init.h"
#include "init/utility.h"
#include "init/wwivinit.h"
#include "initlib/input.h"
#include "initlib/listbox.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::strings::StringPrintf;

#define MAX_LANGUAGES 100


static void edit_lang(languagerec* n) {
  out->Cls(ACS_CKBOARD);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow("Language Configuration", 9, 78));
  const int COL1_POSITION = 19;

  EditItems items{
    new StringEditItem<char*>(COL1_POSITION, 1, 20, n->name, false),
    new FilePathItem(2, 3, 75, n->dir),
    new FilePathItem(2, 6, 75, n->mdir),
  };
  items.set_curses_io(out, window.get());

  int y = 1;
  window->PutsXY(2, y++, "Language name  : ");
  window->PutsXY(2, y++, "Data Directory : ");
  y+=2;
  window->PutsXY(2, y++, "Menu Directory : ");
  items.Run();
}

static uint8_t get_next_langauge_num(languagerec* languages, int size) {
  int max_num = 1;
  std::set<uint8_t> nums;
  for (int i=0; i < size; i++) {
    languagerec l = languages[i];
    max_num = std::max<uint8_t>(max_num, l.num);
    nums.insert(l.num);
  }
  if (max_num < 250) {
    return max_num + 1;
  }

  // Getting close to uint8_t::max() so let's reuse.
  for (int i=1; i<255; i++) {
    if (nums.find(i) == nums.end()) {
      return i;
    }
  }

  // Impossible
  return 255;
}

void edit_languages() {
  initinfo.num_languages = 0;
  unique_ptr<languagerec[]> languages(new languagerec[MAX_LANGUAGES]);
  File file(syscfg.datadir, LANGUAGE_DAT);
  if (file.Open(File::modeReadOnly|File::modeBinary)) {
    int num_read = file.Read(languages.get(), MAX_LANGUAGES * sizeof(languagerec));
    initinfo.num_languages = num_read / sizeof(languagerec);
    file.Close();
  }

  bool done = false;
  do {
    out->Cls(ACS_CKBOARD);
    vector<ListBoxItem> items;
    for (int i = 0; i < initinfo.num_languages; i++) {
      items.emplace_back(StringPrintf("%d. %s (%s)", i + 1, languages[i].name, languages[i].dir));
    }
    CursesWindow* window = out->window();
    ListBox list(out, window, "Select Language", static_cast<int>(floor(window->GetMaxX() * 0.8)), 
        static_cast<int>(floor(window->GetMaxY() * 0.8)), items, out->color_scheme());

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"} });
    ListBoxResult result = list.Run();
    if (result.type == ListBoxResultType::HOTKEY) {
      switch (result.hotkey) {
      case 'D': {
        if (initinfo.num_languages > 1) {
          string prompt = StringPrintf("Delete '%s' ?", items[result.selected].text().c_str());
          bool yn = dialog_yn(window, prompt);
          if (!yn) {
            break;
          }
          initinfo.num_languages--;
          for (int i1 = result.selected; i1 < initinfo.num_languages; i1++) {
            languages[i1] = languages[i1 + 1];
          }
          if (initinfo.num_languages == 1) {
            languages[0].num = 0;
          }
        } else {
          messagebox(window, "You must leave at least one language.");
        }
      } break;
      case 'I':
        if (initinfo.num_languages >= MAX_LANGUAGES) {
          messagebox(window, "Too many languages.");
          break;
        }
        string prompt = StringPrintf("Insert before which (1-%d) : ", initinfo.num_languages + 1);
        int i = dialog_input_number(out->window(), prompt, 1, initinfo.num_languages);
        if ((i > 0) && (i <= initinfo.num_languages + 1)) {
          for (int i1 = initinfo.num_languages; i1 > i - 1; i1--) {
            languages[i1] = languages[i1 - 1];
          }
          initinfo.num_languages++;
          strcpy(languages[i-1].name, "English");
          strncpy(languages[i-1].dir, syscfg.gfilesdir, sizeof(languages[i-1].dir) - 1);
          strncpy(languages[i-1].mdir, syscfg.gfilesdir, sizeof(languages[i-1].mdir) - 1);
          languages[i-1].num = get_next_langauge_num(languages.get(), initinfo.num_languages);
          edit_lang(&languages[i-1]);
        }
        break;
      }
    } else if (result.type == ListBoxResultType::SELECTION) {
      edit_lang(&languages[result.selected]);
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    }
  } while (!done);

  {
    File file(syscfg.datadir, LANGUAGE_DAT);
    if (file.Open(File::modeReadWrite|File::modeBinary|File::modeCreateFile|File::modeTruncate,
      File::shareDenyReadWrite)) {
      file.Write(languages.get(), initinfo.num_languages * sizeof(languagerec));
    }
  }
}
