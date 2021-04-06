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
#ifndef INCLUDED_MENUS_MENUCOMMANDS_H
#define INCLUDED_MENUS_MENUCOMMANDS_H

#include "bbs/menus/menu_context.h"
#include "core/stl.h"
#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace wwiv::bbs::menus {
class Menu;

struct MenuItem {
  MenuItem(std::string desc, std::string category, std::function<void(MenuContext&)> f)
      : description_(std::move(desc)), category_(std::move(category)), f_(std::move(f)) {}
  MenuItem(std::string desc, std::function<void(MenuContext&)> f)
      : description_(std::move(desc)), f_(std::move(f)) {}

  explicit MenuItem(std::function<void(MenuContext&)> f) : f_(std::move(f)) {}

  std::string description_;
  std::string category_;
  std::function<void(MenuContext&)> f_;
  // Set at the end of CreateMenuMap.
  std::string cmd_;
};

std::map<std::string, MenuItem, wwiv::stl::ci_less> CreateCommandMap();

/**
 * Executes a menu command ```script``` using the menu data for the context of
 * the MENU, or nullptr if not invoked from an actual menu.
 */
std::optional<MenuContext> interpret_command(Menu* menu, const std::string& cmd, const std::string& data);

}  // namespace

#endif
