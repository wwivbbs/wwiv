/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*                 Copyright (C)2017, WWIV Software Services              */
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
#include "init/menus.h"

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <sys/stat.h>
#include <curses.h>

#include "bbs/keycodes.h"
#include "init/init.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "core/log.h"
#include "core/findfiles.h"
#include "core/strings.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/wwivport.h"
#include "init/subacc.h"
#include "init/utility.h"
#include "init/wwivinit.h"
#include "init/subacc.h"
#include "sdk/filenames.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/networks.h"
#include "sdk/menu.h"
#include "sdk/subxtr.h"

using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::fido;
using namespace wwiv::stl;
using namespace wwiv::strings;

static void edit_menu_item(MenuRec& m) {
  constexpr int LABEL_WIDTH = 12;
  constexpr int PADDING = 2;
  constexpr int COL1_LINE = PADDING;
  constexpr int COL2_LINE = COL1_LINE + LABEL_WIDTH;
  constexpr int COL3_LINE = COL2_LINE + PADDING + 16 + 6;
  constexpr int COL4_LINE = COL3_LINE + LABEL_WIDTH;

  static const vector<pair<uint8_t, string>> numbers_action = {
    { MENU_NUMFLAG_NOTHING, "Nothing" },
    { MENU_NUMFLAG_SUBNUMBER, "Set Sub Number" },
    { MENU_NUMFLAG_DIRNUMBER, "Set Dir Number" }
  };

  static const vector<pair<uint8_t, string>> logging_action = {
    { MENU_LOGTYPE_NONE, "Nothing" },
    { MENU_LOGTYPE_COMMAND, "Command" },
    { MENU_LOGTYPE_KEY, "Key" },
    { MENU_LOGTYPE_DESC, "Description" }
  };
  static const vector<pair<uint8_t, string>> help_action = {
    { MENU_HELP_DONTFORCE, "Never" },
    { MENU_HELP_FORCE, "Always" },
    { MENU_HELP_ONENTRANCE, "At Entrance" }
  };


  EditItems items{};
  const string title = StrCat("Menu: ", m.szKey);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow(title, 14, 76));
  items.set_curses_io(out, window.get());
  int y = 1;
  window->PrintfXY(COL1_LINE, y, "Menu Key  : ");
  items.add(new StringEditItem<char*>(COL2_LINE, y++, 10, m.szKey, true));
  window->PrintfXY(COL1_LINE, y, "Command   : ");
  items.add(new StringEditItem<char*>(COL2_LINE, y++, 60, m.szExecute, false));
  window->PrintfXY(COL1_LINE, y, "Menu Text : ");
  items.add(new StringEditItem<char*>(COL2_LINE, y++, 40, m.szMenuText, false));
  window->PrintfXY(COL1_LINE, y, "Help Text : ");
  items.add(new StringEditItem<char*>(COL2_LINE, y++, 60, m.szHelp, false));
  window->PrintfXY(COL1_LINE, y, "Sysop Log : ");
  items.add(new StringEditItem<char*>(COL2_LINE, y++, 50, m.szSysopLog, false));
  window->PrintfXY(COL1_LINE, y, "Instance  : ");
  items.add(new StringEditItem<char*>(COL2_LINE, y++, 60, m.szInstanceMessage, false));
  int col2y = y;
  window->PrintfXY(COL1_LINE, y, "Min SL    : ");
  items.add(new NumberEditItem<uint16_t>(COL2_LINE, y++, &m.nMinSL));
  window->PrintfXY(COL1_LINE, y, "Max SL    : ");
  items.add(new NumberEditItem<uint16_t>(COL2_LINE, y++, &m.iMaxSL));
  window->PrintfXY(COL1_LINE, y, "Min DSL   : ");
  items.add(new NumberEditItem<uint16_t>(COL2_LINE, y++, &m.nMinDSL));
  window->PrintfXY(COL1_LINE, y, "Max DSL   : ");
  items.add(new NumberEditItem<uint16_t>(COL2_LINE, y++, &m.iMaxDSL));
  window->PrintfXY(COL1_LINE, y, "AR        : ");
  items.add(new ArEditItem(COL2_LINE, y++, &m.uAR));
  window->PrintfXY(COL1_LINE, y, "DAR       : ");
  items.add(new ArEditItem(COL2_LINE, y++, &m.uDAR));
  
  y = col2y;
  window->PrintfXY(COL3_LINE, y, "Restr     : ");
  items.add(new RestrictionsEditItem(COL4_LINE, y++, &m.uRestrict));
  window->PrintfXY(COL3_LINE, y, "Sysop     : ");
  items.add(new BooleanEditItem(out, COL4_LINE, y++, &m.nSysop));
  window->PrintfXY(COL3_LINE, y, "CoSysop   : ");
  items.add(new BooleanEditItem(out, COL4_LINE, y++, &m.nCoSysop));
  window->PrintfXY(COL3_LINE, y, "Password  : ");
  items.add(new StringEditItem<char*>(COL4_LINE, y++, 20, m.szPassWord, true));

  items.Run();
}

class MenuItemsSubDialog : public BaseEditItem {
public:
  MenuItemsSubDialog(vector<MenuRec>& menu_items, int x, int y, const std::string& title)
    : BaseEditItem(x, y, 1), menu_items_(menu_items), title_(title), x_(x), y_(y) {};
  virtual ~MenuItemsSubDialog() {}

  virtual int Run(CursesWindow* window) {
    ScopeExit at_exit([] { out->footer()->SetDefaultFooter(); });
    try {
      bool done = false;
      int selected = -1;
      do {
        vector<ListBoxItem> items;
        for (size_t i = 1; i < menu_items_.size(); i++) {
          const auto& m = menu_items_.at(i);
          std::ostringstream ss;
          auto key = StringPrintf("(%s)", m.szKey);
          ss << "#" << i << " " << std::left << std::setw(12) << key 
             << " '" << m.szMenuText << "' [" << m.szExecute << "]";
          items.emplace_back(ss.str(), 0, i);
        }
        ListBox list(out, window, title_, static_cast<int>(floor(window->GetMaxX() * 0.8)),
          static_cast<int>(floor(window->GetMaxY() * 0.8)), items, out->color_scheme());

        list.set_additional_hotkeys("DIM");
        list.set_help_items({ 
          { "Esc", "Exit" },{ "Enter", "Edit" },
          { "D", "Delete" },{ "I", "Insert" },
          { "M", "Move" }
        });
        list.set_selected(selected);
        ListBoxResult result = list.Run();
        selected = list.selected();

        if (result.type == ListBoxResultType::SELECTION) {
          edit_menu_item(menu_items_[items[result.selected].data()]);
        }
        else if (result.type == ListBoxResultType::NO_SELECTION) {
          done = true;
        }
        else if (result.type == ListBoxResultType::HOTKEY) {
          const auto key = static_cast<char>(result.hotkey);
          const auto num = items[result.selected].data();
          const auto& item = menu_items_[items[result.selected].data()];
          switch (key) {
          case 'D': {
            dialog_yn(window, StrCat("Do you want to delete #", num, "[", item.szKey, "]"));
            wwiv::stl::erase_at(menu_items_, num);
          } break;
          case 'I': {
            if (num <= 0 || num >= size_int(items)) {
              menu_items_.push_back({});
            }
            else {
              MenuRec mr{};
              wwiv::stl::insert_at(menu_items_, num, mr);
            }
          } break;
          case 'M': {
            auto maxnum = menu_items_.size() + 1;
            auto prompt = StringPrintf("Move to before which (1-%d) : ", maxnum);
            auto new_pos = dialog_input_number(window, prompt, 1, maxnum);
            if (new_pos >= 1) {
              auto saved = menu_items_.at(num);
              if (wwiv::stl::erase_at(menu_items_, num)) {
                wwiv::stl::insert_at(menu_items_, new_pos, saved);
              }
            }
          } break;
          }
        }
      } while (!done);
    }
    catch (const std::exception& e) {
      LOG(ERROR) << e.what();
    }
    return 2;
  }
  virtual void Display(CursesWindow* window) const {
    window->PutsXY(x_, y_, "[Enter to Edit]");
  }

private:
  vector<MenuRec>& menu_items_;
  const std::string title_;
  int x_ = 0;
  int y_ = 0;
};

static void edit_menu(const Config& config, const std::string& menu_dir, const std::string& menu_name) {
  const int COL1_LINE = 2;
  const int COL2_LINE = COL1_LINE + 22;

  vector<MenuRec> menu_items;
  {
    DataFile<MenuRec> menu_file(menu_dir, menu_name);
    if (menu_file) {
      menu_file.ReadVector(menu_items);
    }
  }
  if (menu_items.empty()) {
    // Need to create a default menu header.
    menu_items.push_back({});
  }

  static const vector<pair<uint8_t, string>> numbers_action = {
    { MENU_NUMFLAG_NOTHING, "Nothing" },
    { MENU_NUMFLAG_SUBNUMBER, "Set Sub Number" },
    { MENU_NUMFLAG_DIRNUMBER, "Set Dir Number" }
  };

  static const vector<pair<uint8_t, string>> logging_action = {
    { MENU_LOGTYPE_NONE, "Nothing" },
    { MENU_LOGTYPE_COMMAND, "Command" },
    { MENU_LOGTYPE_KEY, "Key" },
    { MENU_LOGTYPE_DESC, "Description"}
  };
  static const vector<pair<uint8_t, string>> help_action = {
    { MENU_HELP_DONTFORCE, "Never" },
    { MENU_HELP_FORCE, "Always" },
    { MENU_HELP_ONENTRANCE, "At Entrance" }
  };

  auto item0 = menu_items.at(0);
  CHECK_EQ(sizeof(MenuRec), sizeof(MenuHeader));
  MenuHeader h{};
  memcpy(&h, &item0, sizeof(MenuRec));
  if (!IsEquals(h.szSig, "WWIV430")) {
    to_char_array(h.szSig, "WWIV430\x1a");
  }
  if (h.nVersion < 256) {
    h.nVersion = 256;
  }

  EditItems items{};
  const string title = StrCat("Menu: ", menu_name);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow(title, 19, 76));
  items.set_curses_io(out, window.get());
  int y = 1;
  window->PrintfXY(COL1_LINE, y, "Menu Description     : ");
  items.add(new StringEditItem<char*>(COL2_LINE, y++, 20, h.szMenuTitle, false));
  window->PrintfXY(COL1_LINE, y, "Deleted              : ");
  items.add(new FlagEditItem<uint8_t>(out, COL2_LINE, y++, MENU_FLAG_DELETED, "Yes", "No", &h.nFlags));
  window->PrintfXY(COL1_LINE, y, "Main Menu            : ");
  items.add(new FlagEditItem<uint8_t>(out, COL2_LINE, y++, MENU_FLAG_MAINMENU, "Yes", "No", &h.nFlags));
  window->PrintfXY(COL1_LINE, y, "What do Numbers do   : ");
  items.add(new ToggleEditItem<uint8_t>(out, COL2_LINE, y++, numbers_action, &h.nums));
  window->PrintfXY(COL1_LINE, y, "Logging Type         : ");
  items.add(new ToggleEditItem<uint8_t>(out, COL2_LINE, y++, logging_action, &h.nLogging));
  window->PrintfXY(COL1_LINE, y, "Force help to be on  : ");
  items.add(new ToggleEditItem<uint8_t>(out, COL2_LINE, y++, help_action, &h.nForceHelp));
  window->PrintfXY(COL1_LINE, y, "Enter Script         : ");
  items.add(new StringEditItem<char*>(COL2_LINE, y++, 50, h.szScript, false));
  window->PrintfXY(COL1_LINE, y, "Exit Script          : ");
  items.add(new StringEditItem<char*>(COL2_LINE, y++, 50, h.szExitScript, false));
  window->PrintfXY(COL1_LINE, y, "Minimum SL           : ");
  items.add(new NumberEditItem<uint16_t>(COL2_LINE, y++, &h.nMinSL));
  window->PrintfXY(COL1_LINE, y, "Minimum DSL          : ");
  items.add(new NumberEditItem<uint16_t>(COL2_LINE, y++, &h.nMinDSL));
  window->PrintfXY(COL1_LINE, y, "AR                   : ");
  items.add(new ArEditItem(COL2_LINE, y++, &h.uAR));
  window->PrintfXY(COL1_LINE, y, "DAR                  : ");
  items.add(new ArEditItem(COL2_LINE, y++, &h.uDAR));
  window->PrintfXY(COL1_LINE, y, "Restrictions         : ");
  items.add(new RestrictionsEditItem(COL2_LINE, y++, &h.uRestrict));
  window->PrintfXY(COL1_LINE, y, "Sysop                : ");
  items.add(new BooleanEditItem(out, COL2_LINE, y++, &h.nSysop));
  window->PrintfXY(COL1_LINE, y, "CoSysop              : ");
  items.add(new BooleanEditItem(out, COL2_LINE, y++, &h.nCoSysop));
  window->PrintfXY(COL1_LINE, y, "Password             : ");
  items.add(new StringEditItem<char*>(COL2_LINE, y++, 20, h.szPassWord, true));
  window->PrintfXY(COL1_LINE, y, "Edit Menu Commands   : ");
  items.add(new MenuItemsSubDialog(menu_items, COL2_LINE, y++, "[Edit Menu Items]"));

  items.Run();

  MenuRec r{};
  memcpy(&r, &h, sizeof(MenuHeader));
  menu_items[0] = r;

  DataFile<MenuRec> menu_file(menu_dir, menu_name,
    File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate,
    File::shareDenyReadWrite);
  if (menu_file) {
    menu_file.WriteVector(menu_items);
  }

}

static void select_menu(Config& config, const std::string& dir) {
  const auto full_dir_path = FilePath(config.menudir(), dir);
  auto menus = FindFiles(full_dir_path, "*", FindFilesType::files);
  int selected = -1;
  try {
    bool done = false;
    do {
      vector<ListBoxItem> items;
      for (const auto& m : menus) {
        auto lm = ToStringLowerCase(m.name);
        if (!ends_with(lm, ".mnu")) {
          continue;
        }
        items.emplace_back(m.name);
      }
      CursesWindow* window = out->window();
      auto title = StrCat("Select Menu from [", dir, "]");
      ListBox list(out, window, title, static_cast<int>(floor(window->GetMaxX() * 0.8)),
        static_cast<int>(floor(window->GetMaxY() * 0.8)), items, out->color_scheme());
      list.set_selected(selected);
      list.set_additional_hotkeys("DI");
      list.set_help_items({ { "Esc", "Exit" },{ "Enter", "Edit" },{ "D", "Delete" },{ "I", "Insert" } });
      ListBoxResult result = list.Run();
      selected = list.selected();

      if (result.type == ListBoxResultType::SELECTION) {
        const auto& menu_name = items[result.selected].text();
        edit_menu(config, full_dir_path, menu_name);
      }
      else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      }
      else if (result.type == ListBoxResultType::HOTKEY) {
        messagebox(window, StrCat("HotKey: ", static_cast<char>(result.hotkey)));
      }
    } while (!done);
  }
  catch (const std::exception& e) {
    LOG(ERROR) << e.what();
  }

}

void menus(wwiv::sdk::Config& config) {
  try {
    auto dirs = FindFiles(config.menudir(), "*", FindFilesType::directories);

    bool done = false;
    int selected = -1;
    do {
      vector<ListBoxItem> items;
      for (const auto& d : dirs) {
        if (starts_with(d.name, ".")) {
          continue;
        }
        items.emplace_back(d.name);
      }
      CursesWindow* window = out->window();
      ListBox list(out, window, "Select Menu Set", static_cast<int>(floor(window->GetMaxX() * 0.8)), 
          static_cast<int>(floor(window->GetMaxY() * 0.8)), items, out->color_scheme());
      list.set_selected(selected);
      list.selection_returns_hotkey(true);
      list.set_additional_hotkeys("DI");
      list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"} });
      ListBoxResult result = list.Run();
      selected = list.selected();

      if (result.type == ListBoxResultType::SELECTION) {
        const auto& sel_dir = items[result.selected].text();
        select_menu(config, sel_dir);
      } else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      } else if (result.type == ListBoxResultType::HOTKEY) {
        /*        case 'D':
        switch (result.hotkey) {
          if (networks.networks().size() > 1) {
            const string prompt = StringPrintf("Delete '%s'", networks.at(result.selected).name);
            bool yn = dialog_yn(window, prompt);
            if (yn) {
              yn = dialog_yn(window, "Are you REALLY sure? ");
              if (yn) {
                del_net(config, networks, result.selected);
              }
            }
          } else {
            messagebox(window, "You must leave at least one network.");
          }
          break;
        case 'I':
          if (!(syscfg.fnoffset && syscfg.fsoffset && syscfg.fuoffset)) {
            vector<string> lines{ "You must run the BBS once to set up ", "some variables before inserting a network." };
            messagebox(window, lines);
            break;
          }
          if (networks.networks().size() >= MAX_NETWORKS) {
            messagebox(window, "Too many networks.");
            break;
          }
          const string prompt = StringPrintf("Insert before which (1-%d) ? ", networks.networks().size() + 1);
          const size_t net_num = dialog_input_number(window, prompt, 1, networks.networks().size() + 1  );
          if (net_num > 0 && net_num <= networks.networks().size() + 1) {
            if (dialog_yn(window, "Are you sure? ")) {
              insert_net(config, networks, net_num - 1);
            }
          }
          break;
        }
        */
        }
    } while (!done);
  } catch (const std::exception& e) {
    LOG(ERROR) << e.what();
  }
}

