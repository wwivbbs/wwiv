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
#include "core/strings.h"
#include "core/wfndfile.h"

using std::string;
using wwiv::bbs::InputMode;

using namespace wwiv::menus;
using namespace wwiv::strings;


bool GetMenuDir(string& menuDir);
bool GetMenuMenu(const string& pszDirectoryName, string& menuName);
void ReadMenuRec(File &fileEditMenu, MenuRec * Menu, int nCur);
void WriteMenuRec(File &fileEditMenu, MenuRec * Menu, int nCur);
void DisplayItem(MenuRec * Menu, int nCur, int nAmount);
void DisplayHeader(MenuHeader* Header, int nCur, const string& dirname);

static void ListMenuMenus(const char *pszDirectoryName) {
  string path = GetMenuDirectory(pszDirectoryName) + "*.mnu";

  bout.nl();
  bout << "|#1Available Menus\r\n";
  bout << "|#7===============|#0\r\n";

  WFindFile fnd;
  bool bFound = fnd.open(path, 0);
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
    bout << "Is menu deleted? (N) ";
    if (noyes()) {
      header->nFlags |= MENU_FLAG_DELETED;
    } else {
      header->nFlags &= ~MENU_FLAG_DELETED;
    }
    break;
  case '2':
    bout << "Is menu a main menu? (Y) ";
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

void EditMenus() {
  char szTemp1[21];
  char szPW[21];
  int nAmount = 0;
  MenuHeader header{};
  MenuRec Menu{};
  bout.cls();
  bout.litebar("WWIV Menu Editor");

  string menuDir;
  if (!GetMenuDir(menuDir)) {
    return;
  }

  string menuName;
  if (!GetMenuMenu(menuDir, menuName)) {
    return;
  }

  File fileEditMenu(MenuInstanceData::create_menu_filename(menuDir, menuName, "mnu"));
  if (!fileEditMenu.Exists()) {
    bout << "Creating menu...\r\n";
    if (!fileEditMenu.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile, File::shareDenyNone)) {
      bout << "Unable to create menu.\r\n";
      return;
    }
    strcpy(header.szSig, "WWIV430");
    header.nVersion = MENU_VERSION;
    header.nFlags = MENU_FLAG_MAINMENU;
    header.nHeadBytes = sizeof(MenuHeader);
    header.nBodyBytes = sizeof(MenuRec);

    // Copy header into menu and write the menu  to ensure the record is 0 100% 0 filled.
    memmove(&Menu, &header, sizeof(MenuHeader));
    fileEditMenu.Write(&Menu, sizeof(MenuRec));
    nAmount = 0;
  } else {
    if (!fileEditMenu.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile, File::shareDenyNone)) {
      MenuSysopLog("Unable to open menu.");
      MenuSysopLog(fileEditMenu.full_pathname());
      return;
    }
    nAmount = static_cast<uint16_t>(fileEditMenu.GetLength() / sizeof(MenuRec));
    --nAmount;
    if (nAmount < 0) {
      MenuSysopLog("Menu is corrupt.");
      MenuSysopLog(fileEditMenu.full_pathname());
      return;
    }
  }
  int nCur = 0;

  // read first record
  fileEditMenu.Seek(nCur * sizeof(MenuRec), File::seekBegin);
  fileEditMenu.Read(&Menu, sizeof(MenuRec));

  bool done = false;
  while (!hangup && !done) {
    if (nCur == 0) {

    done = EditHeader((MenuHeader*) &Menu, fileEditMenu, menuDir, nAmount, nCur);

    } else {
      DisplayItem(&Menu, nCur, nAmount);
      char chKey = onek("Q[]Z1ABCDEFGKLMNOPRSTUVWX");

      switch (chKey) {
      case 'Q':
        WriteMenuRec(fileEditMenu, &Menu, nCur);
        done = true;
        break;

      case '[':
        WriteMenuRec(fileEditMenu, &Menu, nCur);
        nAmount = static_cast<uint16_t>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
        --nCur;
        if (nCur < 0) {
          nCur = nAmount;
        }
        ReadMenuRec(fileEditMenu, &Menu, nCur);
        break;
      case ']':
        WriteMenuRec(fileEditMenu, &Menu, nCur);
        nAmount = static_cast<uint16_t>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
        ++nCur;
        if (nCur > nAmount) {
          nCur = 0;
        }
        ReadMenuRec(fileEditMenu, &Menu, nCur);
        break;
      case 'Z':
        WriteMenuRec(fileEditMenu, &Menu, nCur);
        memset(&Menu, 0, sizeof(MenuRec));
        nAmount = static_cast<uint16_t>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
        nCur = nAmount + 1;
        memset(&Menu, 0, sizeof(MenuRec));
        Menu.iMaxSL = 255;
        Menu.iMaxDSL = 255;
        WriteMenuRec(fileEditMenu, &Menu, nCur);
        nAmount = static_cast<uint16_t>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
        ReadMenuRec(fileEditMenu, &Menu, nCur);
        break;
      case '1':
        bout << "Is record deleted? (N) ";
        if (yesno()) {
          Menu.nFlags |= MENU_FLAG_DELETED;
        } else {
          Menu.nFlags &= ~MENU_FLAG_DELETED;
        }
        break;
      case 'A':
        bout << "Key to cause execution : ";
        input(Menu.szKey, MENU_MAX_KEYS);
        if (!(Menu.szSysopLog[0])) {
          strcpy(Menu.szSysopLog, Menu.szKey);
        }
        break;
      case 'B':
        bout << "Command to execute : ";
        inputl(Menu.szExecute, 100);
        if (!(Menu.szMenuText[0])) {
          strcpy(Menu.szMenuText, Menu.szExecute);
        }
        break;
      case 'C':
        bout << "Menu Text : ";
        inputl(Menu.szMenuText, 40);
        break;
      case 'E':
        bout << "Help Text : ";
        inputl(Menu.szHelp, 80);
        break;
      case 'F':
        bout << "Instance Message : ";
        inputl(Menu.szInstanceMessage, 80);
        break;
      case 'G':
        bout << "Sysoplog Message : ";
        inputl(Menu.szSysopLog, 50);
        break;
      case 'K':
        bout << "Min SL : ";
        input(szTemp1, 3);
        if (szTemp1[0]) {
          Menu.nMinSL = StringToShort(szTemp1);
        }
        break;
      case 'L':
        bout << "Max SL : ";
        input(szTemp1, 3);
        if (szTemp1[0]) {
          Menu.iMaxSL = StringToShort(szTemp1);
        }
        break;
      case 'M':
        bout << "Min DSL : ";
        input(szTemp1, 3);
        if (szTemp1[0]) {
          Menu.nMinDSL = StringToShort(szTemp1);
        }
        break;
      case 'N':
        bout << "Max DSL : ";
        input(szTemp1, 3);
        if (szTemp1[0]) {
          Menu.iMaxDSL = StringToShort(szTemp1);
        }
        break;
      case 'O':
        bout << "AR : ";
        input(szTemp1, 5);
        if (szTemp1[0]) {
          Menu.uAR = StringToUnsignedShort(szTemp1);
        }
        break;
      case 'P':
        bout << "DAR : ";
        input(szTemp1, 5);
        if (szTemp1[0]) {
          Menu.uDAR = StringToUnsignedShort(szTemp1);
        }
        break;
      case 'R':
        bout << "Restrictions : ";
        input(szTemp1, 5);
        if (szTemp1[0]) {
          Menu.uRestrict = StringToUnsignedShort(szTemp1);
        }
        break;
      case 'S':
        Menu.nSysop = !Menu.nSysop;
        break;
      case 'T':
        Menu.nCoSysop = !Menu.nCoSysop;
        break;
      case 'U':
        if (incom && Menu.szPassWord[0]) {
          bout << "Current PW: ";
          input(szPW, 20);
          if (!IsEqualsIgnoreCase(szPW, Menu.szPassWord)) {
            MenuSysopLog("Unable to change PW");
            break;
          }
        }
        bout << "   New PW : ";
        input(Menu.szPassWord, 20);
        break;

      case 'V':
        ++Menu.nHide;
        if (Menu.nHide >= MENU_HIDE_LAST) {
          Menu.nHide = MENU_HIDE_NONE;
        }
        break;
      case 'X':
        bout << "Filename for detailed help on item : ";
        input(Menu.szExtendedHelp, 12);
        break;
      }
    }
  }
  application()->CdHome(); // make sure we are in the wwiv dir
  fileEditMenu.Close();
}

void ReadMenuRec(File &fileEditMenu, MenuRec * Menu, int nCur) {
  memset(Menu, 0,  sizeof(MenuRec));
  if (fileEditMenu.Seek(nCur * sizeof(MenuRec), File::seekBegin) != -1) {
    fileEditMenu.Read(Menu, sizeof(MenuRec));
  }
}

void WriteMenuRec(File &fileEditMenu, MenuRec * Menu, int nCur) {
  long lRet = fileEditMenu.Seek(nCur * sizeof(MenuRec), File::seekBegin);
  if (lRet == -1) {
    return;
  }

  lRet = fileEditMenu.Write(Menu, sizeof(MenuRec));
  if (lRet != sizeof(MenuRec)) {
    return;
  }
  fileEditMenu.Seek(nCur * sizeof(MenuRec), File::seekBegin);
}

bool GetMenuDir(string& menuName) {
  ListMenuDirs();

  while (!hangup) {
    bout.nl();
    bout << "|#9(Enter=Quit, ?=List) Enter menuset to edit: |#0";

    input1(&menuName, 8, InputMode::FILENAME, true, true);

    if (menuName.empty()) {
      return false;
    } else if (menuName[0] == '?') {
      ListMenuDirs();
    } else {
      File dir(GetMenuDirectory(), menuName);
      if (!dir.Exists()) {
        bout << "The path " << dir.full_pathname() << wwiv::endl <<
                           "does not exist, create it? (N) : ";
        if (noyes()) {
          application()->CdHome(); // go to the wwiv dir
          File::mkdirs(dir);  // Create the new path
          if (dir.Exists()) {
            application()->CdHome();
            bout << "Created\r\n";
            return true;
          } else {
            application()->CdHome();
            bout << "Unable to create\r\n";
            return false;
          }
        } else {
          bout << "Not created\r\n";
          return false;
        }
      }
      application()->CdHome();
      return true;
    }
  }
  // The only way to get here is to hangup
  return false;
}

bool GetMenuMenu(const string& directoryName, string& menuName) {
  ListMenuMenus(directoryName.c_str());

  while (!hangup) {
    bout.nl();
    bout << "|#9(Enter=Quit, ?=List) Enter menu file to edit: |#0";
    input1(&menuName, 8, InputMode::FILENAME, true, true);

    if (menuName.empty()) {
      return false;
    } else if (menuName[0] == '?') {
      ListMenuMenus(directoryName.c_str());
    } else {
      if (!File::Exists(GetMenuDirectory(directoryName))) {
        bout << "File does not exist, create it? (yNq) : ";
        char x = ynq();

        if (x == 'Q') {
          return false;
        }
        if (x == 'Y') {
          return true;
        }
        if (x == 'N') {
          continue;
        }
      } else {
        return true;
      }
    }
  }
  // The only way to get here is to hangup
  return false;
}

void DisplayItem(MenuRec * Menu, int nCur, int nAmount) {
  bout.cls();
  bout << "|02(|#9" << nCur << "|02/|#9" << nAmount << "|02)" << wwiv::endl;

  if (nCur > 0 && nCur <= nAmount) {
    bout << "|#91) Deleted        : |#2" << (Menu->nFlags & MENU_FLAG_DELETED ? "Yes" : "No ") << wwiv::endl;
    bout << "|#9A) Key            : |#2" << Menu->szKey << wwiv::endl;
    bout << "|#9B) Command        : |#2" << Menu->szExecute << wwiv::endl;
    bout << "|#9C) Menu Text      : |#2" << Menu->szMenuText << wwiv::endl;
    bout << "|#9E) Help Text      : |#2" << Menu->szHelp << wwiv::endl;
    bout << "|#9F) Inst Msg       : |#2" << Menu->szInstanceMessage << wwiv::endl;
    bout << "|#9G) Sysop Log      : |#2" << Menu->szSysopLog << wwiv::endl;
    bout << "|#9K) Min SL         : |#2" << Menu->nMinSL << wwiv::endl;
    bout << "|#9L) Max SL         : |#2" << Menu->iMaxSL << wwiv::endl;
    bout << "|#9M) Min DSL        : |#2" << Menu->nMinDSL << wwiv::endl;
    bout << "|#9N) Max DSL        : |#2" << Menu->iMaxDSL << wwiv::endl;
    bout << "|#9O) AR             : |#2" << Menu->uAR << wwiv::endl;
    bout << "|#9P) DAR            : |#2" << Menu->uDAR << wwiv::endl;
    bout << "|#9R) Restrictions   : |#2" << Menu->uRestrict << wwiv::endl;
    bout << "|#9S) Sysop          : |#2" << (Menu->nSysop ? "Yes" : "No")  << wwiv::endl;
    bout << "|#9T) Co-Sysop       : |#2" << (Menu->nCoSysop ? "Yes" : "No") << wwiv::endl;
    bout << "|#9U) Password       : |#2" << (incom ? "<Remote>" : Menu->szPassWord) << wwiv::endl;
    bout << "|#9V) Hide text from : |#2" << (Menu->nHide == MENU_HIDE_NONE ? "None" : Menu->nHide ==
                       MENU_HIDE_PULLDOWN ? "Pulldown Menus" : Menu->nHide == MENU_HIDE_REGULAR ? "Regular Menus" : Menu->nHide ==
                       MENU_HIDE_BOTH ? "Both Menus" : "Out of Range") << wwiv::endl;
    bout << "|#9X) Extended Help  : |#2%s" << Menu->szExtendedHelp << wwiv::endl;
  }
  bout.nl(2);
  bout << "|101,A-F,K-U, Z=Add new record, [=Prev, ]=Next, Q=Quit : ";
}

void DisplayHeader(MenuHeader* pHeader, int nCur, const string& dirname) {
  wwiv::menus::MenuDescriptions descriptions(wwiv::menus::GetMenuDirectory());
  bout.cls();
  bout << "|#9(Menu Header)" << wwiv::endl;
  if (nCur == 0) {
    bout << "|#90) Menu Description     |#2: " << descriptions.description(dirname) << wwiv::endl;
    bout << "|#91) Deleted              |#2: " << ((pHeader->nFlags & MENU_FLAG_DELETED) ? "Yes" : "No") << wwiv::endl;
    bout << "|#92) Main Menu            |#2: " << ((pHeader->nFlags & MENU_FLAG_MAINMENU) ? "Yes" : "No") << wwiv::endl;;
    bout << "|#9A) What do Numbers do   |#2: " << (pHeader->nNumbers == MENU_NUMFLAG_NOTHING ? "Nothing" :
                       pHeader->nNumbers == MENU_NUMFLAG_SUBNUMBER ? "Set sub number" : pHeader->nNumbers == MENU_NUMFLAG_DIRNUMBER ?
                       "Set dir number" : "Out of range") << wwiv::endl;
    bout << "|#9B) What type of logging |#2: " << (pHeader->nLogging == MENU_LOGTYPE_KEY ? "Key entered" :
                       pHeader->nLogging == MENU_LOGTYPE_NONE ? "No logging" : pHeader->nLogging == MENU_LOGTYPE_COMMAND ?
                       "Command being executeed" : pHeader->nLogging == MENU_LOGTYPE_DESC ? "Desc of Command" : "Out of range") << wwiv::endl;
    bout << "|#9C) Force help to be on  |#2: " << (pHeader->nForceHelp == MENU_HELP_DONTFORCE ? "Not forced" :
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
  bout << "|#1Available Menus Sets\r\n";
  bout << "|#7============================\r\n";

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

