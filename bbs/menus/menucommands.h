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
#ifndef INCLUDED_MENUS_MENUCOMMANDS_H
#define INCLUDED_MENUS_MENUCOMMANDS_H

#include "core/stl.h"
#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace wwiv::bbs::menus {
class MenuInstance;

enum class menu_command_action_t { none, push_menu, return_from_menu };

struct MenuItemContext {
  MenuItemContext(MenuInstance* m, std::string data)
      : pMenuData(m), param1(std::move(data)) {}
  // May be null if not invoked from an actual menu.
  MenuInstance* pMenuData;
  std::string param1;
  menu_command_action_t menu_action{menu_command_action_t::none};
  std::string new_menu_name;
  bool finished{false};
  bool need_reload{false};
};

struct MenuItem {
  MenuItem(std::string desc, std::string category, std::function<void(MenuItemContext&)> f)
      : description_(std::move(desc)), category_(std::move(category)), f_(std::move(f)) {}
  MenuItem(std::string desc, std::function<void(MenuItemContext&)> f)
      : description_(std::move(desc)), f_(std::move(f)) {}

  explicit MenuItem(std::function<void(MenuItemContext&)> f) : description_(""), f_(std::move(f)) {}

  std::string description_;
  std::string category_;
  std::function<void(MenuItemContext&)> f_;
  // Set at the end of CreateMenuMap.
  std::string cmd_;
};

std::map<std::string, MenuItem, wwiv::stl::ci_less> CreateCommandMap();

// In menuinterpretcommand.cpp
/**
 * Executes a menu command ```script``` using the menudata for the context of
 * the MENU, or nullptr if not invoked from an actual menu.
 */
std::optional<MenuItemContext> InterpretCommand(MenuInstance* menudata, const std::string& cmd, const std::string& data);
void PrintMenuCommands(const std::string& arg);

}  // namespace

#endif
