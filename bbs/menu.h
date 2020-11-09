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
#ifndef INCLUDED_MENU_H
#define INCLUDED_MENU_H

#include "core/file.h"
#include "core/stl.h"
#include "core/textfile.h"
#include "sdk/menus/menu.h"
#include <map>
#include <string>
#include <vector>

namespace wwiv::menus {

class MenuInstance {
public:
  MenuInstance(std::string menuDirectory, std::string menuName);
  ~MenuInstance();
  void DisplayMenu() const;
  static std::string create_menu_filename(
      const std::string& path, const std::string& menu, const std::string& extension);
  void RunMenu();
  std::vector<menu_rec_430_t> LoadMenuRecord(const std::string& command);
  void GenerateMenu() const;

  std::string menu_directory() { return menu_directory_; }

  bool finished = false;
  bool reload = false;  /* true if we are going to reload the menus */

  std::string prompt;
  std::vector<std::string> insertion_order_;
  menu_header_430_t header{};   /* Holds the header info for current menu set in memory */
private:
  const std::string menu_directory_;
  const std::string menu_name_;
  bool open_ = false;

  bool OpenImpl();
  std::string GetHelpFileName() const;
  std::string create_menu_filename(const std::string& extension) const;

  void MenuExecuteCommand(const std::string& command);
  bool CreateMenuMap(wwiv::core::File& menu_file);
  void PrintMenuPrompt() const;
  std::string GetCommand() const;

  std::multimap<std::string, menu_rec_430_t> menu_command_map_;
};

class MenuDescriptions {
public:
  MenuDescriptions(const std::filesystem::path& menupath);
  ~MenuDescriptions();
  std::string description(const std::string& name) const;
  bool set_description(const std::string& name, const std::string& description);

private:
  const std::filesystem::path menupath_;
  std::map<std::string, std::string, wwiv::stl::ci_less> descriptions_;
};

// Functions used b bbs.cpp and defaults.cpp
void mainmenu();
void ConfigUserMenuSet();

// Functions used by menuedit and menu
void MenuSysopLog(const std::string& pszMsg);

// Used by menuinterpretcommand.cpp
void TurnMCIOff();
void TurnMCIOn();

// In menuinterpretcommand.cpp
/**
 * Executes a menu command ```script``` using the menudata for the context of
 * the MENU, or nullptr if not invoked from an actual menu.
 */
void InterpretCommand(MenuInstance* menudata, const std::string& script);
void PrintMenuCommands(const std::string& arg);

}  // namespace

#endif
