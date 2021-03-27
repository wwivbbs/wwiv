/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2017-2021, WWIV Software Services           */
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

#include "common/menus/menu_generator.h"
#include "common/value/bbsvalueprovider.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "localui/curses_win.h"
#include "localui/edit_items.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "sdk/usermanager.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/menus/menu.h"
#include "common/value/uservalueprovider.h"
#include "local_io/null_local_io.h"
#include "sdk/value/valueprovider.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

using namespace wwiv::common::value;
using namespace wwiv::core;
using namespace wwiv::local::ui;
using namespace wwiv::sdk;
using namespace wwiv::sdk::fido;
using namespace wwiv::stl;
using namespace wwiv::strings;

class ActionPickerSubDialog final : public SubDialog<std::string> {
public:
  ActionPickerSubDialog(const Config& config, std::vector<menus::menu_command_help_t> cmds,
                        int width, std::string& c)
      : SubDialog(config, c), cmds_(std::move(cmds)) {
    this->width_ = width;
  }
  ~ActionPickerSubDialog() override = default;

  void RunSubDialog(CursesWindow* window) override {
    do {
      std::vector<ListBoxItem> items;
      auto selected = -1;
      auto count = 0;
      for (const auto& e : cmds_) {
        const auto cat = StrCat("[", e.cat, "]");
        auto help = e.help;
        help.erase(std::remove(help.begin(), help.end(), 10), help.end());
        help.erase(std::remove(help.begin(), help.end(), 13), help.end());
        items.emplace_back(fmt::format("{:20.20} {:13.13} {:44.44}", e.cmd, cat, help), 0, count);
        if (iequals(e.cmd, t_)) {
          selected = count;
        }
        ++count;
      }
      ListBox list(window, "Select Command", items);
      list.set_selected(selected);
      list.selection_returns_hotkey(true);
      list.set_help_items({{"Esc", "Exit"}, {"Enter", "Select"}, {"/", "Filter"}});
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
    return fmt::format("[Edit] '{:<.{}}'", t_, width());
  }

private:
  const std::vector<menus::menu_command_help_t> cmds_;
};

static void edit_menu_action(const Config& config, menus::menu_action_56_t& a,
                             std::vector<const value::ValueProvider*> providers) {
  const auto cmds = menus::LoadCommandHelpJSON(config.datadir());

  EditItems items{};
  auto y = 1;
  items.add(new Label("Cmd:"), new ActionPickerSubDialog(config, cmds, 40, a.cmd),
            "Command to execute", 1, y);
  y++;
  items.add(new Label("Data:"), new StringEditItem<std::string&>(70, a.data, EditLineMode::ALL),
            "Optional data to pass to the command", 1, y);
  y++;
  items.add(new Label("ACS:"), new ACSEditItem(config,  providers, 70, a.acs),
            "WWIV ACS required to execute this command", 1, y);

  items.relayout_items_and_labels();
  items.Run("Edit Action");
}

class ActionSubDialog final : public SubDialog<std::vector<menus::menu_action_56_t>> {
public:
  ActionSubDialog(const Config& config, std::vector<menus::menu_action_56_t>& actions,
                  std::vector<const value::ValueProvider*> p)
      : SubDialog(config, actions), providers_(std::move(p)) {}
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
      list.set_help_items(
          {{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"}, {"M", "Move"}});
      list.set_selected(selected);
      const auto result = list.Run();
      selected = list.selected();

      if (result.type == ListBoxResultType::SELECTION) {
        edit_menu_action(config(), t_[items[result.selected].data()], providers_);
      } else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      } else if (result.type == ListBoxResultType::HOTKEY) {
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
          } else {
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
  std::vector<const value::ValueProvider*> providers_;
};

static void edit_menu_item(const Config& config, menus::menu_item_56_t& m, std::vector<const value::ValueProvider*> providers) {
  EditItems items{};
  auto y = 1;
  items.add(new Label("Menu Key:"),
            new StringEditItem<std::string&>(10, m.item_key, EditLineMode::UPPER_ONLY),
            "Key(s) to execute command", 1, y);
  y++;
  items.add(new Label("Menu Text:"),
            new StringEditItemWithPipeCodes(60, m.item_text, EditLineMode::ALL),
            "What to show on generated menu", 1, y);
  y++;
  items.add(new Label("Actions:"), new ActionSubDialog(config, m.actions, providers), 
    "The actions to execute when this menu is selected", 1, y);
  y++;
  items.add(new Label("Help Text:"),
            new StringEditItemWithPipeCodes(60, m.help_text, EditLineMode::ALL),
            "One line help for command", 1, y);
  y++;
  items.add(new Label("Sysop Log:"),
            new StringEditItem<std::string&>(50, m.log_text, EditLineMode::ALL),
            "What to show in sysop log", 1, y);
  y++;
  items.add(new Label("Instance:"),
            new StringEditItem<std::string&>(60, m.instance_message, EditLineMode::ALL),
            "Instance message to send to all users", 1, y);
  y++;
  items.add(new Label("ACS:"), new ACSEditItem(config, providers, 60, m.acs),
            "WWIV ACS required to access this menu item", 1, y);
  y++;
  items.add(new Label("Password:"),
            new StringEditItem<std::string&>(20, m.password, EditLineMode::UPPER_ONLY),
            "Password required to access this menu item", 1, y);
  y++;
  items.add(new Label("Visible:"),
            new BooleanEditItem(&m.visible),
            "Is this item included in the generated menus.", 1, y);

  items.relayout_items_and_labels();
  items.Run(StrCat("Menu: ", m.item_key));
}

class MenuItemsSubDialog : public SubDialog<std::vector<menus::menu_item_56_t>> {
public:
  MenuItemsSubDialog(const Config& config, std::vector<menus::menu_item_56_t>& menu_items,
    std::vector<const value::ValueProvider*> providers)
      : SubDialog(config, menu_items), providers_(providers) {}
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
        list.set_help_items(
            {{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"}, {"M", "Move"}});
        list.set_selected(selected);
        auto result = list.Run();
        window->RedrawWin();
        selected = list.selected();

        if (result.type == ListBoxResultType::SELECTION) {
          edit_menu_item(config(), t_[items[result.selected].data()], providers_);
        } else if (result.type == ListBoxResultType::NO_SELECTION) {
          done = true;
        } else if (result.type == ListBoxResultType::HOTKEY) {
          const auto key = static_cast<char>(result.hotkey);
          const auto num = result.selected > 0 ? items[result.selected].data() : 0;
          switch (key) {
          case 'D': {
            const auto& item = t_[items[result.selected].data()];
            if (dialog_yn(window,
                          StrCat("Do you want to delete #", num, "[", item.item_key, "]"))) {
              erase_at(t_, num);
            }
          } break;
          case 'I': {
            if (num <= 0 || num >= ssize(items)) {
              t_.push_back({});
            } else {
              menus::menu_item_56_t mr{};
              insert_at(t_, num, mr);
            }
          } break;
          case 'M': {
            auto maxnum = size_int(t_) + 1;
            auto prompt = fmt::format("Move to before which (1-{}) : ", maxnum);
            auto new_pos = dialog_input_number(window, prompt, 1, maxnum);
            if (new_pos >= 1) {
              auto& saved = t_.at(num);
              if (erase_at(t_, num)) {
                insert_at(t_, new_pos, saved);
              }
            }
          } break;
          }
        }
      } while (!done);
    } catch (const std::exception& e) {
      LOG(ERROR) << e.what();
    }
  }

  [[nodiscard]] std::string menu_label() const override { return "[Edit]"; }
  std::vector<const value::ValueProvider*> providers_;
};


// Base item of an editable value, this class does not use templates.
class GeneratedMenuSubDialog : public SubDialog<menus::generated_menu_56_t> {
public:
  GeneratedMenuSubDialog() = delete;
  GeneratedMenuSubDialog(const GeneratedMenuSubDialog&) = delete;
  GeneratedMenuSubDialog(GeneratedMenuSubDialog&&) = delete;
  GeneratedMenuSubDialog& operator=(const GeneratedMenuSubDialog&) = delete;
  GeneratedMenuSubDialog& operator=(GeneratedMenuSubDialog&&) = delete;
  GeneratedMenuSubDialog(const Config& config, menus::generated_menu_56_t& d)
      : SubDialog(config, d) {}
  ~GeneratedMenuSubDialog() override = default;

  void RunSubDialog(CursesWindow* window) override {
    EditItems items{};
    constexpr int MAX_STRING_LEN = 10;
    auto y = 1;

    items.add(new Label("Num Columns:"),
              new NumberEditItem<int, 1>(&t_.num_cols),
              "How many columns should the generated menu contain", 1, y);
    ++y;

    items.add(new Label("Title Color:"),
              new StringEditItem<std::string&>(MAX_STRING_LEN, t_.color_title, EditLineMode::ALL),
              "This system's FTN address for use on the network.", 1, y);
    ++y;

    items.add(new Label("Brace Color:"),
              new StringEditItem<std::string&>(MAX_STRING_LEN, t_.color_item_braces, EditLineMode::ALL),
              "Pipe code to use to color the braces surrounding the key in generated menus.", 1, y);
    ++y;

    items.add(new Label("Key Color:"),
              new StringEditItem<std::string&>(MAX_STRING_LEN, t_.color_item_key, EditLineMode::ALL),
              "Pipe code to use to color each menu item key in generated menus.", 1, y);
    ++y;

    items.add(new Label("Text Color:"),
              new StringEditItem<std::string&>(MAX_STRING_LEN, t_.color_item_text, EditLineMode::ALL),
              "Pipe code to use to color the text description in generated menus", 1, y);
    ++y;

    items.add(new Label("Show Empty:"),
              new BooleanEditItem(&t_.show_empty_text),
              "Display menu items that have no text.", 1, y);
    ++y;

    items.add(new Label("Num Newlines:"),
              new NumberEditItem<int, 2>(&t_.num_newlines_at_end),
              "Number of blank lines to add between generated menu and prompt.", 1, y);
    ++y;

    window->GotoXY(x_, y_);

    items.relayout_items_and_labels();
    items.Run("Generated Menu Settings");
    window->RedrawWin();
  }
protected:
  [[nodiscard]] std::string menu_label() const override { return "[Edit]"; }
};

/**
 * EditItem that executes a std::function<T, CursesWindow*> to
 * edit the items. It is intended that this function will invoke
 * a new EditItem dialog or ListBox for editing.
 */
class ShowGeneratedMenuSubDialog : public BaseEditItem {
public:
  ShowGeneratedMenuSubDialog(const Config& config, std::vector<const value::ValueProvider*> p,
                             const menus::menu_56_t& menu)
      : BaseEditItem(10), config_(config), providers_(p), menu_(menu) {}
  ~ShowGeneratedMenuSubDialog() override = default;
  ShowGeneratedMenuSubDialog() = delete;
  ShowGeneratedMenuSubDialog(ShowGeneratedMenuSubDialog const&) = delete;
  ShowGeneratedMenuSubDialog(ShowGeneratedMenuSubDialog&&) = delete;
  ShowGeneratedMenuSubDialog& operator=(ShowGeneratedMenuSubDialog const&) = delete;
  ShowGeneratedMenuSubDialog& operator=(ShowGeneratedMenuSubDialog&&) = delete;

  EditlineResult Run(CursesWindow* window) override {
    ScopeExit at_exit([] { curses_out->footer()->SetDefaultFooter(); });
    curses_out->footer()->ShowHelpItems(
        0, {});
    curses_out->footer()->ShowContextHelp("Viewing Menu: Press any key to continue.");
    window->GotoXY(x_, y_);
    uint32_t old_attr;
    short old_pair;
    window->AttrGet(&old_attr, &old_pair);
    window->SetColor(SchemeId::EDITLINE);
    window->PutsXY(x_, y_, menu_label());
    window->GotoXY(x_, y_);
    const auto ch = window->GetChar();
    window->AttrSet(COLOR_PAIR(old_pair) | old_attr);
    if (ch == KEY_ENTER || ch == wwiv::local::io::TAB || ch == wwiv::local::io::ENTER) {
      curses_out->Cls();
      User user{};
      user.screen_width(80);
      user.screen_lines(24);
      user.sl(255);
      user.dsl(255);
      const auto lines = wwiv::common::menus::GenerateMenuLines(
          config_, menu_, user, providers_, menus::menu_type_t::short_menu);
      auto y = 1;
      for (const auto& l : lines) {
        curses_out->window()->PutsWithPipeColors(0, y++, l);
      }
      (void) curses_out->window()->GetChar();
      curses_out->Cls(ACS_CKBOARD);
      // This clears up everything anyway.
      curses_out->ReenableLocalIO();
      window->RedrawWin();
    } else if (ch == KEY_UP || ch == KEY_BTAB) {
      return EditlineResult::PREV;
    } else if (ch == wwiv::local::io::ESC) {
      return EditlineResult::DONE;
    }
    return EditlineResult::NEXT;
  }

  void Display(CursesWindow* window) const override {
    window->SetColor(SchemeId::WINDOW_DATA);
    window->PutsXY(x_, y_, menu_label());
  }

protected:
  [[nodiscard]] virtual std::string menu_label() const { return "[View]"; }

  const Config& config_;
  const std::vector<const value::ValueProvider*> providers_;
  const menus::menu_56_t& menu_;
};


static void edit_menu(const Config& config, const std::filesystem::path& menu_dir,
                      const std::string& menu_set, const std::string& menu_name) {
  menus::Menu56 m(menu_dir, menu_set, menu_name);
  const auto menu_path = FilePath(menu_dir, menu_set);

  if (!m.initialized()) {
    m.set_initialized(true);
    m.menu.title = "New WWIV Menu";
  }

  const std::vector<std::pair<wwiv::sdk::menus::menu_numflag_t, std::string>> numbers_action = {
      {menus::menu_numflag_t::none, "Nothing"},
      {menus::menu_numflag_t::subs, "Set Sub Number"},
      {menus::menu_numflag_t::dirs, "Set Dir Number"}};

  const std::vector<std::pair<menus::menu_logtype_t, std::string>> logging_action = {
      {menus::menu_logtype_t::none, "Nothing"},
      {menus::menu_logtype_t::command, "Command"},
      {menus::menu_logtype_t::key, "Key"},
      {menus::menu_logtype_t::description, "Description"}};

  const std::vector<std::pair<menus::menu_help_display_t, std::string>> help_action = {
      {menus::menu_help_display_t::never, "Never"},
      {menus::menu_help_display_t::always, "Always"},
      {menus::menu_help_display_t::on_entrance, "On Entrance"},
      {menus::menu_help_display_t::user_choice, "User Choice"}};

  EditItems items{};
  const auto title = StrCat("Menu: ", menu_name);
  auto y = 1;
  auto& h = m.menu;

  const UserManager um(config);
  User user{};
  if (!um.readuser(&user, 1)) {
    user.set_name("SYSOP");
    user.sl(255);
    user.dsl(255);
  }
  UserValueProvider up(config, user, 255, config.sl(255));

  wwiv::local::io::NullLocalIO null_io;
  const wwiv::common::SessionContext sess(&null_io);
  BbsValueProvider bbs_provider(config, sess);

  std::vector<const value::ValueProvider*> providers{&up, &bbs_provider};

  items.add(new Label("Title:"),
            new StringEditItemWithPipeCodes(55, h.title, EditLineMode::ALL),
            "This is the title to display at the top of the menu", 1, y);
  y++;
  items.add(new Label("ACS:"), new ACSEditItem(config, providers, 55, h.acs),
            "WWIV ACS required to access this menu", 1, y);
  y++;
  items.add(new Label("Number Keys:"),
            new ToggleEditItem<menus::menu_numflag_t>(numbers_action, &h.num_action),
            "Select what number keys do in this menu", 1, y);
  y++;
  items.add(new Label("Logging Type:"),
            new ToggleEditItem<menus::menu_logtype_t>(logging_action, &h.logging_action),
            "Select what will appear in the sysop log when the user executes the commands", 1, y);
  y++;
  items.add(new Label("Display Help:"),
            new ToggleEditItem<menus::menu_help_display_t>(help_action, &h.help_type),
            "When is the help screen (menu MSG/ANS/or generated help) displayed", 1, y);
  y++;
  items.add(new Label("Enter Actions:"), new ActionSubDialog(config, h.enter_actions, providers),
            "Menu actions to execute when entering this menu", 1, y);
  y++;
  items.add(new Label("Exit Actions:"), new ActionSubDialog(config, h.exit_actions, providers),
            "Menu actions to execute when leaving this menu", 1, y);
  y++;
  items.add(new Label("Password:"),
            new StringEditItem<std::string&>(20, h.password, EditLineMode::UPPER_ONLY),
            "Password to access menu. You may use '*SYSTEM' for system password.", 1, y);
  y++;
  items.add(new Label("Clear Screen:"),
            new BooleanEditItem(&h.cls),
            "Clear the screen before displaying menu.", 1, y);
  y++;
  items.add(new Label("Menu Settings:"), new GeneratedMenuSubDialog(config, h.generated_menu), 
    "Edits the settings for a generated menu (not using .msg/.ans file)", 1, y);
  y++;
  const auto p = FilePath(menu_path, StrCat(menu_name, ".pro"));
  items.add(new Label("Edit Prompt:"), new EditExternalFileItem(p), 1, y);
  y++;
  items.add(new Label("Menu Items:"), new MenuItemsSubDialog(config, h.items, providers), "", 1, y);
  y++;
  items.add(new Label("View Menu:"), new ShowGeneratedMenuSubDialog(config, providers, h), 
    "Display the generated menu for the sysop user.", 1, y);
  y++;

  items.relayout_items_and_labels();
  items.Run(title);

  if (!m.Save()) {
    messagebox(curses_out->window(), "Error saving menu");
  }
}

static void select_menu(const wwiv::sdk::Config& config, const std::string& menu_dir,
                        const std::string& dir) {
  const auto full_dir_path = FilePath(menu_dir, dir);
  auto selected = -1;
  try {
    auto done{false};
    do {
      auto menus = FindFiles(FilePath(full_dir_path, "*"), FindFiles::FindFilesType::files,
                             FindFiles::WinNameType::long_name);
      std::vector<ListBoxItem> items;
      for (const auto& m : menus) {
        auto lm = ToStringLowerCase(m.name);
        if (!ends_with(lm, ".mnu.json")) {
          continue;
        }
        const auto name = m.name.substr(0, m.name.size() - 9);
        items.emplace_back(name);
      }
      auto* window = curses_out->window();
      auto title = StrCat("Select Menu from [", dir, "]");
      ListBox list(window, title, items);
      list.set_selected(selected);
      list.set_additional_hotkeys("CDI");
      list.set_help_items(
          {{"Esc", "Exit"}, {"Enter", "Edit"}, {"D", "Delete"}, {"I", "Insert"}, {"C", "Copy"}});
      auto result = list.Run();
      selected = list.selected();

      if (result.type == ListBoxResultType::SELECTION) {
        const auto& menu_name = items[result.selected].text();
        edit_menu(config, menu_dir, dir, menu_name);
      } else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      } else if (result.type == ListBoxResultType::HOTKEY) {
        if (result.hotkey == 'I') {
          auto menu_name = dialog_input_string(window, "Enter Menu Name: ", 8);
          if (!menu_name.empty()) {
            edit_menu(config, menu_dir, dir, menu_name);
          }
        } else if (result.hotkey == 'C') {
          const auto& old_menu_name = items[result.selected].text();
          if (auto menu_name = dialog_input_string(window, "Enter New Menu Name: ", 8); !menu_name.empty()) {
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
            File::Move(FilePath(full_dir_path, fn), FilePath(full_dir_path, ofn));
          }
        }
      }
    } while (!done);
  } catch (const std::exception& e) {
    LOG(ERROR) << e.what();
  }
}

static bool check_for_menu_help(const std::string& datadir) {
  const auto path = FilePath(datadir, "menu_commands.json");
  if (!File::Exists(path)) {
    auto* window = curses_out->window();
    const std::vector<std::string> msg{
        "menu_commands.json does not exist in the WWIV/datadir.", "",
        "Please either run 'bbs -oj' to create it, or copy it from data.zip",
        "in the WWIV installation."};
    messagebox(window, msg);
    return false;
  }
  return true;
}

void menus(const Config& config) {
  try {
    const auto menu_dir = config.menudir();
    if (!check_for_menu_help(config.datadir())) {
      return;
    }
    auto done = false;
    auto selected = -1;
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
      list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}, {"/", "Filter"}, {"I", "Insert"}});
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

