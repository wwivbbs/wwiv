/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include <cstdint>
#include <string>

#include "bbs/wwiv.h"
#include "bbs/input.h"
#include "bbs/menu.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/wfndfile.h"

using std::string;
using wwiv::bbs::InputMode;

using namespace wwiv::menus;
using namespace wwiv::strings;

bool GetMenuMenu(const string& pszDirectoryName, string& menuName);
void ReadMenuRec(File &fileEditMenu, MenuRec * menu, int nCur);
void WriteMenuRec(File &fileEditMenu, MenuRec * menu, int nCur);
void DisplayItem(MenuRec * menu, int nCur, int nAmount);
void DisplayHeader(MenuHeader* Header, int nCur, const string& dirname);

static void ListMenuMenus(const string& directory_name) {
  bout.nl();
  bout << "|#1Available Menus" << wwiv::endl 
       << "|#9===============" << wwiv::endl;

  WFindFile fnd;
  bool bFound = fnd.open(StrCat(GetMenuDirectory(directory_name), "*.mnu"), 0);
  while (bFound && !hangup) {
    if (fnd.IsFile()) {
      const string s = fnd.GetFileName();
      bout << "|#2" << s.substr(0, s.find_last_of('.')) << wwiv::endl;
    }
    bFound = fnd.next();
  }
  bout.nl();
  bout.Color(0);
}

static bool EditHeader(MenuHeader* header, File &fileEditMenu, const string& menuDir, int& nAmount, int& nCur) {
  bool done = false;
  char szTemp1[21];
  DisplayHeader(header, 0, menuDir);
  char chKey = onek("Q[]Z012ABCDEFGHIJKLMNOP");
  switch (chKey) {
  case 'Q':
    WriteMenuRec(fileEditMenu, (MenuRec*) header, nCur);
    done = true;
    break;
  case '[':
    WriteMenuRec(fileEditMenu, (MenuRec*) header, nCur);
    nAmount = static_cast<uint16_t>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
    --(nCur);
    if ((nCur) < 0) {
      nCur = nAmount;
    }
    ReadMenuRec(fileEditMenu, (MenuRec*) header, nCur);
    break;
  case ']':
    WriteMenuRec(fileEditMenu, (MenuRec*) header, nCur);
    nAmount = static_cast<uint16_t>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
    (nCur)++;
    if (nCur > nAmount) {
      (nCur) = 0;
    }
    ReadMenuRec(fileEditMenu, (MenuRec*) header, nCur);
    break;
  case 'Z':
    WriteMenuRec(fileEditMenu, (MenuRec*) header, nCur);
    memset(header, 0, sizeof(MenuRec));
    nAmount = static_cast<uint16_t>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
    (nCur) = nAmount + 1;

    // TODO(rushfan): WTF is this for?
    ((MenuRec*) header)->iMaxSL = 255;
    ((MenuRec*) header)->iMaxDSL = 255;

    WriteMenuRec(fileEditMenu, (MenuRec*) header, nCur);
    nAmount = static_cast<uint16_t>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
    ReadMenuRec(fileEditMenu, (MenuRec*) header, nCur);
    break;
  case '0': {
    wwiv::menus::MenuDescriptions descriptions(GetMenuDirectory());
    string description = descriptions.description(menuDir);

    bout << "|#5New desc     : ";
    bout.Color(0);
    inputl(&description, 60);
    if (!description.empty()) {
      descriptions.set_description(menuDir, description);
    }
  } break;
  case '1':
    bout << "Is Menu deleted? (N) ";
    if (noyes()) {
      header->nFlags |= MENU_FLAG_DELETED;
    } else {
      header->nFlags &= ~MENU_FLAG_DELETED;
    }
    break;
  case '2':
    bout << "Is Menu a main menu? (Y) ";
    if (yesno()) {
      header->nFlags |= MENU_FLAG_MAINMENU;
    } else {
      header->nFlags &= ~MENU_FLAG_MAINMENU;
    }
    break;
  case 'A':
    header->nNumbers++;
    if (header->nNumbers == MENU_NUMFLAG_LAST) {
      header->nNumbers = 0;
    }
    break;
  case 'B':
    header->nLogging++;
    if (header->nLogging == MENU_LOGTYPE_LAST) {
      header->nLogging = 0;
    }
    break;
  case 'C':
    header->nForceHelp++;
    if (header->nForceHelp == MENU_HELP_LAST) {
      header->nForceHelp = 0;
    }
    break;
  case 'D':
    header->nAllowedMenu++;
    if (header->nAllowedMenu == MENU_ALLOWED_LAST) {
      header->nAllowedMenu = 0;
    }
    break;
  case 'F':
    bout << "Command to execute : ";
    inputl(header->szScript, 100);
    break;
  case 'G':
    bout << "Script for when menu ends : ";
    inputl(header->szExitScript, 100);
    break;
  case 'H':
    bout << "Min SL : ";
    input(szTemp1, 3);
    if (szTemp1[0]) {
      header->nMinSL = StringToShort(szTemp1);
    }
    break;
  case 'I':
    bout << "Min DSL : ";
    input(szTemp1, 3);
    if (szTemp1[0]) {
      header->nMinDSL = StringToShort(szTemp1);
    }
    break;
  case 'J':
    bout << "AR : ";
    input(szTemp1, 5);
    if (szTemp1[0]) {
      header->uAR = StringToUnsignedShort(szTemp1);
    }
    break;
  case 'K':
    bout << "DAR : ";
    input(szTemp1, 5);
    if (szTemp1[0]) {
      header->uDAR = StringToUnsignedShort(szTemp1);
    }
    break;
  case 'L':
    bout << "Restrictions : ";
    input(szTemp1, 5);
    if (szTemp1[0]) {
      header->uRestrict = StringToUnsignedShort(szTemp1);
    }
    break;
  case 'M':
    header->nSysop = !header->nSysop;
    break;
  case 'N':
    header->nCoSysop = !header->nCoSysop;
    break;
  case 'O':
    if (incom && header->szPassWord[0]) {
      bout << "Current PW: ";
      string pw;
      input(&pw, 20);
      if (!IsEqualsIgnoreCase(pw.c_str(), header->szPassWord)) {
        MenuSysopLog("Unable to change PW");
        break;
      }
    }
    bout << "   New PW : ";
    input(header->szPassWord, 20);
    break;
  }
  return done;
}

static bool EditMenuItem(MenuRec* menu, File &fileEditMenu, int& nAmount, int& nCur) {
  bool done = false;
  char szPW[21];
  char szTemp1[21];
  DisplayItem(menu, nCur, nAmount);
  char chKey = onek("Q[]Z1ABCDEFGKLMNOPRSTUVW");

  switch (chKey) {
  case 'Q':
    WriteMenuRec(fileEditMenu, menu, nCur);
    done = true;
    break;

  case '[':
    WriteMenuRec(fileEditMenu, menu, nCur);
    nAmount = static_cast<uint16_t>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
    --nCur;
    if (nCur < 0) {
      nCur = nAmount;
    }
    ReadMenuRec(fileEditMenu, menu, nCur);
    break;
  case ']':
    WriteMenuRec(fileEditMenu, menu, nCur);
    nAmount = static_cast<uint16_t>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
    ++nCur;
    if (nCur > nAmount) {
      nCur = 0;
    }
    ReadMenuRec(fileEditMenu, menu, nCur);
    break;
  case 'Z':
    WriteMenuRec(fileEditMenu, menu, nCur);
    memset(menu, 0, sizeof(MenuRec));
    nAmount = static_cast<uint16_t>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
    nCur = nAmount + 1;
    memset(menu, 0, sizeof(MenuRec));
    menu->iMaxSL = 255;
    menu->iMaxDSL = 255;
    WriteMenuRec(fileEditMenu, menu, nCur);
    nAmount = static_cast<uint16_t>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
    ReadMenuRec(fileEditMenu, menu, nCur);
    break;
  case '1':
    bout << "Is record deleted? (N) ";
    if (yesno()) {
      menu->nFlags |= MENU_FLAG_DELETED;
    } else {
      menu->nFlags &= ~MENU_FLAG_DELETED;
    }
    break;
  case 'A':
    bout << "Key to cause execution : ";
    input(menu->szKey, MENU_MAX_KEYS);
    if (!(menu->szSysopLog[0])) {
      strcpy(menu->szSysopLog, menu->szKey);
    }
    break;
  case 'B':
    bout << "Command to execute : ";
    inputl(menu->szExecute, 100);
    if (!(menu->szMenuText[0])) {
      strcpy(menu->szMenuText, menu->szExecute);
    }
    break;
  case 'C':
    bout << "Menu Text : ";
    inputl(menu->szMenuText, 40);
    break;
  case 'E':
    bout << "Help Text : ";
    inputl(menu->szHelp, 80);
    break;
  case 'F':
    bout << "Instance Message : ";
    inputl(menu->szInstanceMessage, 80);
    break;
  case 'G':
    bout << "Sysoplog Message : ";
    inputl(menu->szSysopLog, 50);
    break;
  case 'K':
    bout << "Min SL : ";
    input(szTemp1, 3);
    if (szTemp1[0]) {
      menu->nMinSL = StringToShort(szTemp1);
    }
    break;
  case 'L':
    bout << "Max SL : ";
    input(szTemp1, 3);
    if (szTemp1[0]) {
      menu->iMaxSL = StringToShort(szTemp1);
    }
    break;
  case 'M':
    bout << "Min DSL : ";
    input(szTemp1, 3);
    if (szTemp1[0]) {
      menu->nMinDSL = StringToShort(szTemp1);
    }
    break;
  case 'N':
    bout << "Max DSL : ";
    input(szTemp1, 3);
    if (szTemp1[0]) {
      menu->iMaxDSL = StringToShort(szTemp1);
    }
    break;
  case 'O':
    bout << "AR : ";
    input(szTemp1, 5);
    if (szTemp1[0]) {
      menu->uAR = StringToUnsignedShort(szTemp1);
    }
    break;
  case 'P':
    bout << "DAR : ";
    input(szTemp1, 5);
    if (szTemp1[0]) {
      menu->uDAR = StringToUnsignedShort(szTemp1);
    }
    break;
  case 'R':
    bout << "Restrictions : ";
    input(szTemp1, 5);
    if (szTemp1[0]) {
      menu->uRestrict = StringToUnsignedShort(szTemp1);
    }
    break;
  case 'S':
    menu->nSysop = !menu->nSysop;
    break;
  case 'T':
    menu->nCoSysop = !menu->nCoSysop;
    break;
  case 'U':
    if (incom && menu->szPassWord[0]) {
      bout << "Current PW: ";
      input(szPW, 20);
      if (!IsEqualsIgnoreCase(szPW, menu->szPassWord)) {
        MenuSysopLog("Unable to change PW");
        break;
      }
    }
    bout << "   New PW : ";
    input(menu->szPassWord, 20);
    break;

  case 'V':
    ++menu->nHide;
    if (menu->nHide >= MENU_HIDE_LAST) {
      menu->nHide = MENU_HIDE_NONE;
    }
    break;
  }
  return done;
}

static bool GetMenuDir(string* menuName) {
  wwiv::core::ScopeExit on_exit([=] { application()->CdHome(); });
  ListMenuDirs();
  while (!hangup) {
    bout.nl();
    bout << "|#9(Enter=Quit, ?=List) \r\n|#9Enter menuset to edit: |#0";
    input1(menuName, 8, InputMode::FILENAME, true, true);
    if (menuName->empty()) {
      return false;
    } else if (menuName->at(0) == '?') {
      ListMenuDirs();
    } 
    File dir(GetMenuDirectory(), *menuName);
    if (dir.Exists()) {
      return true;
    }
    bout << "The path " << dir << wwiv::endl << "does not exist, create it? (N) : ";
    if (!noyes()) {
      bout << "Not created\r\n";
      return false;
    }

    application()->CdHome(); // go to the wwiv dir
    File::mkdirs(dir);  // Create the new path
    if (dir.Exists()) {
      bout << "Created\r\n";
      return true;
    } else {
      bout << "Unable to create\r\n";
      return false;
    }
  }
  // The only way to get here is to hangup
  return false;
}

static bool CreateNewMenu(File& file, MenuHeader* header) {
  MenuRec menu{};
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile, File::shareDenyNone)) {
    return false;
  }
  strcpy(header->szSig, "WWIV430");
  header->nVersion = MENU_VERSION;
  header->nFlags = MENU_FLAG_MAINMENU;
  header->nHeadBytes = sizeof(MenuHeader);
  header->nBodyBytes = sizeof(MenuRec);

  // Copy header into menu and write the menu  to ensure the record is 0 100% 0 filled.
  memmove(&menu, header, sizeof(MenuHeader));
  file.Write(&menu, sizeof(MenuRec));

  return true;
}

void EditMenus() {
  wwiv::core::ScopeExit on_exit([] {application()->CdHome(); } );
  bout.cls();
  bout.litebar("WWIV Menu Editor");

  string menuDir;
  if (!GetMenuDir(&menuDir)) {
    return;
  }

  bool menu_done = false;
  while (!hangup && !menu_done) {
    bout.nl();
    bout << "|#9Current MenuSet: |#2" << menuDir << wwiv::endl; 
    string menuName;
    if (!GetMenuMenu(menuDir, menuName)) {
      return;
    }

    int nAmount = 0;
    MenuHeader header{};
    MenuRec menu{};
    File fileEditMenu(MenuInstanceData::create_menu_filename(menuDir, menuName, "mnu"));
    if (!fileEditMenu.Exists()) {
      bout << "Creating menu...\r\n";
      if (!CreateNewMenu(fileEditMenu, &header)) {
        bout << "Unable to create menu.\r\n";
        return;
      }
    } else {
      if (!fileEditMenu.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile, File::shareDenyNone)) {
        MenuSysopLog(StrCat("Unable to open menu: ", fileEditMenu));
        return;
      }
      nAmount = static_cast<int>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
      if (nAmount < 0) {
        MenuSysopLog(StrCat("Menu is corrupt: ", fileEditMenu));
        return;
      }
    }
    int nCur = 0;

    // read first record
    fileEditMenu.Seek(nCur * sizeof(MenuRec), File::seekBegin);
    fileEditMenu.Read(&menu, sizeof(MenuRec));

    bool menuitem_done = false;
    while (!hangup && !menuitem_done) {
      if (nCur == 0) {
        menuitem_done = EditHeader((MenuHeader*) &menu, fileEditMenu, menuDir, nAmount, nCur);
      } else {
        menuitem_done = EditMenuItem(&menu, fileEditMenu, nAmount, nCur);
      }
    }

  } // menu_done
}

void ReadMenuRec(File &fileEditMenu, MenuRec * menu, int nCur) {
  memset(menu, 0,  sizeof(MenuRec));
  if (fileEditMenu.Seek(nCur * sizeof(MenuRec), File::seekBegin) != -1) {
    fileEditMenu.Read(menu, sizeof(MenuRec));
  }
}

void WriteMenuRec(File &fileEditMenu, MenuRec * menu, int nCur) {
  long lRet = fileEditMenu.Seek(nCur * sizeof(MenuRec), File::seekBegin);
  if (lRet == -1) {
    return;
  }

  lRet = fileEditMenu.Write(menu, sizeof(MenuRec));
  if (lRet != sizeof(MenuRec)) {
    return;
  }
  fileEditMenu.Seek(nCur * sizeof(MenuRec), File::seekBegin);
}

bool GetMenuMenu(const string& directoryName, string& menuName) {
  ListMenuMenus(directoryName);

  while (!hangup) {
    bout.nl();
    bout << "|#9(Enter=Quit, ?=List) \r\n|#9Enter menu file to edit: |#0";
    input1(&menuName, 8, InputMode::FILENAME, true, true);

    if (menuName.empty()) {
      return false;
    } else if (menuName[0] == '?') {
      ListMenuMenus(directoryName);
    } 
    if (File::Exists(GetMenuDirectory(directoryName))) {
      return true;
    }
    bout << "File does not exist, create it? (yNq) : ";
    char x = ynq();
    if (x == 'Q') {
      return false;
    } else if (x == 'Y') {
      return true;
    } else  if (x == 'N') {
      continue;
    }
  }
  // The only way to get here is to hangup
  return false;
}

void DisplayItem(MenuRec * menu, int nCur, int nAmount) {
  bout.cls();
  const string title = StrCat("Menu Item (", nCur, " of ", nAmount, ")");
  bout.litebar(title.c_str());

  if (nCur > 0 && nCur <= nAmount) {
    bout << "|#91) Deleted        : |#2" << (menu->nFlags & MENU_FLAG_DELETED ? "Yes" : "No ") << wwiv::endl;
    bout << "|#9A) Key            : |#2" << menu->szKey << wwiv::endl;
    bout << "|#9B) Command        : |#2" << menu->szExecute << wwiv::endl;
    bout << "|#9C) Menu Text      : |#2" << menu->szMenuText << wwiv::endl;
    bout << "|#9E) Help Text      : |#2" << menu->szHelp << wwiv::endl;
    bout << "|#9F) Inst Msg       : |#2" << menu->szInstanceMessage << wwiv::endl;
    bout << "|#9G) Sysop Log      : |#2" << menu->szSysopLog << wwiv::endl;
    bout << "|#9K) Min SL         : |#2" << menu->nMinSL << wwiv::endl;
    bout << "|#9L) Max SL         : |#2" << menu->iMaxSL << wwiv::endl;
    bout << "|#9M) Min DSL        : |#2" << menu->nMinDSL << wwiv::endl;
    bout << "|#9N) Max DSL        : |#2" << menu->iMaxDSL << wwiv::endl;
    bout << "|#9O) AR             : |#2" << menu->uAR << wwiv::endl;
    bout << "|#9P) DAR            : |#2" << menu->uDAR << wwiv::endl;
    bout << "|#9R) Restrictions   : |#2" << menu->uRestrict << wwiv::endl;
    bout << "|#9S) Sysop          : |#2" << (menu->nSysop ? "Yes" : "No")  << wwiv::endl;
    bout << "|#9T) Co-Sysop       : |#2" << (menu->nCoSysop ? "Yes" : "No") << wwiv::endl;
    bout << "|#9U) Password       : |#2" << (incom ? "<Remote>" : menu->szPassWord) << wwiv::endl;
    bout << "|#9V) Hide text from : |#2" << (menu->nHide == MENU_HIDE_NONE ? "None" : menu->nHide ==
                       MENU_HIDE_PULLDOWN ? "Pulldown Menus" : menu->nHide == MENU_HIDE_REGULAR ? "Regular Menus" : menu->nHide ==
                       MENU_HIDE_BOTH ? "Both Menus" : "Out of Range") << wwiv::endl;
  }
  bout.nl(2);
  bout << "|101,A-F,K-U, Z=Add new record, [=Prev, ]=Next, Q=Quit : ";
}

void DisplayHeader(MenuHeader* pHeader, int nCur, const string& dirname) {
  wwiv::menus::MenuDescriptions descriptions(wwiv::menus::GetMenuDirectory());
  bout.cls();
  bout.litebar("Menu Header");
  if (nCur == 0) {
    bout << "|#90) Menu Description     :|#2 " << descriptions.description(dirname) << wwiv::endl;
    bout << "|#91) Deleted              :|#2 " << ((pHeader->nFlags & MENU_FLAG_DELETED) ? "Yes" : "No") << wwiv::endl;
    bout << "|#92) Main Menu            :|#2 " << ((pHeader->nFlags & MENU_FLAG_MAINMENU) ? "Yes" : "No") << wwiv::endl;;
    bout << "|#9A) What do Numbers do   :|#2 " << (pHeader->nNumbers == MENU_NUMFLAG_NOTHING ? "Nothing" :
                       pHeader->nNumbers == MENU_NUMFLAG_SUBNUMBER ? "Set sub number" : pHeader->nNumbers == MENU_NUMFLAG_DIRNUMBER ?
                       "Set dir number" : "Out of range") << wwiv::endl;
    bout << "|#9B) What type of logging :|#2 " << (pHeader->nLogging == MENU_LOGTYPE_KEY ? "Key entered" :
                       pHeader->nLogging == MENU_LOGTYPE_NONE ? "No logging" : pHeader->nLogging == MENU_LOGTYPE_COMMAND ?
                       "Command being executeed" : pHeader->nLogging == MENU_LOGTYPE_DESC ? "Desc of Command" : "Out of range") << wwiv::endl;
    bout << "|#9C) Force help to be on  :|#2 " << (pHeader->nForceHelp == MENU_HELP_DONTFORCE ? "Not forced" :
                       pHeader->nForceHelp == MENU_HELP_FORCE ? "Forced On" : pHeader->nForceHelp == MENU_HELP_ONENTRANCE ?
                       "Forced on entrance" : "Out of range") << wwiv::endl;
    bout << "|#9F) Enter Script         : |#2" << pHeader->szScript << wwiv::endl;
    bout << "|#9G) Exit Script          : |#2" << pHeader->szExitScript << wwiv::endl;
    bout << "|#9H) Min SL               : |#2" << pHeader->nMinSL << wwiv::endl;
    bout << "|#9I) Min DSL              : |#2" << pHeader->nMinDSL << wwiv::endl;
    bout << "|#9J) AR                   : |#2" << pHeader->uAR << wwiv::endl;
    bout << "|#9K) DAR                  : |#2" << pHeader->uDAR << wwiv::endl;
    bout << "|#9L) Restrictions         : |#2" << pHeader->uRestrict << wwiv::endl;
    bout << "|#9M) Sysop                : |#2" << (pHeader->nSysop ? "Yes" : "No") << wwiv::endl;
    bout << "|#9N) Co-Sysop             : |#2" << (pHeader->nCoSysop ? "Yes" : "No") << wwiv::endl;
    bout << "|#9O) Password             : |#2" << (incom ? "<Remote>" : pHeader->szPassWord) << wwiv::endl;
  }
  bout.nl(2);
  bout << "|100-2, A-O, Z=Add new Record, [=Prev, ]=Next, Q=Quit : ";
}

void ListMenuDirs() {
  const string menu_directory = GetMenuDirectory();
  wwiv::menus::MenuDescriptions descriptions(menu_directory);

  bout.nl();
  bout << "|#1Available Menus Sets" << wwiv::endl 
       << "|#9============================" << wwiv::endl;

  WFindFile fnd;
  const string search_path = StrCat(menu_directory, "*");
  bool bFound = fnd.open(search_path, 0);
  while (bFound && !hangup) {
    if (fnd.IsDirectory()) {
      const string filename = fnd.GetFileName();
      if (!starts_with(filename, ".")) {
        // Skip the . and .. dir entries.
        const string description = descriptions.description(filename);
        bout.bprintf("|#2%-8.8s |#9%-60.60s\r\n", filename.c_str(), description.c_str());
      }
    }
    bFound = fnd.next();
  }
  bout.nl();
  bout.Color(0);
}

