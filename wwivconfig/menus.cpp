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
#include "wwivconfig/menus.h"

#include "core/datafile.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/curses_win.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/menu.h"
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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
  int y = 1;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Menu Key:"),
            new StringEditItem<char*>(COL2_LINE, y, 10, m.szKey, EditLineMode::UPPER_ONLY));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Command:"),
            new StringEditItem<char*>(COL2_LINE, y, 60, m.szExecute, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Menu Text:"),
            new StringEditItem<char*>(COL2_LINE, y, 40, m.szMenuText, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Help Text:"),
            new StringEditItem<char*>(COL2_LINE, y, 60, m.szHelp, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Sysop Log:"),
            new StringEditItem<char*>(COL2_LINE, y, 50, m.szSysopLog, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Instance :"),
            new StringEditItem<char*>(COL2_LINE, y, 60, m.szInstanceMessage, EditLineMode::ALL));
  y++;
  const auto col2y = y;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Min SL:"),
            new NumberEditItem<uint16_t>(COL2_LINE, y, &m.nMinSL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Max SL:"),
            new NumberEditItem<uint16_t>(COL2_LINE, y, &m.iMaxSL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Min DSL:"),
            new NumberEditItem<uint16_t>(COL2_LINE, y, &m.nMinDSL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Max DSL:"),
            new NumberEditItem<uint16_t>(COL2_LINE, y, &m.iMaxDSL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "AR:"),
            new ArEditItem(COL2_LINE, y, &m.uAR));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "DAR:"),
            new ArEditItem(COL2_LINE, y, &m.uDAR));
  y++;
  
  y = col2y;
  items.add(new Label(COL3_LINE, y, LABEL_WIDTH, "Restr:"),
            new RestrictionsEditItem(COL4_LINE, y, &m.uRestrict));
  y++;
  items.add(new Label(COL3_LINE, y, LABEL_WIDTH, "Sysop?"),
            new BooleanEditItem(COL4_LINE, y, &m.nSysop));
  y++;
  items.add(new Label(COL3_LINE, y, LABEL_WIDTH, "CoSysop?"),
            new BooleanEditItem(COL4_LINE, y, &m.nCoSysop));
  y++;
  items.add(new Label(COL3_LINE, y, LABEL_WIDTH, "Password:"),
            new StringEditItem<char*>(COL4_LINE, y, 20, m.szPassWord, EditLineMode::UPPER_ONLY));

  items.Run(StrCat("Menu: ", m.szKey));
}

class MenuItemsSubDialog : public BaseEditItem {
public:
  MenuItemsSubDialog(vector<MenuRec>& menu_items, int x, int y, const std::string& title)
    : BaseEditItem(x, y, 1), menu_items_(menu_items), title_(title), x_(x), y_(y) {};
  virtual ~MenuItemsSubDialog() = default;

  EditlineResult Run(CursesWindow* window) override {
    ScopeExit at_exit([] { out->footer()->SetDefaultFooter(); });
    try {
      bool done = false;
      int selected = -1;
      do {
        vector<ListBoxItem> items;
        for (size_t i = 1; i < menu_items_.size(); i++) {
          const auto& m = menu_items_.at(i);
          std::ostringstream ss;
          auto key = fmt::format("({})", m.szKey);
          ss << "#" << i << " " << std::left << std::setw(12) << key 
             << " '" << m.szMenuText << "' [" << m.szExecute << "]";
          items.emplace_back(ss.str(), 0, i);
        }
        ListBox list(window, title_, items);

        list.set_additional_hotkeys("DIM");
        list.set_help_items({ 
          { "Esc", "Exit" },{ "Enter", "Edit" },
          { "D", "Delete" },{ "I", "Insert" },
          { "M", "Move" }
        });
        list.set_selected(selected);
        auto result = list.Run();
        selected = list.selected();

        if (result.type == ListBoxResultType::SELECTION) {
          edit_menu_item(menu_items_[items[result.selected].data()]);
        }
        else if (result.type == ListBoxResultType::NO_SELECTION) {
          done = true;
        }
        else if (result.type == ListBoxResultType::HOTKEY) {
          const auto key = static_cast<char>(result.hotkey);
          const auto num = result.selected > 0 ? items[result.selected].data() : 0;
          switch (key) {
          case 'D': {
            const auto& item = menu_items_[items[result.selected].data()];
            if (dialog_yn(window, StrCat("Do you want to delete #", num, "[", item.szKey, "]"))) {
              wwiv::stl::erase_at(menu_items_, num);
            }
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
            auto prompt = fmt::format("Move to before which (1-{}) : ", maxnum);
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
    return EditlineResult::NEXT;
  }

  void Display(CursesWindow* window) const override {
    window->PutsXY(x_, y_, "[Enter to Edit]");
  }

private:
  vector<MenuRec>& menu_items_;
  const std::string title_;
  int x_ = 0;
  int y_ = 0;
};

static void edit_menu(const std::filesystem::path& menu_dir, const std::string& menu_name) {
  vector<MenuRec> menu_items;
  {
    DataFile<MenuRec> menu_file(PathFilePath(menu_dir, menu_name));
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
  MenuHeader h{};
  memcpy(&h, &item0, sizeof(MenuRec));
  if (!IsEquals(h.szSig, "WWIV430")) {
    to_char_array(h.szSig, "WWIV430\x1a");
  }
  if (h.nVersion < 256) {
    h.nVersion = 256;
  }

  constexpr int COL1_LINE = 2;
  constexpr int LABEL1_WIDTH = 20;
  constexpr int COL2_LINE = COL1_LINE + LABEL1_WIDTH + 1;

  EditItems items{};
  const string title = StrCat("Menu: ", menu_name);
  int y = 1;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "Menu Description:"),
            new StringEditItem<char*>(COL2_LINE, y, 20, h.szMenuTitle, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "Deleted:"),
            new FlagEditItem<uint8_t>(COL2_LINE, y, MENU_FLAG_DELETED, "Yes", "No", &h.nFlags));
  y++;
  items.add(
      new Label(COL1_LINE, y, LABEL1_WIDTH, "Main Menu:"),
            new FlagEditItem<uint8_t>(COL2_LINE, y, MENU_FLAG_MAINMENU, "Yes", "No", &h.nFlags));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "What do Numbers do:"),
            new ToggleEditItem<uint8_t>(COL2_LINE, y, numbers_action, &h.nums));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "Logging Type:"),
            new ToggleEditItem<uint8_t>(COL2_LINE, y, logging_action, &h.nLogging));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "Force help to be on:"),
            new ToggleEditItem<uint8_t>(COL2_LINE, y, help_action, &h.nForceHelp));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "Enter Script:"),
            new StringEditItem<char*>(COL2_LINE, y, 50, h.szScript, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "Exit Script:"),
            new StringEditItem<char*>(COL2_LINE, y, 50, h.szExitScript, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "Minimum SL:"),
            new NumberEditItem<uint16_t>(COL2_LINE, y, &h.nMinSL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "Minimum DSL:"),
            new NumberEditItem<uint16_t>(COL2_LINE, y, &h.nMinDSL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "AR:"),
            new ArEditItem(COL2_LINE, y, &h.uAR));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "DAR:"),
            new ArEditItem(COL2_LINE, y, &h.uDAR));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "Restrictions:"),
            new RestrictionsEditItem(COL2_LINE, y, &h.uRestrict));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "Sysop?"),
            new BooleanEditItem(COL2_LINE, y, &h.nSysop));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "CoSysop?"),
            new BooleanEditItem(COL2_LINE, y, &h.nCoSysop));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "Password:"),
            new StringEditItem<char*>(COL2_LINE, y, 20, h.szPassWord, EditLineMode::UPPER_ONLY));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL1_WIDTH, "Edit Menu Commands:"),
            new MenuItemsSubDialog(menu_items, COL2_LINE, y, "[Edit Menu Items]"));

  items.Run(title);

  MenuRec r{};
  memcpy(&r, &h, sizeof(MenuHeader));
  menu_items[0] = r;

  DataFile<MenuRec> menu_file(PathFilePath(menu_dir, menu_name),
                              File::modeReadWrite | File::modeBinary | File::modeCreateFile |
                              File::modeTruncate, File::shareDenyReadWrite);
  if (menu_file) {
    menu_file.WriteVector(menu_items);
  }

}

static void select_menu(const std::string& menu_dir, const std::string& dir) {
  const auto full_dir_path = PathFilePath(menu_dir, dir);
  auto menus = FindFiles(PathFilePath(full_dir_path, "*"), FindFilesType::files);
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
      ListBox list(window, title, items);
      list.set_selected(selected);
      list.set_additional_hotkeys("DI");
      list.set_help_items({ { "Esc", "Exit" },{ "Enter", "Edit" },{ "D", "Delete" },{ "I", "Insert" } });
      ListBoxResult result = list.Run();
      selected = list.selected();

      if (result.type == ListBoxResultType::SELECTION) {
        const auto& menu_name = items[result.selected].text();
        edit_menu(full_dir_path, menu_name);
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

void menus(const std::string& menu_dir) {
  try {
    auto dirs = FindFiles(PathFilePath(menu_dir, "*"), FindFilesType::directories);

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
      ListBox list(window, "Select Menu Set", items);
      list.set_selected(selected);
      list.selection_returns_hotkey(true);
      list.set_additional_hotkeys("DI");
      list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"} });
      ListBoxResult result = list.Run();
      selected = list.selected();

      if (result.type == ListBoxResultType::SELECTION) {
        const auto& sel_dir = items[result.selected].text();
        select_menu(menu_dir, sel_dir);
      } else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      } else if (result.type == ListBoxResultType::HOTKEY) {
        /*        case 'D':
        switch (result.hotkey) {
          if (networks.networks().size() > 1) {
            const string prompt = fmt::format("Delete '{}'", networks.at(result.selected).name);
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
          const auto prompt = fmt::format("Insert before which (1-{}) ? ", networks.networks().size() + 1);
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

