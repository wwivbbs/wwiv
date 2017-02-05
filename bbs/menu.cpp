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
#include "bbs/menu.h"
#include <cstdint>
#include <iomanip>
#include <memory>
#include <string>

#include "bbs/input.h"
#include "bbs/com.h"
#include "bbs/common.h"
#include "bbs/instmsg.h"
#include "bbs/menusupp.h"
#include "bbs/menu_parser.h"
#include "bbs/mmkey.h"
#include "bbs/newuser.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/sysoplog.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/utility.h"
#include "bbs/vars.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/findfiles.h"
#include "core/wwivassert.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv {
namespace menus {

static bool ValidateMenuSet(const std::string& menu_dir) {
  // ensure the entry point exists
  return File::Exists(GetMenuDirectory(menu_dir), "main.mnu");
}

static bool CheckMenuPassword(const string& original_password) {
  const string expected_password = (original_password == "*SYSTEM") 
      ? a()->config()->config()->systempw : original_password;
  bout.nl();
  const auto actual_password = input_password("|#2SY: ", 20);
  return actual_password == expected_password;
}

static void StartMenus() {
  while (true) {
    CheckForHangup();
    if (!ValidateMenuSet(a()->user()->menu_set())) {
      ConfigUserMenuSet();
    }
    MenuInstance menu_data(a()->user()->menu_set(), "main");
    menu_data.RunMenu(); // default starting menu
    if (!menu_data.reload) {
      return;
    }
  }
}

static bool CheckMenuSecurity(const MenuHeader* pHeader, bool bCheckPassword) {
  if ((pHeader->nFlags & MENU_FLAG_DELETED) ||
    (a()->GetEffectiveSl() < pHeader->nMinSL) ||
    (a()->user()->GetDsl() < pHeader->nMinDSL)) {
    return false;
  }

  // All AR bits specified must match
  for (short int x = 0; x < 16; x++) {
    if (pHeader->uAR & (1 << x)) {
      if (!a()->user()->HasArFlag(1 << x)) {
        return false;
      }
    }
  }

  // All DAR bits specified must match
  for (short int x = 0; x < 16; x++) {
    if (pHeader->uDAR & (1 << x)) {
      if (!a()->user()->HasDarFlag(1 << x)) {
        return (a()->user()->GetDsl() < pHeader->nMinDSL);
      }
    }
  }

  // If any restrictions match, then they arn't allowed
  for (short int x = 0; x < 16; x++) {
    if (pHeader->uRestrict & (1 << x)) {
      if (a()->user()->HasRestrictionFlag(1 << x)) {
        return (a()->user()->GetDsl() < pHeader->nMinDSL);
      }
    }
  }

  if ((pHeader->nSysop && !so()) ||
    (pHeader->nCoSysop && !cs())) {
    return false;
  }

  if (pHeader->szPassWord[0] && bCheckPassword) {
    if (!CheckMenuPassword(pHeader->szPassWord)) {
      return false;
    }
  }
  return true;
}

static bool CheckMenuItemSecurity(const MenuRec* pMenu, bool bCheckPassword) {
  // if deleted, return as failed
  if ((pMenu->nFlags & MENU_FLAG_DELETED) ||
    (a()->GetEffectiveSl() < pMenu->nMinSL) ||
    (a()->GetEffectiveSl() > pMenu->iMaxSL && pMenu->iMaxSL != 0) ||
    (a()->user()->GetDsl() < pMenu->nMinDSL) ||
    (a()->user()->GetDsl() > pMenu->iMaxDSL && pMenu->iMaxDSL != 0)) {
    return false;
  }

  // All AR bits specified must match
  for (int x = 0; x < 16; x++) {
    if (pMenu->uAR & (1 << x)) {
      if (!a()->user()->HasArFlag(1 << x)) {
        return false;
      }
    }
  }

  // All DAR bits specified must match
  for (int x = 0; x < 16; x++) {
    if (pMenu->uDAR & (1 << x)) {
      if (!a()->user()->HasDarFlag(1 << x)) {
        return false;
      }
    }
  }

  // If any restrictions match, then they arn't allowed
  for (int x = 0; x < 16; x++) {
    if (pMenu->uRestrict & (1 << x)) {
      if (a()->user()->HasRestrictionFlag(1 << x)) {
        return false;
      }
    }
  }

  if ((pMenu->nSysop && !so()) ||
    (pMenu->nCoSysop && !cs())) {
    return false;
  }

  if (pMenu->szPassWord[0] && bCheckPassword) {
    if (!CheckMenuPassword(pMenu->szPassWord)) {
      return false;
    }
  }

  // If you made it past all of the checks
  // then you may execute the menu record
  return true;
}

static void LogUserFunction(
  const MenuInstance* menu_data,
  const string& command, 
  const MenuRec* pMenu /* Nullable */) {
  if (pMenu == nullptr) {
    sysopchar(command);
    return;
  }

  switch (menu_data->header.nLogging) {
  case MENU_LOGTYPE_KEY:
    sysopchar(command);
    break;
  case MENU_LOGTYPE_COMMAND:
    sysoplog() << pMenu->szExecute;
    break;
  case MENU_LOGTYPE_DESC:
    sysoplog() << (pMenu->szMenuText[0] ? pMenu->szMenuText : pMenu->szExecute);
    break;
  case MENU_LOGTYPE_NONE:
  default:
    break;
  }
}

void mainmenu() {
  while (true) {
    StartMenus();
  }
}

void MenuInstance::RunMenu() {
  if (!open_ && iequals(menu_name_, "main")) {
    Hangup();
    return;
  }

  if (header.nums == MENU_NUMFLAG_DIRNUMBER && a()->udir[0].subnum == -1) {
    bout << "\r\nYou cannot currently access the file section.\r\n\n";
    return;
  }

  // if flagged to display help on entrance, then do so
  if (a()->user()->IsExpert() && header.nForceHelp == MENU_HELP_ONENTRANCE) {
    DisplayHelp();
  }

  while (!finished) {
    PrintMenuPrompt();
    const auto cmd = GetCommand();
    MenuExecuteCommand(cmd);
  }
}

MenuInstance::MenuInstance(const std::string& menu_directory, const std::string& menu_name)
: menu_directory_(menu_directory), menu_name_(menu_name) {
// OpenImpl needs the class contructed, so this must happen in the constructor
// and not in the initializer list.
open_ = OpenImpl();
}

MenuInstance::~MenuInstance() {}

//static
const std::string MenuInstance::create_menu_filename(
    const std::string& path, const std::string& menu, const std::string& extension) {
  return StrCat(GetMenuDirectory(), path, File::pathSeparatorString, menu, ".", extension);
}

std::string MenuInstance::create_menu_filename(const string& extension) const {
  return MenuInstance::create_menu_filename(menu_directory_, menu_name_, extension);
}

bool MenuInstance::CreateMenuMap(File& menu_file) {
  insertion_order_.clear();
  auto nAmount = menu_file.GetLength() / sizeof(MenuRec);

  for (size_t nRec = 1; nRec < nAmount; nRec++) {
    MenuRec menu{};
    menu_file.Seek(nRec * sizeof(MenuRec), File::Whence::begin);
    menu_file.Read(&menu, sizeof(MenuRec));

    string key = menu.szKey;
    menu_command_map_.insert(std::make_pair(key, menu));
    if (nRec != 0 && !(menu.nFlags & MENU_FLAG_DELETED)) {
      insertion_order_.push_back(menu.szKey);
    }
  }
  return true;
}

bool MenuInstance::OpenImpl() {
  // Open up the main data file
  File menu_file(create_menu_filename("mnu"));
  if (!menu_file.Open(File::modeBinary | File::modeReadOnly, File::shareDenyNone))  {
    // Unable to open menu
    MenuSysopLog("Unable to open Menu");
    return false;
  }

  // Read the header (control) record into memory
  menu_file.Seek(0L, File::Whence::begin);
  menu_file.Read(&header, sizeof(MenuHeader));

  // Version numbers can be checked here.
  if (!CreateMenuMap(menu_file)) {
    MenuSysopLog("Unable to create menu index.");
    return false;
  }

  if (!CheckMenuSecurity(&header, true)) {
    MenuSysopLog("< Menu Sec");
    return false;
  }

  // Open/Rease/Close Prompt file.  We use binary mode since we want the
  // \r to remain on windows (and linux).
  TextFile prompt_file(create_menu_filename("pro"), "rb");
  if (prompt_file.IsOpen()) {
    string tmp = prompt_file.ReadFileIntoString();
    string::size_type end = tmp.find(".end.");
    if (end != string::npos) {
      prompt = tmp.substr(0, end);
    } else {
      prompt = tmp;
    }
  }
  else {
    prompt = "|09Command? ";
  }

  // Execute command to use on entering the menu (if any).
  if (header.szScript[0]) {
    InterpretCommand(this, header.szScript);
  }
  return true;
}

string MenuInstance::GetHelpFileName() const {
  if (a()->user()->HasAnsi()) {
    if (a()->user()->HasColor()) {
      const string filename = create_menu_filename("ans");
      if (File::Exists(filename)) {
        return filename;
      }
    }
    const string filename = create_menu_filename("b&w");
    if (File::Exists(filename)) {
      return filename;
    }
  }
  return create_menu_filename("msg");
}

void MenuInstance::DisplayHelp() const {
  const auto filename = GetHelpFileName();
  if (!printfile(filename, true)) {
    GenerateMenu();
  }
}

static bool IsNumber(const string& command) {
  if (!command.length()) {
    return false;
  }
  for (const auto& ch : command) {
    if (!isdigit(ch)) {
      return false;
    }
  }
  return true;
}

static std::map<int, std::string> ListMenuDirs() {
  std::map<int, std::string> result;
  const string menu_directory = GetMenuDirectory();
  wwiv::menus::MenuDescriptions descriptions(menu_directory);

  bout.nl();
  bout << "|#1Available Menus Sets" << wwiv::endl
       << "|#9============================" << wwiv::endl;

  int num = 1;
  auto menus = FindFiles(menu_directory, "*", FindFilesType::directories);

  for (const auto& m : menus) {
    const auto& filename = m.name;
    const string description = descriptions.description(filename);
    bout.bprintf("|#2#%d |#1%-8.8s |#9%-60.60s\r\n", num, filename.c_str(), description.c_str());
    result.emplace(num, filename);
    ++num;
  }
  bout.nl();
  bout.Color(0);
  return result;
}

std::vector<MenuRec> MenuInstance::LoadMenuRecord(const std::string& command) {
  std::vector<MenuRec> result;
  // If we have 'numbers set the sub #' turned on then create a command to do so if a # is entered.
  if (IsNumber(command)) {
    if (header.nums == MENU_NUMFLAG_SUBNUMBER) {
      MenuRec r{};
      sprintf(r.szExecute, "SetSubNumber %d", atoi(command.c_str()));
      result.push_back(std::move(r));
      return result;
    } else if (header.nums == MENU_NUMFLAG_DIRNUMBER) {
      MenuRec r{};
      sprintf(r.szExecute, "SetDirNumber %d", atoi(command.c_str()));
      result.push_back(std::move(r));
      return result;
    }
  }

  if (menu_command_map_.count(command) > 0) {
    auto range = menu_command_map_.equal_range(command);
    for (auto i = range.first; i != range.second; ++i) {
      auto& m = i->second;
      if (CheckMenuItemSecurity(&m, true)) {
        result.push_back(m);
      }
      else {
        MenuSysopLog(StrCat("|06< item security : ", command));
      }
    }
  }
  return result;
}

void MenuInstance::MenuExecuteCommand(const string& command) {
  const auto menus = LoadMenuRecord(command);
  if (menus.empty()) {
    LogUserFunction(this, command, nullptr);
    return;
  }

  for (const auto& menu : menus) {
    LogUserFunction(this, command, &menu);
    InterpretCommand(this, menu.szExecute);
  }
}

void MenuSysopLog(const string& msg) {
  const string log_message = StrCat("*MENU* : ", msg);
  sysoplog() << log_message;
  bout << log_message << wwiv::endl;
}

void MenuInstance::PrintMenuPrompt() const {
  if (!a()->user()->IsExpert() || header.nForceHelp == MENU_HELP_FORCE) {
    DisplayHelp();
  }
  TurnMCIOn();
  if (!prompt.empty()) {
    bout << prompt;
  }
}

void TurnMCIOff() {
  g_flags |= g_flag_disable_mci;
}

void TurnMCIOn() {
  g_flags &= ~g_flag_disable_mci;
}

void ConfigUserMenuSet() {
  bout.cls();
  bout.litebar("Configure Menus");
  printfile(MENUWEL_NOEXT);
  bool done = false;
  while (!done) {
    bout.nl();
    bout << "|#11|#9) Menuset      :|#2 " << a()->user()->menu_set() << wwiv::endl;
    bout << "|#12|#9) Use hot keys :|#2 " << (a()->user()->hotkeys() ? "Yes" : "No ")
         << wwiv::endl;
    bout.nl();
    bout << "|#9(|#2Q|#9=|#1Quit|#9) : ";
    char chKey = onek("Q12?");

    switch (chKey) {
    case 'Q':
      done = true;
      break;
    case '1': {
      auto r = ListMenuDirs();
      bout.nl(2);
      bout << "|#9Enter the menu set to use : ";
      int sel = input_number<int>(1, 1, r.size(), false);
      const string menuSetName = r.at(sel);
      if (ValidateMenuSet(menuSetName.c_str())) {
        wwiv::menus::MenuDescriptions descriptions(GetMenuDirectory());
        bout.nl();
        bout << "|#9Menu Set : |#2" <<  menuSetName << " :  |#1" << descriptions.description(menuSetName) << wwiv::endl;
        bout << "|#5Use this menu set? ";
        if (noyes()) {
          a()->user()->set_menu_set(menuSetName);
          break;
        }
      }
      bout.nl();
      bout << "|#6That menu set does not exists, resetting to the default menu set" << wwiv::endl;
      pausescr();
      if (a()->user()->menu_set().empty()) {
        a()->user()->set_menu_set("wwiv");
      }
    }
    break;
    case '2':
      a()->user()->set_hotkeys(!a()->user()->hotkeys());
      break;
    case '?':
      printfile(MENUWEL_NOEXT);
      continue;                           // bypass the below cls()
    }
    bout.cls();
  }

  // If menu is invalid, it picks the first one it finds
  if (!ValidateMenuSet(a()->user()->menu_set())) {
    if (a()->languages.size() > 1 && a()->user()->GetLanguage() != 0) {
      bout << "|#6No menus for " << a()->languages[a()->user()->GetLanguage()].name
           << " language.";
      input_language();
    }
  }

  // Save current menu setup.
  a()->WriteCurrentUser();

  MenuSysopLog(StringPrintf("Menu in use : %s - %s",
    a()->user()->menu_set().c_str(),
    a()->user()->hotkeys() ? "Hot" : "Off"));
  bout.nl(2);
}

string MenuInstance::GetCommand() const {
  if (!a()->user()->hotkeys()) {
    return input(50);
  }
  if (header.nums == MENU_NUMFLAG_DIRNUMBER) {
    write_inst(INST_LOC_XFER, a()->current_user_dir().subnum, INST_FLAGS_NONE);
    return mmkey(MMKeyAreaType::dirs);
  } else if (header.nums == MENU_NUMFLAG_SUBNUMBER) {
    write_inst(INST_LOC_MAIN, a()->current_user_sub().subnum, INST_FLAGS_NONE);
    return mmkey(MMKeyAreaType::subs);
  } else {
    std::set<char> x = { '/' };
    return mmkey(x);
  }
}

MenuDescriptions::MenuDescriptions(const std::string& menupath) :menupath_(menupath) {
  TextFile file(menupath, DESCRIPT_ION, "rt");
  if (file.IsOpen()) {
    string s;
    while (file.ReadLine(&s)) {
      StringTrim(&s);
      string::size_type space = s.find(' ');
      if (s.empty() || space == string::npos) {
        continue;
      }
      descriptions_.emplace(ToStringLowerCase(s.substr(0, space)), 
          ToStringLowerCase(s.substr(space + 1)));
    }
  }
}

MenuDescriptions::~MenuDescriptions() {}

const std::string MenuDescriptions::description(const std::string& name) const {
  if (contains(descriptions_, name)) {
    return descriptions_.at(name);
  }
  return "";
}

bool MenuDescriptions::set_description(const std::string& name, const std::string& description) {
  descriptions_[name] = description;

  TextFile file(menupath_, DESCRIPT_ION, "wt");
  if (!file.IsOpen()) {
    return false;
  }

  for (const auto& iter : descriptions_) {
    file.WriteFormatted("%s %s", iter.first.c_str(), iter.second.c_str());
  }
  return true;
}

const string GetMenuDirectory() {
  return StrCat(a()->language_dir, "menus", File::pathSeparatorString);
}

const string GetMenuDirectory(const string menuPath) {
  return StrCat(GetMenuDirectory(), menuPath, File::pathSeparatorString);
}

void MenuInstance::GenerateMenu() const {
  bout.Color(0);
  bout.nl();

  int lines_displayed = 0;
  if (header.nums != MENU_NUMFLAG_NOTHING) {
    bout.bprintf("|#1%-8.8s  |#2%-25.25s  ", "[#]", "Change Sub/Dir #");
    ++lines_displayed;
  }
  for (const auto& key : insertion_order_) {
    if (menu_command_map_.count(key) == 0) {
      continue;
    }
    const MenuRec& menu = menu_command_map_.find(key)->second;
    if (CheckMenuItemSecurity(&menu, false) &&
        menu.nHide != MENU_HIDE_REGULAR &&
        menu.nHide != MENU_HIDE_BOTH) {
      string keystr;
      if (strlen(menu.szKey) > 1 && menu.szKey[0] != '/' && a()->user()->hotkeys()) {
        keystr = StrCat("//", menu.szKey);
      } else {
        keystr = StrCat("[", menu.szKey, "]");
      }
      bout.Color(1);
      bout << std::left << std::setw(8) << keystr << "  ";
      bout.Color(9);
      bout << std::left << std::setw(25) << (menu.szMenuText[0] ? menu.szMenuText : menu.szExecute);
      if (lines_displayed % 2) {
        bout.nl();
      }
      ++lines_displayed;
    }
  }
  if (IsEquals(a()->user()->GetName(), "GUEST")) {
    if (lines_displayed % 2) {
      bout.nl();
    }
    bout.bprintf("|#1%-8.8s  |#2%-25.25s  ",
      a()->user()->hotkeys() ? "//APPLY" : "[APPLY]",
      "Guest Account Application");
    ++lines_displayed;
  }
  bout.nl(2);
  return;
}

}  // namespace menus
}  // namespace wwiv
