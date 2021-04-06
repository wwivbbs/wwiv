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
#ifndef INCLUDED_BBS_MENUS_MENU_CONTEXT_H
#define INCLUDED_BBS_MENUS_MENU_CONTEXT_H

#include "core/stl.h"
#include <filesystem>
#include <string>

namespace wwiv::bbs::menus {
class Menu;

enum class menu_command_action_t { none, push_menu, return_from_menu };

struct MenuContext {
  MenuContext(Menu* m, std::string d)
      : cur_menu(m), data(std::move(d)) {}
  // May be null if not invoked from an actual menu.
  Menu* cur_menu;
  std::string data;
  menu_command_action_t menu_action{menu_command_action_t::none};
  bool need_reload{false};
};

}  // namespace

#endif
