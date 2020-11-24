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

class ActionPickerSubDialog final : public SubDialog<std::string> {
public:
  ActionPickerSubDialog(const Config& config, const std::vector<menus::menu_command_help_t>& cmds, int x, int y,
                      int width, std::string& c)
      : SubDialog(config, x, y, c), cmds_(cmds), width_(width) {}
  ~ActionPickerSubDialog() override = default;

  void RunSubDialog(CursesWindow* window) override {
    do {
      std::vector<ListBoxItem> items;
      for (const auto& e : cmds_) {
        const auto cat = StrCat("[", e.cat, "]");
        auto help = e.help;
        help.erase(std::remove(help.begin(), help.end(), 10), help.end());
        help.erase(std::remove(help.begin(), help.end(), 13), help.end());
        items.emplace_back(fmt::format("{:17.17} {:13.13} {:48.48}", e.cmd, cat, help));
      }
      ListBox list(window, "Select Command", items);

      list.selection_returns_hotkey(true);
      list.set_help_items({{"Esc", "Exit"}, {"Enter", "Select"}});
      const auto result = list.Run();
      if (result.type == ListBoxResultType::SELECTION) {
        const auto selected_cmd = cmds_[result.selected].cmd;
        t_ = selected_cmd;
        return;
      }
      if (result.type == ListBoxResultType::NO_SELECTION) {
        return;
      }
    } while (true);
  }

  [[nodiscard]] std::string menu_label() const override {
    return fmt::format("{:<{}}", t_, width_);
  }

private:
  const std::vector<menus::menu_command_help_t> cmds_;
  int width_;
};

static void edit_menu_action(const Config& config, menus::menu_action_56_t& a) {
  constexpr auto LABEL_WIDTH = 6;
  constexpr auto PADDING = 2;
  constexpr auto COL1_LINE = PADDING;
  constexpr auto COL2_LINE = COL1_LINE + LABEL_WIDTH;

  static const auto cmds = menus::LoadCommandHelpJSON(config.datadir());

  EditItems items{};
  auto y = 1;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Cmd:"),
            new ActionPickerSubDialog(config, cmds, COL2_LINE, y, 40, a.cmd));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Data:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 70, a.data, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "ACS:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 70, a.acs, EditLineMode::ALL));

  
  items.Run("Edit Action");
}


class ActionSubDialog final : public SubDialog<std::vector<menus::menu_action_56_t>> {
public:
  ActionSubDialog(const Config& config, int x, int y, std::vector<menus::menu_action_56_t>& actions)
    : SubDialog(config, x, y, actions) {}
  ~ActionSubDialog() override = default;

  void RunSubDialog(CursesWindow* window) override {
    auto done = false;
    auto selected = -1;
    do {
      std::vector<ListBoxItem> items;
      for (auto i = 0; i < wwiv::stl::ssize(t_); i++) {
        const auto& m = t_.at(i);
        auto s = fmt::format("{}:{}", m.cmd, m.data);
        items.emplace_back(s, 0, i);
      }
      ListBox list(window, "[Edit Actions]", items);

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
        edit_menu_action(config(), t_[items[result.selected].data()]);
      }
      else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      }
      else if (result.type == ListBoxResultType::HOTKEY) {
        const auto key = static_cast<char>(result.hotkey);
        const auto num = result.selected > 0 ? items[result.selected].data() : 0;
        switch (key) {
        case 'D': {
          if (t_.empty()) {
            break;
          }
          const auto& item = t_[num];
          if (dialog_yn(window, StrCat("Do you want to delete #", num, " [", item.cmd, "]"))) {
            erase_at(t_, num);
          }
        } break;
        case 'I': {
          if (num <= 0 || num >= ssize(items)) {
            t_.push_back({});
          }
          else {
            menus::menu_action_56_t mr{};
            insert_at(t_, num, mr);
          }
        } break;
        case 'M': {
          auto maxnum = size_int(t_) + 1;
          auto prompt = fmt::format("Move to before which (1-{}) : ", maxnum);
          auto new_pos = dialog_input_number(window, prompt, 1, maxnum);
          if (new_pos >= 1) {
            auto saved = t_.at(num);
            if (erase_at(t_, num)) {
              insert_at(t_, new_pos, saved);
            }
          }
        } break;
        }
      }
    } while (!done);
  }

  [[nodiscard]] std::string menu_label() const override {
    return fmt::format("[Edit] {} actions.", t_.size());
  }

};

static void edit_menu_item(const Config& config, menus::menu_item_56_t& m) {
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
            new ActionSubDialog(config, COL2_LINE, y, m.actions));


  items.Run(StrCat("Menu: ", m.item_key));
}

class MenuItemsSubDialog : public SubDialog<std::vector<menus::menu_item_56_t>> {
public:
  MenuItemsSubDialog(const wwiv::sdk::Config& config,
    int x, int y, std::vector<menus::menu_item_56_t>& menu_items)
      : SubDialog(config, x, y, menu_items) {}
  ~MenuItemsSubDialog() override = default;

  void RunSubDialog(CursesWindow* window) override {
    try {
      auto done = false;
      auto selected = -1;
      do {
        std::vector<ListBoxItem> items;
        for (auto i = 1; i < wwiv::stl::ssize(t_); i++) {
          const auto& m = t_.at(i);
          auto key = fmt::format("({})", m.item_key);
          auto s = fmt::format("#{} {:<12} '{}'", i, key, m.item_text);
          if (!m.actions.empty()) {
            s += fmt::format(" [{}]", m.actions.front().cmd);
          }
          items.emplace_back(s, 0, i);
        }
        ListBox list(curses_out->window(), "[Edit Menu Items]", items);

        list.set_additional_hotkeys("DIM");
        list.set_help_items({ 
          { "Esc", "Exit" },{ "Enter", "Edit" },
          { "D", "Delete" },{ "I", "Insert" },
          { "M", "Move" }
        });
        list.set_selected(selected);
        auto result = list.Run();
        window->RedrawWin();
        selected = list.selected();

        if (result.type == ListBoxResultType::SELECTION) {
          edit_menu_item(config(), t_[items[result.selected].data()]);
        }
        else if (result.type == ListBoxResultType::NO_SELECTION) {
          done = true;
        }
        else if (result.type == ListBoxResultType::HOTKEY) {
          const auto key = static_cast<char>(result.hotkey);
          const auto num = result.selected > 0 ? items[result.selected].data() : 0;
          switch (key) {
          case 'D': {
            const auto& item = t_[items[result.selected].data()];
            if (dialog_yn(window, StrCat("Do you want to delete #", num, "[", item.item_key, "]"))) {
              erase_at(t_, num);
            }
          } break;
          case 'I': {
            if (num <= 0 || num >= ssize(items)) {
              t_.push_back({});
            }
            else {
              menus::menu_item_56_t mr{};
              insert_at(t_, num, mr);
            }
          } break;
          case 'M': {
            auto maxnum = size_int(t_) + 1;
            auto prompt = fmt::format("Move to before which (1-{}) : ", maxnum);
            auto new_pos = dialog_input_number(window, prompt, 1, maxnum);
            if (new_pos >= 1) {
              auto &saved = t_.at(num);
              if (erase_at(t_, num)) {
                insert_at(t_, new_pos, saved);
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
  }

  void Display(CursesWindow* window) const override {
    window->PutsXY(x_, y_, "[Enter to Edit]");
  }

};

static void edit_menu(const Config& config, const std::filesystem::path& menu_dir, const std::string& menu_set, const std::string& menu_name) {
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
            new ActionSubDialog(config, COL2_LINE, y, h.enter_actions));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Exit Actions:"),
            new ActionSubDialog(config, COL2_LINE, y, h.enter_actions));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "ACS:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 55, h.acs, EditLineMode::ALL));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Password:"),
            new StringEditItem<std::string&>(COL2_LINE, y, 20, h.password, EditLineMode::UPPER_ONLY));
  y++;
  items.add(new Label(COL1_LINE, y, LABEL_WIDTH, "Menu Items:"),
            new MenuItemsSubDialog(config, COL2_LINE, y, h.items));

  items.Run(title);

  if (!m.Save()) {
    messagebox(curses_out->window(), "Error saving menu");
  }
}

static void select_menu(const wwiv::sdk::Config& config, const std::string& menu_dir, const std::string& dir) {
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
      list.set_additional_hotkeys("CDI");
      list.set_help_items({ { "Esc", "Exit" },{ "Enter", "Edit" },{ "D", "Delete" },{ "I", "Insert" },{ "C", "Copy" } });
      auto result = list.Run();
      selected = list.selected();

      if (result.type == ListBoxResultType::SELECTION) {
        const auto& menu_name = items[result.selected].text();
        edit_menu(config, menu_dir, dir, menu_name);
      }
      else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      }
      else if (result.type == ListBoxResultType::HOTKEY) {
        if (result.hotkey == 'I') {
          auto menu_name = dialog_input_string(window, "Enter Menu Name: ", 8);
          if (!menu_name.empty()) {
            edit_menu(config, menu_dir, dir, menu_name);
          }
        } else  if (result.hotkey == 'C') {
          const auto& old_menu_name = items[result.selected].text();
          auto menu_name = dialog_input_string(window, "Enter New Menu Name: ", 8);
          if (!menu_name.empty()) {
            const auto old_name = FilePath(full_dir_path, StrCat(old_menu_name, ".mnu.json"));
            const auto new_name = FilePath(full_dir_path, StrCat(menu_name, ".mnu.json"));
            File::Copy(old_name, new_name);
            edit_menu(config, menu_dir, dir, menu_name);
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

void menus(const wwiv::sdk::Config& config) {
  try {
    const auto menu_dir = config.menudir();
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
        select_menu(config, menu_dir, sel_dir);
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

