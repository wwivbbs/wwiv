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
#ifndef INCLUDED_MENUS_MAINMENU_H
#define INCLUDED_MENUS_MAINMENU_H

#include "menucommands.h"
#include "sdk/menus/menu.h"
#include "sdk/menus/menu_set.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace wwiv::sdk {
class Config;
class User;
}

namespace wwiv::bbs::menus {
// Functions used b bbs.cpp and defaults.cpp
void mainmenu();

enum class menu_run_result_t {
  none,
  error,
  push_menu,
  return_from_menu,
  clear_stack,
  change_menu_set
};

class Menu {
public:
  Menu() = delete;
  Menu(const Menu&) = default;
  Menu(Menu&&) noexcept = default;
  Menu& operator=(const Menu&) = default;
  Menu& operator=(Menu&&) = default;
  Menu(const std::filesystem::path& menu_path, const wwiv::sdk::menus::MenuSet56& menu_set,
               const std::string& menu_name);
  ~Menu() = default;
  [[nodiscard]] bool initalized() const { return menu_.initialized(); }
  // Gets the command string from the user for this menu.
  [[nodiscard]] std::string GetCommandFromUser() const;
  [[nodiscard]] std::optional<wwiv::sdk::menus::menu_item_56_t>
  GetMenuItemForCommand(const std::string& cmd) const;
  void DisplayMenu();
  // Generates the short form (multi-column) or long form (single col, help text) menu.
  [[nodiscard]] std::vector<std::string> GenerateMenuAsLines(sdk::menus::menu_type_t typ);
  // Generates the short form (multi-column) or long form (single col, help text) menu.
  void GenerateMenu(sdk::menus::menu_type_t typ);
  [[nodiscard]] const sdk::menus::menu_56_t& menu() const noexcept { return menu_.menu; }
  [[nodiscard]] std::tuple<menu_command_action_t, std::string>
  ExecuteAction(const sdk::menus::menu_action_56_t& a);
  [[nodiscard]] std::tuple<menu_command_action_t, std::string>
  ExecuteActions(const std::vector<wwiv::sdk::menus::menu_action_56_t>& actions);
  [[nodiscard]] std::tuple<menu_run_result_t, std::string> Run();

  bool reload{false}; /* true if we are going to reload the menus */
  bool menu_displayed_{false};

private:
  const std::string menu_set_name_;
  const std::string menu_name_;
  const wwiv::sdk::menus::MenuSet56 menu_set_;
  sdk::menus::Menu56 menu_;
  std::filesystem::path menu_set_path_;
  std::string prompt_;
};

class MainMenu {
public:
  explicit MainMenu(const sdk::Config& config);
  void Run();

  wwiv::sdk::menus::MenuSet56 menu_set_;
  const sdk::Config& config_;

private:
  std::vector<Menu> stack_;
};

} // namespace wwiv::bbs::menus

#endif
