/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#ifndef __INCLUDED_MENU_H__
#define __INCLUDED_MENU_H__

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "bbs/vars.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/textfile.h"
#include "sdk/menu.h"

namespace wwiv {
namespace menus {

class MenuInstanceData {
public:
  MenuInstanceData();
  ~MenuInstanceData();
  bool Open();
  void Close();
  void DisplayHelp() const;
  const std::string create_menu_filename(const std::string& extension) const;
  static const std::string create_menu_filename(
      const std::string& path, const std::string& menu, const std::string& extension);
  void Menus(const std::string& menuDirectory, const std::string& menuName);
  bool LoadMenuRecord(const std::string& command, MenuRec* pMenu);
  void GenerateMenu() const;

  std::string menu_;
  std::string path_;
  bool finished=false;
  bool reload=false;  /* true if we are going to reload the menus */

  std::string prompt;
  std::vector<std::string> insertion_order_;
  MenuHeader header;   /* Holds the header info for current menu set in memory */
private:
  bool CreateMenuMap(File* menu_file);
  std::map<std::string, MenuRec> menu_command_map_;
};

class MenuDescriptions {
public:
  MenuDescriptions(const std::string& menupath);
  ~MenuDescriptions();
  const std::string description(const std::string& name) const;
  bool set_description(const std::string& name, const std::string& description);

private:
  const std::string menupath_;
  std::map<std::string, std::string, wwiv::stl::ci_less> descriptions_;
};

// Functions used b bbs.cpp and defaults.cpp
void mainmenu();
void ConfigUserMenuSet();

// Functions used by menuedit and menu
const std::string GetMenuDirectory(const std::string menuPath);
const std::string GetMenuDirectory();
void MenuSysopLog(const std::string& pszMsg);

// Used by menuinterpretcommand.cpp
void TurnMCIOff();
void TurnMCIOn();

// In menuinterpretcommand.cpp
/**
 * Executes a menu command ```script``` using the menudata for the context of
 * the MENU, or nullptr if not invoked from an actual menu.
 */
void InterpretCommand(MenuInstanceData* menudata, const std::string& script);

}  // namespace menus
}  // namespace wwiv

#endif  // __INCLUDED_MENU_H__
