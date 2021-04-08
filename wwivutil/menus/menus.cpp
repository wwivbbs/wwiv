/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
/**************************************************************************/
#include "wwivutil/menus/menus.h"

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/menus/menu.h"
#include "wwivutil/util.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "sdk/menus/menu_set.h"

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::menus;
using namespace wwiv::strings;

constexpr char CD = 4;

namespace wwiv::wwivutil {

static void DisplayMenu(const menus::Menu56& m5) {
  std::cout << "Title: " << m5.menu.title;
  for (const auto& item : m5.menu.items) {
    std::cout << item.item_key << ") '" << item.item_text << "' ";
    for (const auto& a : item.actions) {
      std::cout << "[" << a.cmd;
      if (!a.data.empty()) {
        std::cout << " {" << a.data << "}";
      }
      std::cout << "] ";
    }
    std::cout << std::endl;
  }
}

class MenusDumpCommand final : public UtilCommand {
public:
  MenusDumpCommand() : UtilCommand("dump", "Displays the info of a menu") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   dump --menu_set=[menu set] <menu name>" << std::endl;
    ss << "Example: dump --menu_set=wwiv main.mnu" << std::endl << std::endl;
    ss << "         Note that menu_set defaults to 'wwiv'" << std::endl;
    return ss.str();
  }

  int Execute() override {
    const auto menu_set = sarg("menu_set");
    if (menu_set.empty()) {
      std::cout << "--menu_set is missing. " << std::endl;
      std::cout << GetUsage();
      return 2;
    }

    if (remaining().empty()) {
      std::cout << "<menu name> is missing. " << std::endl;
      std::cout << GetUsage();
      return 2;
    }
    const auto& menu_name = remaining().front();
    MenuSet56 menuset(FilePath(config()->config()->menudir(), menu_set));

    const menus::Menu56 m5(config()->config()->menudir(), menuset, menu_name);
    if (!m5.initialized()) {
      std::cout << "Unable to parse menu." << std::endl;
      return 1;
    }

    DisplayMenu(m5);

    return 0;
  }

  bool AddSubCommands() override {
    add_argument(BooleanCommandLineArgument("full", "Display full info about every command.", false));
    add_argument({"menu_set", "The menuset to use", "wwiv"});
    return true;
  }
};

class MenusConvertCommand final : public UtilCommand {
public:
  MenusConvertCommand() : UtilCommand("convert", "Converts a 4.3x menu to 5.6 format") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   convert --menu_set=<menu set> <menu name>" << std::endl;
    ss << "Example: convert --menu_set=wwiv main" << std::endl;
    ss << "         Note that menu_set defaults to 'wwiv'" << std::endl;
    return ss.str();
  }

  int Execute() override {
    const auto menu_set = sarg("menu_set");
    if (menu_set.empty()) {
      std::cout << "--menu_set is missing. " << std::endl;
      std::cout << GetUsage();
      return 2;
    }

    if (remaining().empty()) {
      std::cout << "<menu name> is missing. " << std::endl;
      std::cout << GetUsage();
      return 2;
    }
    const auto& menu_name = remaining().front();
    const menus::Menu430 m4(config()->config()->menudir(), menu_set, menu_name);
    if (!m4.initialized()) {
      std::cout << "Unable to 4.3 parse menu." << std::endl;
      return 1;
    }

    if (auto om5 = Create56MenuFrom43(m4, config()->config()->max_backups())) {
      auto& m5 = om5.value();
      return m5.Save() ? 0 : 1;
    }
    std::cout << "Failed to create 5.6 style menu from 4.3x style menu.";
    return 1;
  }

  bool AddSubCommands() override {
    add_argument({"menu_set", "The menuset to use", "wwiv"});
    return true;
  }
};

bool MenusCommand::AddSubCommands() {
  if (!add(std::make_unique<MenusDumpCommand>())) {
    return false;
  }
  if (!add(std::make_unique<MenusConvertCommand>())) {
    return false;
  }

  
  
  return true;
}

} // namespace wwiv
