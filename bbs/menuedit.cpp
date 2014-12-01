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

#include "wwiv.h"
#include "bbs/input.h"
#include "bbs/menu.h"
#include "core/strings.h"
#include "core/wfndfile.h"

using std::string;
using wwiv::bbs::InputMode;
using wwiv::strings::IsEqualsIgnoreCase;
using wwiv::strings::StringToChar;
using wwiv::strings::StringToShort;
using wwiv::strings::StringToUnsignedShort;

bool GetMenuDir(string& menuDir);
bool GetMenuMenu(const string& pszDirectoryName, string& menuName);
void ReIndexMenu(File &fileEditMenu, const char *pszDirectoryName, const char *pszMenuName);
void ReadMenuRec(File &fileEditMenu, MenuRec * Menu, int nCur);
void WriteMenuRec(File &fileEditMenu, MenuRec * Menu, int nCur);
void DisplayItem(MenuRec * Menu, int nCur, int nAmount);
void DisplayHeader(MenuHeader * Header, int nCur, int nAmount, const char *pszDirectoryName);
void EditPulldownColors(MenuHeader * H);
void ListMenuMenus(const char *pszDirectoryName);

#ifndef _WIN32
#ifndef LOBYTE
#define LOBYTE(w)           ((uint8_t)(w))
#endif  // LOBYTE

#ifndef HIBYTE
#define HIBYTE(w)           ((uint8_t)(((uint16_t)(w) >> 8) & 0xFF))
#endif  // HIBYTE
#endif

void EditMenus() {
  char szTemp1[21];
  char szPW[21];
  char szDesc[81];
  int nAmount = 0;
  MenuHeader header;
  MenuRec Menu;
  bout.cls();
  bout << "|#2WWIV Menu Editor|#0\r\n";

  string menuDir;
  if (!GetMenuDir(menuDir)) {
    return;
  }

  string menuName;
  if (!GetMenuMenu(menuDir, menuName)) {
    return;
  }

  File fileEditMenu(GetMenuDirectory(menuDir, menuName, "mnu"));
  if (!fileEditMenu.Exists()) {
    bout << "Creating menu...\r\n";
    if (!fileEditMenu.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile, File::shareDenyNone)) {
      bout << "Unable to create menu.\r\n";
      return;
    }
    memset(&header, 0, sizeof(MenuHeader));
    memset(&Menu, 0, sizeof(MenuRec));
    strcpy(header.szSig, "WWIV430");

    header.nVersion = MENU_VERSION;
    header.nFlags = MENU_FLAG_MAINMENU;

    header.nHeadBytes = sizeof(MenuHeader);
    header.nBodyBytes = sizeof(MenuRec);

    /* ---------------------------------------- */
    /* Copy header into menu and write the menu */
    /* to ensure the record is 0 100% 0 filled  */
    /* ---------------------------------------- */
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
      DisplayHeader((MenuHeader *)(&Menu), nCur, nAmount, menuDir.c_str());
      char chKey = onek("Q[]Z012ABCDEFGHIJKLMNOP");
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
      case '0':
        OpenMenuDescriptions();
        GetMenuDescription(menuDir.c_str(), szDesc);

        bout << "|#5New desc     : ";
        bout.Color(0);
        inputl(szDesc, 60);
        if (szDesc[0]) {
          SetMenuDescription(menuDir.c_str(), szDesc);
        }
        CloseMenuDescriptions();
        break;
      case '1':
        bout << "Is menu deleted? (N) ";
        if (yesno()) {
          ((MenuHeader *)(&Menu))->nFlags |= MENU_FLAG_DELETED;
        } else {
          ((MenuHeader *)(&Menu))->nFlags &= ~MENU_FLAG_DELETED;
        }
        break;
      case '2':
        bout << "Is menu a main menu? (Y) ";
        if (noyes()) {
          ((MenuHeader *)(&Menu))->nFlags |= MENU_FLAG_MAINMENU;
        } else {
          ((MenuHeader *)(&Menu))->nFlags &= ~MENU_FLAG_MAINMENU;
        }
        break;
      case 'A':
        ((MenuHeader *)(&Menu))->nNumbers++;
        if (((MenuHeader *)(&Menu))->nNumbers == MENU_NUMFLAG_LAST) {
          ((MenuHeader *)(&Menu))->nNumbers = 0;
        }
        break;
      case 'B':
        ((MenuHeader *)(&Menu))->nLogging++;
        if (((MenuHeader *)(&Menu))->nLogging == MENU_LOGTYPE_LAST) {
          ((MenuHeader *)(&Menu))->nLogging = 0;
        }
        break;
      case 'C':
        ((MenuHeader *)(&Menu))->nForceHelp++;
        if (((MenuHeader *)(&Menu))->nForceHelp == MENU_HELP_LAST) {
          ((MenuHeader *)(&Menu))->nForceHelp = 0;
        }
        break;
      case 'D':
        ((MenuHeader *)(&Menu))->nAllowedMenu++;
        if (((MenuHeader *)(&Menu))->nAllowedMenu == MENU_ALLOWED_LAST) {
          ((MenuHeader *)(&Menu))->nAllowedMenu = 0;
        }
        break;
      case 'E':
        bout << "Pulldown menu title : ";
        inputl(((MenuHeader *)(&Menu))->szMenuTitle, 20);
        break;
      case 'F':
        bout << "Command to execute : ";
        inputl(((MenuHeader *)(&Menu))->szScript, 100);
        break;
      case 'G':
        bout << "Script for when menu ends : ";
        inputl(((MenuHeader *)(&Menu))->szExitScript, 100);
        break;
      case 'H':
        bout << "Min SL : ";
        input(szTemp1, 3);
        if (szTemp1[0]) {
          ((MenuHeader *)(&Menu))->nMinSL = StringToShort(szTemp1);
        }
        break;
      case 'I':
        bout << "Min DSL : ";
        input(szTemp1, 3);
        if (szTemp1[0]) {
          ((MenuHeader *)(&Menu))->nMinDSL = StringToShort(szTemp1);
        }
        break;
      case 'J':
        bout << "AR : ";
        input(szTemp1, 5);
        if (szTemp1[0]) {
          ((MenuHeader *)(&Menu))->uAR = StringToUnsignedShort(szTemp1);
        }
        break;
      case 'K':
        bout << "DAR : ";
        input(szTemp1, 5);
        if (szTemp1[0]) {
          ((MenuHeader *)(&Menu))->uDAR = StringToUnsignedShort(szTemp1);
        }
        break;
      case 'L':
        bout << "Restrictions : ";
        input(szTemp1, 5);
        if (szTemp1[0]) {
          ((MenuHeader *)(&Menu))->uRestrict = StringToUnsignedShort(szTemp1);
        }
        break;
      case 'M':
        ((MenuHeader *)(&Menu))->nSysop = !((MenuHeader *)(&Menu))->nSysop;
        break;
      case 'N':
        ((MenuHeader *)(&Menu))->nCoSysop = !((MenuHeader *)(&Menu))->nCoSysop;
        break;
      case 'O':
        if (incom && ((MenuHeader *)(&Menu))->szPassWord[0]) {
          bout << "Current PW: ";
          input(szPW, 20);
          if (!IsEqualsIgnoreCase(szPW, ((MenuHeader *)(&Menu))->szPassWord)) {
            MenuSysopLog("Unable to change PW");
            break;
          }
        }
        bout << "   New PW : ";
        input(((MenuHeader *)(&Menu))->szPassWord, 20);
        break;
      case 'P':
        EditPulldownColors(((MenuHeader *)(&Menu)));
        break;
      }
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
        if (!(Menu.szPDText[0])) {
          strcpy(Menu.szPDText, Menu.szExecute);
        }
        break;
      case 'C':
        bout << "Menu Text : ";
        inputl(Menu.szMenuText, 40);
        if (!(Menu.szPDText[0])) {
          strcpy(Menu.szPDText, Menu.szMenuText);
        }
        break;
      case 'D':
        bout << "Pulldown Menu Text : ";
        inputl(Menu.szPDText, 40);
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
      case 'W':
        bout << "Clear screen before command is run? (Y) ";
        if (noyes()) {
          Menu.nPDFlags &= ~PDFLAGS_NOCLEAR;
        } else {
          Menu.nPDFlags |= PDFLAGS_NOCLEAR;
        }

        bout << "Pause screen after command is run? (Y) ";
        if (noyes()) {
          Menu.nPDFlags &= ~PDFLAGS_NOPAUSEAFTER;
        } else {
          Menu.nPDFlags |= PDFLAGS_NOPAUSEAFTER;
        }

        bout << "Restore screen after command is run? (Y) ";
        if (noyes()) {
          Menu.nPDFlags &= ~PDFLAGS_NORESTORE;
        } else {
          Menu.nPDFlags |= PDFLAGS_NORESTORE;
        }
        break;
      case 'X':
        bout << "Filename for detailed help on item : ";
        input(Menu.szExtendedHelp, 12);
        break;
      }
    }
  }
  GetApplication()->CdHome(); // make sure we are in the wwiv dir

  ReIndexMenu(fileEditMenu, menuDir.c_str(), menuName.c_str());
  fileEditMenu.Close();
}


void ReIndexMenu(File &fileEditMenu, const char *pszDirectoryName, const char *pszMenuName) {
  bout << "Indexing Menu...\r\n";

  File fileIdx(GetMenuDirectory(pszDirectoryName, pszMenuName, "idx"));
  if (!fileIdx.Open(File::modeBinary | File::modeCreateFile | File::modeTruncate | File::modeReadWrite,
                    File::shareDenyWrite)) {
    bout << "Unable to reindex\r\n";
    pausescr();
    return;
  }
  int nAmount = static_cast<uint16_t>(fileEditMenu.GetLength() / sizeof(MenuRec));

  for (int nRec = 1; nRec < nAmount; nRec++) {
    MenuRec menu;
    fileEditMenu.Seek(nRec * sizeof(MenuRec), File::seekBegin);
    fileEditMenu.Read(&menu, sizeof(MenuRec));

    MenuRecIndex menuIndex;
    memset(&menuIndex, 0, sizeof(MenuRecIndex));
    menuIndex.nRec = static_cast<short>(nRec);
    menuIndex.nFlags = menu.nFlags;
    strcpy(menuIndex.szKey, menu.szKey);

    fileIdx.Seek((nRec - 1) * sizeof(MenuRecIndex), File::seekBegin);
    fileIdx.Write(&menuIndex, sizeof(MenuRecIndex));
  }

  fileIdx.Close();
}


void ReadMenuRec(File &fileEditMenu, MenuRec * Menu, int nCur) {
  if (fileEditMenu.Seek(nCur * sizeof(MenuRec), File::seekBegin) != -1) {
    fileEditMenu.Read(Menu, sizeof(MenuRec));
  }
}


void WriteMenuRec(File &fileEditMenu, MenuRec * Menu, int nCur) {
  // %%TODO Add in locking (_locking) support via WIN32 file api's

  long lRet = fileEditMenu.Seek(nCur * sizeof(MenuRec), File::seekBegin);
  if (lRet == -1) {
    return;
  }

  /*
  if (lock(iEditMenu, nCur * sizeof(MenuRec), sizeof(MenuRec)) != 0) {
  bout << "Unable to lock record for write\r\n";
  pausescr();
  return;
  }
  */

  lRet = fileEditMenu.Write(Menu, sizeof(MenuRec));
  if (lRet != sizeof(MenuRec)) {
    return;
  }

  fileEditMenu.Seek(nCur * sizeof(MenuRec), File::seekBegin);

  /*
  unlock(iEditMenu, nCur * sizeof(MenuRec), sizeof(MenuRec));
  */
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
          GetApplication()->CdHome(); // go to the wwiv dir
          File::mkdirs(dir);  // Create the new path
          if (dir.Exists()) {
            GetApplication()->CdHome();
            bout << "Created\r\n";
            return true;
          } else {
            GetApplication()->CdHome();
            bout << "Unable to create\r\n";
            return false;
          }
        } else {
          bout << "Not created\r\n";
          return false;
        }
      }
      GetApplication()->CdHome();
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
    input(&menuName, 8);

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
    bout << "|#9D) PD Menu Text   : |#2" << Menu->szPDText << wwiv::endl;
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
    bout.bprintf("|#9W) Pulldown flags : |#5%-20.20s |#1%-18.18s |#6%-20.20s",
                                      Menu->nPDFlags & PDFLAGS_NOCLEAR ? "No Clear before run" : "Clear before run",
                                      Menu->nPDFlags & PDFLAGS_NOPAUSEAFTER ? "No Pause after run" : "Pause after run",
                                      Menu->nPDFlags & PDFLAGS_NORESTORE ? "No Restore after run" : "Restore after run");
    bout << "|#9X) Extended Help  : |#2%s" << Menu->szExtendedHelp << wwiv::endl;

  }
  bout.nl(2);
  bout <<
                     "|041|#0,|04A|#0-|04F|#0,|04K|#0-|04U|#0, |04Z|#0=Add new record, |04[|#0=Prev, |04]|#0=Next, |04Q|#0=Quit : ";
}


void DisplayHeader(MenuHeader * pHeader, int nCur, int nAmount, const char *pszDirectoryName) {
  bout.cls();

  OpenMenuDescriptions();

  bout << "(" << nCur << "/" << nAmount << ")" << wwiv::endl;

  if (nCur == 0) {
    bout << "   Menu Version         : " <<
                       static_cast<int>(HIBYTE(pHeader->nVersion)) <<
                       static_cast<int>(LOBYTE(pHeader->nVersion)) << wwiv::endl;
    char szDesc[101];
    bout << "0) Menu Description     : " << GetMenuDescription(string(pszDirectoryName),
                       szDesc) << wwiv::endl;
    bout << "1) Deleted              : " << ((pHeader->nFlags & MENU_FLAG_DELETED) ? "Yes" : "No") <<
                       wwiv::endl;
    bout << "2) Main Menu            : " << ((pHeader->nFlags & MENU_FLAG_MAINMENU) ? "Yes" : "No") <<
                       wwiv::endl;;
    bout << "A) What do Numbers do   : " << (pHeader->nNumbers == MENU_NUMFLAG_NOTHING ? "Nothing" :
                       pHeader->nNumbers == MENU_NUMFLAG_SUBNUMBER ? "Set sub number" : pHeader->nNumbers == MENU_NUMFLAG_DIRNUMBER ?
                       "Set dir number" : "Out of range") << wwiv::endl;
    bout << "B) What type of logging : " << (pHeader->nLogging == MENU_LOGTYPE_KEY ? "Key entered" :
                       pHeader->nLogging == MENU_LOGTYPE_NONE ? "No logging" : pHeader->nLogging == MENU_LOGTYPE_COMMAND ?
                       "Command being executeed" : pHeader->nLogging == MENU_LOGTYPE_DESC ? "Desc of Command" : "Out of range") << wwiv::endl;
    bout << "C) Force help to be on  : " << (pHeader->nForceHelp == MENU_HELP_DONTFORCE ? "Not forced" :
                       pHeader->nForceHelp == MENU_HELP_FORCE ? "Forced On" : pHeader->nForceHelp == MENU_HELP_ONENTRANCE ?
                       "Forced on entrance" : "Out of range") << wwiv::endl;
    bout << "D) Allowed menu type    : " << (pHeader->nAllowedMenu == MENU_ALLOWED_BOTH ? "Regular/Pulldown" :
                       pHeader->nAllowedMenu == MENU_ALLOWED_PULLDOWN ? "Pulldown" : pHeader->nAllowedMenu == MENU_ALLOWED_REGULAR ?
                       "Regular" : "Out of range") << wwiv::endl;
    bout << "E) Pulldown menu title  : " << pHeader->szMenuTitle << wwiv::endl;
    bout << "F) Enter Script         : " << pHeader->szScript << wwiv::endl;
    bout << "G) Exit Script          : " << pHeader->szExitScript << wwiv::endl;
    bout << "H) Min SL               : " << static_cast<unsigned int>(pHeader->nMinSL) << wwiv::endl;
    bout << "I) Min DSL              : " << static_cast<unsigned int>(pHeader->nMinDSL) << wwiv::endl;
    bout << "J) AR                   : " << static_cast<unsigned int>(pHeader->uAR) << wwiv::endl;
    bout << "K) DAR                  : " << static_cast<unsigned int>(pHeader->uDAR) << wwiv::endl;
    bout << "L) Restrictions         : " << static_cast<unsigned int>(pHeader->uRestrict) << wwiv::endl;
    bout << "M) Sysop                : " << (pHeader->nSysop ? "Yes" : "No") << wwiv::endl;
    bout << "N) Co-Sysop             : " << (pHeader->nCoSysop ? "Yes" : "No") << wwiv::endl;
    bout << "O) Password             : " << (incom ? "<Remote>" : pHeader->szPassWord) << wwiv::endl;
    bout << "P) Change pulldown colors" << wwiv::endl;
  }
  bout.nl(2);
  bout << "0-2, A-O, Z=Add new Record, [=Prev, ]=Next, Q=Quit : ";

  CloseMenuDescriptions();
}


void EditPulldownColors(MenuHeader * pMenuHeader) {
  char szTemp[15];

  bool done = false;
  while (!done && !hangup) {
    bout.cls();
    bout.Color(0);

    bout.bprintf("%-35.35s", "A) Title color");
    if (pMenuHeader->nTitleColor) {
      bout.SystemColor(pMenuHeader->nTitleColor);
    }
    bout << static_cast<int>(pMenuHeader->nTitleColor) << wwiv::endl;
    bout.Color(0);
    bout.bprintf("%-35.35s", "B) Main border color");
    if (pMenuHeader->nMainBorderColor) {
      bout.SystemColor(pMenuHeader->nMainBorderColor);
    }
    bout << static_cast<int>(pMenuHeader->nMainBorderColor) << wwiv::endl;
    bout.Color(0);
    bout.bprintf("%-35.35s", "C) Main box color");
    if (pMenuHeader->nMainBoxColor) {
      bout.SystemColor(pMenuHeader->nMainBoxColor);
    }
    bout << static_cast<int>(pMenuHeader->nMainBoxColor) << wwiv::endl;
    bout.Color(0);
    bout.bprintf("%-35.35s", "D) Main text color");
    if (pMenuHeader->nMainTextColor) {
      bout.SystemColor(pMenuHeader->nMainTextColor);
    }
    bout << static_cast<int>(pMenuHeader->nMainTextColor) << wwiv::endl;
    bout.Color(0);
    bout.bprintf("%-35.35s", "E) Main text highlight color");
    if (pMenuHeader->nMainTextHLColor) {
      bout.SystemColor(pMenuHeader->nMainTextHLColor);
    }
    bout << static_cast<int>(pMenuHeader->nMainTextHLColor) << wwiv::endl;
    bout.Color(0);
    bout.bprintf("%-35.35s", "F) Main selected color");
    if (pMenuHeader->nMainSelectedColor) {
      bout.SystemColor(pMenuHeader->nMainSelectedColor);
    }
    bout << static_cast<int>(pMenuHeader->nMainSelectedColor) << wwiv::endl;
    bout.Color(0);
    bout.bprintf("%-35.35s", "G) Main selected hightlight color");
    if (pMenuHeader->nMainSelectedHLColor) {
      bout.SystemColor(pMenuHeader->nMainSelectedHLColor);
    }
    bout << static_cast<int>(pMenuHeader->nMainSelectedHLColor) << wwiv::endl;
    bout.Color(0);

    bout.bprintf("%-35.35s", "K) Item border color");
    if (pMenuHeader->nItemBorderColor) {
      bout.SystemColor(pMenuHeader->nItemBorderColor);
    }
    bout << static_cast<int>(pMenuHeader->nItemBorderColor) << wwiv::endl;
    bout.Color(0);
    bout.bprintf("%-35.35s", "L) Item box color");
    if (pMenuHeader->nItemBoxColor) {
      bout.SystemColor(pMenuHeader->nItemBoxColor);
    }
    bout << static_cast<int>(pMenuHeader->nItemBoxColor) << wwiv::endl;
    bout.Color(0);
    bout.bprintf("%-35.35s", "M) Item text color");
    if (pMenuHeader->nItemTextColor) {
      bout.SystemColor(pMenuHeader->nItemTextColor);
    }
    bout << static_cast<int>(pMenuHeader->nItemTextColor) << wwiv::endl;
    bout.Color(0);
    bout.bprintf("%-35.35s", "N) Item text highlight color");
    if (pMenuHeader->nItemTextHLColor) {
      bout.SystemColor(pMenuHeader->nItemTextHLColor);
    }
    bout << static_cast<int>(pMenuHeader->nItemTextHLColor) << wwiv::endl;
    bout.Color(0);
    bout.bprintf("%-35.35s", "O) Item selected color");
    if (pMenuHeader->nItemSelectedColor) {
      bout.SystemColor(pMenuHeader->nItemSelectedColor);
    }
    bout << static_cast<int>(pMenuHeader->nItemSelectedColor) << wwiv::endl;
    bout.Color(0);
    bout.bprintf("%-35.35s", "P) Item selected hightlight color");
    if (pMenuHeader->nItemSelectedHLColor) {
      bout.SystemColor(pMenuHeader->nItemSelectedHLColor);
    }
    bout << static_cast<int>(pMenuHeader->nItemSelectedHLColor) << wwiv::endl;
    bout.Color(0);

    bout.nl(2);
    bout << "A-G,K-P, Q=Quit : ";
    char chKey = onek("QABCDEFGKLMNOP");

    if (chKey != 'Q') {
      ListAllColors();
      bout.nl();
      bout << "Select a color : ";
    }
    switch (chKey) {
    case 'A':
      input(szTemp, 3);
      if (szTemp[0]) {
        pMenuHeader->nTitleColor = StringToChar(szTemp);
      }
      break;
    case 'B':
      input(szTemp, 3);
      if (szTemp[0]) {
        pMenuHeader->nMainBorderColor = StringToChar(szTemp);
      }
      break;
    case 'C':
      input(szTemp, 3);
      if (szTemp[0]) {
        pMenuHeader->nMainBoxColor = StringToChar(szTemp);
      }
      break;
    case 'D':
      input(szTemp, 3);
      if (szTemp[0]) {
        pMenuHeader->nMainTextColor = StringToChar(szTemp);
      }
      break;
    case 'E':
      input(szTemp, 3);
      if (szTemp[0]) {
        pMenuHeader->nMainTextHLColor = StringToChar(szTemp);
      }
      break;
    case 'F':
      input(szTemp, 3);
      if (szTemp[0]) {
        pMenuHeader->nMainSelectedColor = StringToChar(szTemp);
      }
      break;
    case 'G':
      input(szTemp, 3);
      if (szTemp[0]) {
        pMenuHeader->nMainSelectedHLColor = StringToChar(szTemp);
      }
      break;
    case 'K':
      input(szTemp, 3);
      if (szTemp[0]) {
        pMenuHeader->nItemBorderColor = StringToChar(szTemp);
      }
      break;
    case 'L':
      input(szTemp, 3);
      if (szTemp[0]) {
        pMenuHeader->nItemBoxColor = StringToChar(szTemp);
      }
      break;
    case 'M':
      input(szTemp, 3);
      if (szTemp[0]) {
        pMenuHeader->nItemTextColor = StringToChar(szTemp);
      }
      break;
    case 'N':
      input(szTemp, 3);
      if (szTemp[0]) {
        pMenuHeader->nItemTextHLColor = StringToChar(szTemp);
      }
      break;
    case 'O':
      input(szTemp, 3);
      if (szTemp[0]) {
        pMenuHeader->nItemSelectedColor = StringToChar(szTemp);
      }
      break;
    case 'P':
      input(szTemp, 3);
      if (szTemp[0]) {
        pMenuHeader->nItemSelectedHLColor = StringToChar(szTemp);
      }
      break;
    case 'Q':
      done = true;
      break;
    }
  }
}


void ListMenuDirs() {
  char szName[20];
  char szDesc[101];
  WFindFile fnd;
  bool bFound;

  string path = GetMenuDirectory() + "*";

  OpenMenuDescriptions();

  bout.nl();
  bout << "|#7Available Menus Sets\r\n";
  bout << "|#5============================\r\n";

  bFound = fnd.open(path.c_str(), 0);
  while (bFound && !hangup) {
    string fileName = fnd.GetFileName();
    if (fnd.IsDirectory()) {
      WWIV_GetFileNameFromPath(fileName.c_str(), szName);
      bout.bprintf("|#2%-8.8s |15%-60.60s\r\n", szName, GetMenuDescription(fileName, szDesc));
    }
    bFound = fnd.next();
  }
  bout.nl();

  CloseMenuDescriptions();
  bout.Color(0);
}


void ListMenuMenus(const char *pszDirectoryName) {
  string path = GetMenuDirectory(pszDirectoryName) + "*.mnu";

  bout.nl();
  bout << "|#7Available Menus\r\n";
  bout << "|#5===============|06\r\n";

  WFindFile fnd;
  bool bFound = fnd.open(path.c_str(), 0);
  while (bFound && !hangup) {
    if (fnd.IsFile()) {
      string s = fnd.GetFileName();
      bout << s.substr(0, s.find_last_of('.')) << wwiv::endl;
    }
    bFound = fnd.next();
  }
  bout.nl();
  bout.Color(0);
}
