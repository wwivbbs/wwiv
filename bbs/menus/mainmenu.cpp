/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
#include "bbs/menus/mainmenu.h"


#include "bbs/menus/menucommands.h"
#include "bbs/acs.h"
#include "bbs/bbs.h"
#include "bbs/instmsg.h"
#include "bbs/mmkey.h"
#include "bbs/oldmenu.h"
#include "common/printfile.h"
#include "sdk/config.h"

namespace wwiv::bbs::menus {

using namespace wwiv::core;

MainMenu::MainMenu(const sdk::Config& config) : config_(config) {}

void MainMenu::Run() {
  menu_set_ = a()->user()->menu_set();
  auto main = std::make_unique<MenuInstance>(config_.menudir(), menu_set_, "main");
  while (!main->initalized()) {
    wwiv::menus::ConfigUserMenuSet();
    // TODO(rushfan): Update menu_set_ and reload main
    main = std::make_unique<MenuInstance>(config_.menudir(), menu_set_, "main");
  }
  if (!check_acs(main->menu().acs)) {
    LOG(ERROR) << "ACS check failed for main menu!";
    return;
  }

  while (true) {
    if (stack_.empty()) {
      stack_.push_back(*main);
    }

    auto& cur_menu = stack_.back();
    auto[res, s] = cur_menu.Run();
    switch (res) {
    case menu_run_result_t::return_from_menu:
    case menu_run_result_t::error:
      stack_.pop_back();
      break;
    case menu_run_result_t::push_menu: {
      MenuInstance m(config_.menudir(), menu_set_, s);
      if (!m.initalized()) {
        LOG(ERROR) << "Menu never loaded";
        continue;
      }
      if (!check_acs(m.menu().acs)) {
        LOG(ERROR) << "Insufficient ACS";
        continue;
      }
      stack_.push_back(m);
    } break;
    case menu_run_result_t::clear_stack:
      stack_.clear();
      break;
    case menu_run_result_t::change_menu_set:
      stack_.clear();
      return;
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

MenuInstance::MenuInstance(const std::filesystem::path& menu_path, const std::string& menu_set,
                           const std::string& menu_name)
    : menu_set_(menu_set), menu_name_(menu_name), menu_(menu_path, menu_set, menu_name) {

  menu_set_path_ = FilePath(menu_path, menu_set_);
  prompt_ = "|09Command? ";

  // Load Prompt;
  const auto prompt_filename = wwiv::strings::StrCat(menu_name, ".pro");
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

void MenuInstance::DisplayMenu() const {
  const auto path = common::CreateFullPathToPrint({menu_set_path_.string()}, *a()->user(), menu_name_);
  if (!bout.printfile_path(path, true, false)) {
    GenerateMenu();
  }
}

std::string MenuInstance::GetCommandFromUser() const {
  if (!a()->user()->hotkeys()) {
    return bin.input(50);
  }
  const auto nums = menu().num_action;
  if (nums == MENU_NUMFLAG_DIRNUMBER) {
    write_inst(INST_LOC_XFER, a()->current_user_dir().subnum, INST_FLAGS_NONE);
    return mmkey(MMKeyAreaType::dirs);
  }
  if (nums == MENU_NUMFLAG_SUBNUMBER) {
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

std::optional<sdk::menus::menu_item_56_t>
MenuInstance::GetMenuItemForCommand(const std::string& cmd) const {
  const auto nums = menu().num_action;
  if (IsNumber(cmd) && nums != MENU_NUMFLAG_NOTHING) {
    if (nums == MENU_NUMFLAG_SUBNUMBER) {
      sdk::menus::menu_item_56_t i{};
      sdk::menus::menu_action_56_t a{"SetSubNumber", cmd, ""};
      i.actions.emplace_back(a);
      return {i};
    }
    if (nums == MENU_NUMFLAG_DIRNUMBER) {
      sdk::menus::menu_item_56_t i{};
      sdk::menus::menu_action_56_t a{"SetDirNumber", cmd, ""};
      i.actions.emplace_back(a);
      return {i};
    }
    DLOG(FATAL) << "Bad value for nums: " << nums << "; in menu: " << menu_name_;
  }

  for (const auto& mi : menu().items) {
    if (cmd == mi.item_key) {
      return {mi};
    }
  }
  return std::nullopt;
}


std::tuple<menu_command_action_t, std::string> MenuInstance::ExecuteAction(const sdk::menus::menu_action_56_t& a) {
  if (auto o = InterpretCommand(this, a.cmd, a.data)) {
    auto& ctx = o.value();
    if (ctx.menu_action == menu_command_action_t::return_from_menu) {
      return std::make_tuple(menu_command_action_t::return_from_menu,"");
    }
    if (ctx.menu_action == menu_command_action_t::push_menu) {
      return std::make_tuple(menu_command_action_t::push_menu, a.data);
    }
  }
  return std::make_tuple(menu_command_action_t::none, "");

}

std::tuple<menu_run_result_t, std::string> MenuInstance::Run() {
  if (!menu_.initialized()) {
    // There was an error loading the menu.
    finished = true;
    return std::make_tuple(menu_run_result_t::error, "");
  }

  // We have a good menu.
  for (;;) {
    if (!a()->user()->IsExpert()) {
      DisplayMenu();
    }
    bout << prompt_;
    // Do actions on enter.

    auto cmd = GetCommandFromUser();
    if (auto omi = GetMenuItemForCommand(cmd)) {
      const auto& mi = omi.value();
      bout << "Command is: " << cmd << "; " << mi.item_text << endl;
      for (const auto& action : mi.actions) {
        auto [a, d] = ExecuteAction(action);
        bout << "Action: " << action.cmd << "; " << action.data << endl;
        if (a == menu_command_action_t::push_menu) {
          return std::make_tuple(menu_run_result_t::push_menu, d);
        }
        if (a == menu_command_action_t::return_from_menu) {
          return std::make_tuple(menu_run_result_t::return_from_menu, "");
        }
      }
    }
  }

}

void MenuInstance::GenerateMenu() const {
  bout.Color(0);
  bout.nl();

  auto lines_displayed = 0;
  const auto nums = menu().num_action;
  if (nums != MENU_NUMFLAG_NOTHING) {
    bout.format("|#1{:<8} |#1{:<25}  ", "[#]", "Change Sub/Dir #");
    ++lines_displayed;
  }
  for (const auto& mi : menu().items) {
    if (mi.item_key.empty()) {
      continue;
    }
    if (!check_acs(mi.acs)) {
      continue;
    }
    const auto key = mi.item_key.size() == 1
        ? fmt::format("|#9[|#2{}|#9]", mi.item_key) : fmt::format("|#2//{}", mi.item_key);
    bout.format("{:<8} |#1{:<25}  ", key, mi.item_text);
    if (lines_displayed % 2) {
      bout.nl();
    }
    ++lines_displayed;
  }
  bout.nl(2);
}

}
