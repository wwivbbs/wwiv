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
#ifndef INCLUDED_MENUS_MAINMENU_H
#define INCLUDED_MENUS_MAINMENU_H

#include "menucommands.h"
#include "bbs/oldmenu.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/textfile.h"
#include "sdk/menus/menu.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace wwiv::sdk {
class Config;
}

namespace wwiv::bbs::menus {
// Functions used b bbs.cpp and defaults.cpp
void mainmenu();


enum class menu_run_result_t { none, error, push_menu, return_from_menu, clear_stack, change_menu_set };

class MenuInstance {
public:
  MenuInstance(const std::filesystem::path& menu_path, const std::string& menu_set, const std::string&
               menu_name);
  ~MenuInstance() = default;
  [[nodiscard]] bool initalized() const { return menu_.initialized(); }
  // Gets the command string from the user for this menu.
  [[nodiscard]] std::string GetCommandFromUser() const;
  [[nodiscard]] std::optional<wwiv::sdk::menus::menu_item_56_t> GetMenuItemForCommand(const std::string& cmd);
  void DisplayMenu() const;
  [[nodiscard]] const sdk::menus::menu_56_t& menu() const noexcept { return menu_.menu; }
  std::tuple<menu_command_action_t, std::string> ExecuteAction(const sdk::menus::menu_action_56_t& value);
  std::tuple<menu_run_result_t, std::string> Run();

  bool finished{false};
  bool reload{false};  /* true if we are going to reload the menus */


private:
  const std::string menu_set_;
  const std::string menu_name_;
  sdk::menus::Menu56 menu_;
  std::filesystem::path menu_set_path_;
  std::string prompt_;
};

class MainMenu {
public:
  MainMenu(const sdk::Config& config);
  void Run();

  std::string menu_set_;
  const sdk::Config& config_;

private:
  std::vector<MenuInstance> stack_;
};

// Functions used by menuedit and menu
void MenuSysopLog(const std::string& pszMsg);

// In menuinterpretcommand.cpp
/**
 * Executes a menu command ```script``` using the menudata for the context of
 * the MENU, or nullptr if not invoked from an actual menu.
 */
void InterpretCommand(MenuInstance* menudata, const std::string& script);
void PrintMenuCommands(const std::string& arg);

}  // namespace

#endif
