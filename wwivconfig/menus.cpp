/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2017-2020, WWIV Software Services           */
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

#include "curses.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "localui/curses_win.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "local_io/keycodes.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/menus/menu.h"
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using std::pair;
using std::string;
using std::unique_ptr;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::fido;
using namespace wwiv::stl;
using namespace wwiv::strings;

  static const std::vector<pair<wwiv::sdk::menus::menu_numflag_t, std::string>> numbers_action = {
    { menus::menu_numflag_t::none, "Nothing" },
    { menus::menu_numflag_t::subs, "Set Sub Number" },
    { menus::menu_numflag_t::dirs, "Set Dir Number" }
  };

  static const std::vector<pair<menus::menu_logtype_t, std::string>> logging_action = {
    { menus::menu_logtype_t::none, "Nothing" },
    { menus::menu_logtype_t::command, "Command" },
    { menus::menu_logtype_t::key, "Key" },
    { menus::menu_logtype_t::description, "Description"}
  };
  static const std::vector<pair<menus::menu_help_display_t, std::string>> help_action = {
    { menus::menu_help_display_t::never, "Never" },
    { menus::menu_help_display_t::always, "Always" },
    { menus::menu_help_display_t::on_entrance, "On Entrance" }
  };

static void edit_menu_action(menus::menu_action_56_t& a) {
  constexpr auto LABEL_WIDTH = 6;
  constexpr auto PADDING = 2;
  constexpr auto COL1_LINE = PADDING;
  constexpr auto COL2_LINE = COL1_LINE + LABEL_WIDTH;

  EditItems items{};
  auto y = 1;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Cmd:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 40, a.cmd, EditLineMode::LOWER));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Data:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 70, a.data, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "ACS:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 70, a.acs, EditLineMode::ALL));

  items.Run("Edit Action");
}

class ActionSubDialog final : public BaseEditItem {
public:
  ActionSubDialog(std::vector<menus::menu_action_56_t>& actions, int x, int y, std::string title)
    : BaseEditItem(x, y, 1), actions_(actions), title_(std::move(title)) {}
  ~ActionSubDialog() override = default;

  void RunSubDialog(CursesWindow* window) {
    auto done = false;
    auto selected = -1;
    do {
      std::vector<ListBoxItem> items;
      for (auto i = 0; i < wwiv::stl::ssize(actions_); i++) {
        const auto& m = actions_.at(i);
        auto s = fmt::format("{}:{}", m.cmd, m.data);
        items.emplace_back(s, 0, i);
      }
      ListBox list(window, title_, items);

      list.set_additional_hotkeys("DIM");
      list.set_help_items({ 
        { "Esc", "Exit" },{ "Enter", "Edit" },
        { "D", "Delete" },{ "I", "Insert" },
        { "M", "Move" }
      });
      list.set_selected(selected);
      const auto result = list.Run();
      selected = list.selected();

      if (result.type == ListBoxResultType::SELECTION) {
        edit_menu_action(actions_[items[result.selected].data()]);
      }
      else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      }
      else if (result.type == ListBoxResultType::HOTKEY) {
        const auto key = static_cast<char>(result.hotkey);
        const auto num = result.selected > 0 ? items[result.selected].data() : 0;
        switch (key) {
        case 'D': {
          if (actions_.empty()) {
            break;
          }
          const auto& item = actions_[num];
          if (dialog_yn(window, StrCat("Do you want to delete #", num, " [", item.cmd, "]"))) {
            erase_at(actions_, num);
          }
        } break;
        case 'I': {
          if (num <= 0 || num >= ssize(items)) {
            actions_.push_back({});
          }
          else {
            menus::menu_action_56_t mr{};
            insert_at(actions_, num, mr);
          }
        } break;
        case 'M': {
          auto maxnum = size_int(actions_) + 1;
          auto prompt = fmt::format("Move to before which (1-{}) : ", maxnum);
          auto new_pos = dialog_input_number(window, prompt, 1, maxnum);
          if (new_pos >= 1) {
            auto saved = actions_.at(num);
            if (erase_at(actions_, num)) {
              insert_at(actions_, new_pos, saved);
            }
          }
        } break;
        }
      }
    } while (!done);
  }

  EditlineResult Run(CursesWindow* window) override {
    ScopeExit at_exit([] { curses_out->footer()->SetDefaultFooter(); });
    curses_out->footer()->ShowHelpItems(0, {{"Esc", "Exit"}, {"ENTER", "Edit Items (opens new dialog)."}});
    window->GotoXY(x_, y_);
    const auto ch = window->GetChar();
    if (ch == KEY_ENTER || ch == TAB || ch == 13) {
      try {
        RunSubDialog(window);
      }
      catch (const std::exception& e) {
        LOG(ERROR) << e.what();
      }
    } else if (ch == KEY_UP || ch == KEY_BTAB) {
      return EditlineResult::PREV;
    }
    return EditlineResult::NEXT;
  }


  void Display(CursesWindow* window) const override {
    window->PutsXY(x_, y_, fmt::format("[Edit] {} actions.", actions_.size()));
  }

private:
  std::vector<menus::menu_action_56_t>& actions_;
  const std::string title_;
};

static void edit_menu_item(menus::menu_item_56_t& m) {
  constexpr auto LABEL_WIDTH = 12;
  constexpr auto PADDING = 2;
  constexpr auto COL1_LINE = PADDING;
  constexpr auto COL2_LINE = COL1_LINE + LABEL_WIDTH;

  EditItems items{};
  auto y = 1;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Menu Key:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 10, m.item_key, EditLineMode::UPPER_ONLY));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Menu Text:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 60, m.item_text, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Help Text:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 60, m.help_text, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Sysop Log:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 50, m.log_text, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Instance :"),
            new StringEditItem<std::string&>(COL2_LINE, y, 60, m.instance_message, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "ACS:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 60, m.acs, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Password:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 20, m.password, EditLineMode::UPPER_ONLY));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Actions:"),
            new ActionSubDialog(m.actions, COL2_LINE, y, "[Edit Actions]"));


  items.Run(StrCat("Menu: ", m.item_key));
}

class MenuItemsSubDialog : public BaseEditItem {
public:
  MenuItemsSubDialog(std::vector<menus::menu_item_56_t>& menu_items, int x, int y, const std::string& title)
    : BaseEditItem(x, y, 1), menu_items_(menu_items), title_(title), x_(x), y_(y) {}
  ~MenuItemsSubDialog() override = default;

  EditlineResult Run(CursesWindow* window) override {
    ScopeExit at_exit([] { curses_out->footer()->SetDefaultFooter(); });
    try {
      auto done = false;
      auto selected = -1;
      do {
        std::vector<ListBoxItem> items;
        for (auto i = 1; i < wwiv::stl::ssize(menu_items_); i++) {
          const auto& m = menu_items_.at(i);
          auto key = fmt::format("({})", m.item_key);
          auto s = fmt::format("#{} {:<12} '{}'", i, key, m.item_text);
          if (!m.actions.empty()) {
            s += fmt::format(" [{}]", m.actions.front().cmd);
          }
          items.emplace_back(s, 0, i);
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
            if (dialog_yn(window, StrCat("Do you want to delete #", num, "[", item.item_key, "]"))) {
              erase_at(menu_items_, num);
            }
          } break;
          case 'I': {
            if (num <= 0 || num >= ssize(items)) {
              menu_items_.push_back({});
            }
            else {
              menus::menu_item_56_t mr{};
              insert_at(menu_items_, num, mr);
            }
          } break;
          case 'M': {
            auto maxnum = size_int(menu_items_) + 1;
            auto prompt = fmt::format("Move to before which (1-{}) : ", maxnum);
            auto new_pos = dialog_input_number(window, prompt, 1, maxnum);
            if (new_pos >= 1) {
              auto saved = menu_items_.at(num);
              if (erase_at(menu_items_, num)) {
                insert_at(menu_items_, new_pos, saved);
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
  std::vector<menus::menu_item_56_t>& menu_items_;
  const std::string title_;
  int x_{0};
  int y_{0};
};

static void edit_menu(const std::filesystem::path& menu_dir, const std::string& menu_set, const std::string& menu_name) {
  menus::Menu56 m(menu_dir, menu_set, menu_name);
  if (!m.initialized()) {
    m.set_initialized(true);
    m.menu.title = "New WWIV Menu";
    m.menu.color_item_braces = 9;
    m.menu.color_item_key = 2;
    m.menu.color_item_text = 1;
    m.menu.color_title = 5;
  }


  constexpr auto COL1_LINE = 2;
  constexpr auto LABEL_WIDTH = 14;
  constexpr auto COL2_LINE = COL1_LINE + LABEL_WIDTH + 1;

  EditItems items{};
  const string title = StrCat("Menu: ", menu_name);
  int y = 1;
  auto& h = m.menu;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Description:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 55, h.title, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Number Keys:"),
            new ToggleEditItem<menus::menu_numflag_t>(COL2_LINE, y, numbers_action, &h.num_action));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Logging Type:"),
            new ToggleEditItem<menus::menu_logtype_t>(COL2_LINE, y, logging_action, &h.logging_action));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Display Help:"),
            new ToggleEditItem<menus::menu_help_display_t>(COL2_LINE, y, help_action, &h.help_type));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Enter Actions:"),
            new ActionSubDialog(h.enter_actions, COL2_LINE, y, "[Edit Actions]"));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Exit Actions:"),
            new ActionSubDialog(h.exit_actions, COL2_LINE, y, "[Edit Actions]"));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "ACS:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 55, h.acs, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Password:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 20, h.password, EditLineMode::UPPER_ONLY));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Menu Items:"),
            new MenuItemsSubDialog(h.items, COL2_LINE, y, "[Edit Menu Items]"));

  items.Run(title);

  if (!m.Save()) {
    messagebox(curses_out->window(), "Error saving menu");
  }
}

static void select_menu(const std::string& menu_dir, const std::string& dir) {
  const auto full_dir_path = FilePath(menu_dir, dir);
  auto selected = -1;
  try {
    auto done{false};
    do {
      auto menus = FindFiles(FilePath(full_dir_path, "*"), FindFiles::FindFilesType::files, FindFiles::WinNameType::long_name);
      std::vector<ListBoxItem> items;
      for (const auto& m : menus) {
        auto lm = ToStringLowerCase(m.name);
        if (!ends_with(lm, ".mnu.json")) {
          continue;
        }
        const auto name = m.name.substr(0, m.name.size() - 9 ); 
        items.emplace_back(name);
      }
      auto* window = curses_out->window();
      auto title = StrCat("Select Menu from [", dir, "]");
      ListBox list(window, title, items);
      list.set_selected(selected);
      list.set_additional_hotkeys("DI");
      list.set_help_items({ { "Esc", "Exit" },{ "Enter", "Edit" },{ "D", "Delete" },{ "I", "Insert" } });
      auto result = list.Run();
      selected = list.selected();

      if (result.type == ListBoxResultType::SELECTION) {
        const auto& menu_name = items[result.selected].text();
        edit_menu(menu_dir, dir, menu_name);
      }
      else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      }
      else if (result.type == ListBoxResultType::HOTKEY) {
        if (result.hotkey == 'I') {
          auto menu_name = dialog_input_string(window, "Enter Menu Name: ", 8);
          if (!menu_name.empty()) {
            edit_menu(menu_dir, dir, menu_name);
          }
        } else if (result.hotkey == 'D') {
          const auto& menu_name = items[result.selected].text();
          if (dialog_yn(window, StrCat("Do you want to delete menu: ", menu_name))) {
            const auto fn = StrCat(menu_name, ".mnu.json");
            const auto ofn = StrCat(fn, ".deleted");
            File::Move(FilePath(full_dir_path, fn), 
                       FilePath(full_dir_path, ofn));
          }
        }
      }
    } while (!done);
  }
  catch (const std::exception& e) {
    LOG(ERROR) << e.what();
  }

}

void menus(const std::string& menu_dir) {
  try {

    bool done = false;
    int selected = -1;
    do {
      auto dirs = FindFiles(FilePath(menu_dir, "*"), FindFiles::FindFilesType::directories);
      std::vector<ListBoxItem> items;
      for (const auto& d : dirs) {
        if (starts_with(d.name, ".")) {
          continue;
        }
        items.emplace_back(d.name);
      }
      auto* window = curses_out->window();
      ListBox list(window, "Select Menu Set", items);
      list.set_selected(selected);
      list.selection_returns_hotkey(true);
      list.set_additional_hotkeys("I");
      list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"I", "Insert"} });
      const auto result = list.Run();
      selected = list.selected();

      if (result.type == ListBoxResultType::SELECTION) {
        const auto& sel_dir = items[result.selected].text();
        select_menu(menu_dir, sel_dir);
      } else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      } else if (result.type == ListBoxResultType::HOTKEY) {
        switch (result.hotkey) {
        case 'I':
          const auto sel_dir = dialog_input_string(window, "Enter Menu Set Name: ", 8);
          File::mkdirs(FilePath(menu_dir, sel_dir));
          break;
        }
      }
    } while (!done);
  } catch (const std::exception& e) {
    LOG(ERROR) << e.what();
  }
}

