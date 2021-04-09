/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
#include "bbs/menus/mainmenu.h"

#include "bbs/acs.h"
#include "bbs/bbs.h"
#include "bbs/instmsg.h"
#include "bbs/mmkey.h"
#include "bbs/sysoplog.h"
#include "bbs/menus/config_menus.h"
#include "bbs/menus/menucommands.h"
#include "common/printfile.h"
#include "common/menus/menu_generator.h"
#include "common/value/bbsvalueprovider.h"
#include "common/value/uservalueprovider.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/menus/menu_set.h"

#include <string>

using namespace wwiv::core;
using namespace wwiv::common::menus;
using namespace wwiv::strings;
using namespace wwiv::sdk::menus;

namespace wwiv::bbs::menus {

static bool CheckMenuPassword(const std::string& pw) {
  if (pw.empty()) {
    return true;
  }
  const auto expected_password = pw == "*SYSTEM" ? a()->config()->system_password() : pw;
  bout.nl();
  const auto actual_password = bin.input_password("|#2SY: ", 20);
  return actual_password == expected_password;
}

static void log_command(menu_logtype_t logging, const menu_item_56_t& mi) {
  switch (logging) {
  case menu_logtype_t::key:
    sysopchar(mi.item_key);
    break;
  case menu_logtype_t::command: {
    for (const auto& a : mi.actions) {
      sysoplog() << a.cmd;
    }
  } break;
  case menu_logtype_t::description: {
    if (!mi.item_text.empty()) {
      sysoplog() << "(" << mi.item_key << ") : " << mi.item_text;
    } else {
      for (const auto& a : mi.actions) {
        sysoplog() << a.cmd;
      }
    }
  } break;
  case menu_logtype_t::none:
    break;
  }
}

MainMenu::MainMenu(const sdk::Config& config) : config_(config) {}

void MainMenu::Run() {
  menu_set_name_ = a()->sess().current_menu_set();
  auto main = std::make_unique<Menu>(config_.menudir(), menu_set_name_, "main");
  while (!main->initalized()) {
    ConfigUserMenuSet("");
    // This should get updated when the user record gets written.
    menu_set_name_ = a()->sess().current_menu_set();
    // TODO(rushfan): Update menu_set_ and reload main
    main = std::make_unique<Menu>(config_.menudir(), menu_set_name_, "main");
  }
  if (!check_acs(main->menu().acs)) {
    sysoplog() << "Insufficient ACS for main menu";
    bout << "|#6Insufficient ACS for main menu! " << endl;
    LOG(ERROR) << "ACS check failed for main menu!";
    return;
  }

  // N.B. Main menu can not have a password.

  while (true) {
    if (stack_.empty()) {
      stack_.push_back(*main);
    }

    auto& cur_menu = stack_.back();
    auto [res, s] = cur_menu.Run();
    switch (res) {
    case menu_run_result_t::return_from_menu:
    case menu_run_result_t::error:
      stack_.pop_back();
      break;
    case menu_run_result_t::push_menu: {
      Menu m(config_.menudir(), menu_set_name_, s);
      if (!m.initalized()) {
        LOG(ERROR) << "Menu never loaded";
        continue;
      }
      stack_.push_back(m);
    } break;
    case menu_run_result_t::clear_stack:
      stack_.clear();
      break;
    case menu_run_result_t::change_menu_set:
      stack_.clear();
      menu_set_name_ = a()->sess().current_menu_set();
      main = std::make_unique<Menu>(config_.menudir(), menu_set_name_, "main");
      break;
    case menu_run_result_t::none:
      // Do nothing.
      break;
    }
  }
}

void mainmenu() {
  MainMenu m(*a()->config());
  m.Run();
}

Menu::Menu(const std::filesystem::path& menu_path, const wwiv::sdk::menus::MenuSet56& menu_set,
           const std::string& menu_name)
    : menu_set_name_(menu_set.menu_set.name), menu_name_(menu_name),
      menu_set_(menu_set),  menu_(menu_path, menu_set, menu_name) {

  menu_set_path_ = menu_set_.menuset_dir();
  prompt_ = "|09Command? ";

  // Load Prompt;
  const auto prompt_filename = StrCat(menu_name, ".pro");
  const auto prompt_path = FilePath(menu_set_path_, prompt_filename);

  TextFile prompt_file(prompt_path, "rb");
  if (prompt_file.IsOpen()) {
    const auto tmp = prompt_file.ReadFileIntoString();
    const auto end = tmp.find(".end.");
    if (end != std::string::npos) {
      prompt_ = tmp.substr(0, end);
    } else {
      prompt_ = tmp;
    }
  }
}

void Menu::DisplayMenu() {
  if (menu_displayed_) {
    VLOG(1) << "DisplayMenu: menu_displayed_ = true";
    return;
  }
  if (menu().cls) {
    bout.cls();
  }
  const auto path =
      common::CreateFullPathToPrint({menu_set_path_}, *a()->user(), menu_name_);
  if (!bout.printfile_path(path, true, false)) {
    GenerateMenu(menu_type_t::short_menu);
  }
  menu_displayed_ = true;
}

std::string Menu::GetCommandFromUser() const {
  if (!a()->user()->hotkeys()) {
    return bin.input_upper(50);
  }
  const auto nums = menu().num_action;
  if (nums == menu_numflag_t::dirs) {
    write_inst(INST_LOC_XFER, a()->current_user_dir().subnum, INST_FLAGS_NONE);
    return mmkey(MMKeyAreaType::dirs);
  }
  if (nums == menu_numflag_t::subs) {
    write_inst(INST_LOC_MAIN, a()->current_user_sub().subnum, INST_FLAGS_NONE);
    return mmkey(MMKeyAreaType::subs);
  }
  std::set<char> x = {'/'};
  return mmkey(x);
}

static bool IsNumber(const std::string& command) {
  if (command.empty()) {
    return false;
  }
  for (const auto& ch : command) {
    if (!isdigit(ch)) {
      return false;
    }
  }
  return true;
}

std::optional<menu_item_56_t> Menu::GetMenuItemForCommand(const std::string& cmd) const {
  const auto nums = menu().num_action;
  if (IsNumber(cmd) && nums != menu_numflag_t::none) {
    if (nums == menu_numflag_t::subs) {
      menu_item_56_t i{};
      menu_action_56_t a{"SetSubNumber", cmd, ""};
      i.actions.emplace_back(a);
      return {i};
    }
    if (nums == menu_numflag_t::dirs) {
      menu_item_56_t i{};
      menu_action_56_t a{"SetDirNumber", cmd, ""};
      i.actions.emplace_back(a);
      return {i};
    }
  }

  for (const auto& mi : menu().items) {
    if (cmd == mi.item_key) {
      return {mi};
    }
  }

  // Check global items.
  for (const auto& mi : menu_set_.menu_set.items) {
    if (cmd == mi.item_key) {
      return {mi};
    }
  }

  return std::nullopt;
}

std::tuple<menu_command_action_t, std::string> Menu::ExecuteAction(const menu_action_56_t& a) {
  if (auto o = interpret_command(this, a.cmd, a.data)) {
    auto& ctx = o.value();
    if (ctx.menu_action == menu_command_action_t::return_from_menu) {
      return std::make_tuple(menu_command_action_t::return_from_menu, "");
    }
    if (ctx.menu_action == menu_command_action_t::push_menu) {
      return std::make_tuple(menu_command_action_t::push_menu, a.data);
    }
  }
  return std::make_tuple(menu_command_action_t::none, "");
}

std::tuple<menu_command_action_t, std::string>
Menu::ExecuteActions(const std::vector<menu_action_56_t>& actions) {
  for (const auto& action : actions) {
    auto [a, d] = ExecuteAction(action);
    VLOG(1) << "Action: " << action.cmd << "; " << action.data << endl;
    if (a == menu_command_action_t::push_menu) {
      return std::make_tuple(menu_command_action_t::push_menu, d);
    }
    if (a == menu_command_action_t::return_from_menu) {
      return std::make_tuple(menu_command_action_t::return_from_menu, "");
    }
  }
  return std::make_tuple(menu_command_action_t::none, "");
}

std::tuple<menu_run_result_t, std::string> Menu::Run() {
  const auto menu_set = a()->user()->menu_set();
  if (!menu_.initialized()) {
    return std::make_tuple(menu_run_result_t::error, "");
  }
  if (!check_acs(menu().acs)) {
    sysoplog() << "Insufficient ACS for menu.";
    bout << "|#6Insufficient ACS for menu: " << menu().title << endl;
    return std::make_tuple(menu_run_result_t::error, "");
  }
  if (!CheckMenuPassword(menu().password)) {
    LOG(ERROR) << "Incorrect password";
    return std::make_tuple(menu_run_result_t::error, "");
  }

  {
    // Process entrance actions.
    auto [a, d] = ExecuteActions(menu().enter_actions);
    if (a == menu_command_action_t::push_menu) {
      return std::make_tuple(menu_run_result_t::push_menu, d);
    }
    if (a == menu_command_action_t::return_from_menu) {
      return std::make_tuple(menu_run_result_t::return_from_menu, "");
    }
  }

  auto first = true;
  // We have a good menu.
  for (;;) {
    if (menu().help_type == menu_help_display_t::never) {
      // Do nothing
    } else if (menu().help_type == menu_help_display_t::always) {
      DisplayMenu();
    } else if (first && menu().help_type == menu_help_display_t::on_entrance) {
      DisplayMenu();
      first = false;
    } else if (menu().help_type == menu_help_display_t::user_choice && !a()->user()->IsExpert()) {
      DisplayMenu();
    }
    const auto save_mci = bout.mci_enabled();
    bout.enable_mci();
    bout << prompt_;
    bout.set_mci_enabled(save_mci);
    // Do actions on enter.

    auto cmd = GetCommandFromUser();
    // Reset menu displayed
    menu_displayed_ = false;
    if (auto omi = GetMenuItemForCommand(cmd)) {
      const auto& mi = omi.value();
      if (!check_acs(mi.acs)) {
        sysoplog() << "Insufficient ACS for menu item: " << mi.item_key;
        bout << "|#6Insufficient ACS for menu item: " << mi.item_key << endl;
        continue;
      }
      VLOG(1) << "Command is: " << cmd << "; " << mi.item_key << endl;
      log_command(menu().logging_action, mi);
      auto [action, d] = ExecuteActions(mi.actions);
      if (action == menu_command_action_t::push_menu) {
        // No push or pop allowed in exit actions
        (void) ExecuteActions(menu().exit_actions);
        return std::make_tuple(menu_run_result_t::push_menu, d);
      }
      if (action == menu_command_action_t::return_from_menu) {
        // No push or pop allowed in exit actions
        (void) ExecuteActions(menu().exit_actions);
        return std::make_tuple(menu_run_result_t::return_from_menu, "");
      }
      if (reload || !iequals(menu_set, a()->user()->menu_set())) {
        // We changed menu sets.  Reload is set by Defaults when changing the
        // menu set.
        return std::make_tuple(menu_run_result_t::change_menu_set, "");
      }
    }
  }
}


void Menu::GenerateMenu(menu_type_t typ) {
  if (menu_displayed_) {
    VLOG(1) << "GenerateMenu: menu_displayed_: true";
    return;
  }
  const auto lines = GenerateMenuAsLines(typ);
  for (const auto& l : lines) {
    if (bin.checka()) {
      break;
    }
    bout << l << endl;    
  }
  menu_displayed_ = true;
}


std::vector<std::string> Menu::GenerateMenuAsLines(menu_type_t typ) {
  const common::value::UserValueProvider up(a()->context());
  const common::value::BbsValueProvider bp(*a()->config(), a()->sess());
  const std::vector<const wwiv::sdk::value::ValueProvider*> providers{&up, &bp};
  return GenerateMenuLines(*a()->config(), menu_set_, menu(), *a()->user(),
                           providers, typ);
}


} // namespace wwiv::bbs::menus
