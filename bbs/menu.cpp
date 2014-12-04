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

#include "wwiv.h"

#include "common.h"
#include "instmsg.h"
#include "menu.h"
#include "menusupp.h"
#include "printfile.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/wwivassert.h"

static user_config *pSecondUserRec;         // Userrec2 style setup
static int nSecondUserRecLoaded;            // Whos config is loaded

// TODO: move this to TextFile or File
static FILE *hMenuDesc;

static char *pMenuStrings;
static char **ppMenuStringsIndex;
static int nNumMenuCmds;

using std::string;
using namespace wwiv::strings;

// Local function prototypes
bool CheckMenuPassword(char* pszCorrectPassword);
bool LoadMenuSetup(int nUserNum);
bool ValidateMenuSet(const char *pszMenuDir);
void ReadMenuSetup();
void StartMenus();
void CloseMenu(MenuInstanceData * pMenuData);
bool OpenMenu(MenuInstanceData * pMenuData);
bool CheckMenuSecurity(MenuHeader * pHeader, bool bCheckPassword);
bool LoadMenuRecord(MenuInstanceData * pMenuData, string& command, MenuRec * pMenu);
void MenuExecuteCommand(MenuInstanceData * pMenuData, string& command);
void LogUserFunction(MenuInstanceData * pMenuData, string& command, MenuRec * pMenu);
void PrintMenuPrompt(MenuInstanceData * pMenuData);
bool AMIsNumber(string& command);
void QueryMenuSet();
void WriteMenuSetup(int nUserNum);
void UnloadMenuSetup();
string GetCommand(MenuInstanceData * pMenuData);
bool CheckMenuItemSecurity(MenuRec * pMenu, bool bCheckPassword);
void GenerateMenu(MenuInstanceData * pMenuData);
char *MenuDoParenCheck(char *pszSrc, int bMore, char *porig);
char *MenuGetParam(char *pszSrc, char *pszParam);
char *MenuSkipSpaces(char *pszSrc);
void InterpretCommand(MenuInstanceData * pMenuData, const char *pszScript);


bool CheckMenuPassword(char* pszCorrectPassword) {
  string password = IsEqualsIgnoreCase(pszCorrectPassword, "*SYSTEM") ? syscfg.systempw : pszCorrectPassword;

  bout.nl();
  string passwordFromUser;
  input_password("|#2SY: ", &passwordFromUser, 20);
  return passwordFromUser == password;
}

// returns -1 of the item is not matched, this will fall though to the
// default case in InterpretCommand
int GetMenuIndex(const char* pszCommand) {
  for (int i = 0; i < nNumMenuCmds; i++) {
    char* p = ppMenuStringsIndex[i];
    if (p && *p && IsEqualsIgnoreCase(pszCommand, p)) {
      return i;
    }
  }
  return -1;
}

void ReadMenuSetup() {
  if (pMenuStrings == nullptr) {
    char szMenuCmdsFileName[MAX_PATH];
    sprintf(szMenuCmdsFileName, "%s%s", syscfg.datadir, MENUCMDS_DAT);
    FILE* fp = fopen(szMenuCmdsFileName, "rb");
    if (!fp) {
      sysoplog("Unable to open menucmds.dat");
      exit(0);
    }

    fseek(fp, 0, SEEK_END);
    long lLen = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    short int nAmt = 0;
    fread(&nAmt, 2, 1, fp);
    if (nAmt == 0) {
      exit(0);
    }

    short int* index = static_cast<short *>(malloc(sizeof(short int) * nAmt));
    fread(index, 2, nAmt, fp);
    lLen -= ftell(fp);
    pMenuStrings = static_cast<char *>(malloc(lLen));
    ppMenuStringsIndex = static_cast<char **>(malloc(sizeof(char **) * nAmt));
    fread(pMenuStrings, lLen, 1, fp);
    fclose(fp);

    for (int nX = 0; nX < nAmt; nX++) {
      ppMenuStringsIndex[nX] = pMenuStrings + index[nX];
    }
    nNumMenuCmds = nAmt;

    free(index);
  }
}

void mainmenu() {
  if (pSecondUserRec) {
    free(pSecondUserRec);
    pSecondUserRec = nullptr;
  }
  pSecondUserRec = static_cast<user_config *>(malloc(sizeof(user_config)));
  if (!pSecondUserRec) {
    return;
  }

  ReadMenuSetup();

  while (!hangup) {
    StartMenus();
  }

  free(pSecondUserRec);
  pSecondUserRec = nullptr;
  nSecondUserRecLoaded = 0;

  free(pMenuStrings);
  pMenuStrings = nullptr;
  free(ppMenuStringsIndex);
  ppMenuStringsIndex = nullptr;
}

void StartMenus() {
  MenuInstanceData* pMenuData = static_cast<MenuInstanceData *>(malloc(sizeof(MenuInstanceData)));
  pMenuData->pMenuFile = nullptr;
  if (!pMenuData) {
    sysoplog("Unable to allocate memory for pMenuData");
    hangup = true;
    return;
  }
  pMenuData->nReload = 1;                    // force loading of menu

  if (!LoadMenuSetup(GetSession()->usernum)) {
    strcpy(pSecondUserRec->szMenuSet, "wwiv"); // default menu set name
    pSecondUserRec->cHotKeys = HOTKEYS_ON;
    pSecondUserRec->cMenuType = MENUTYPE_REGULAR;
    WriteMenuSetup(GetSession()->usernum);
  }
  while (pMenuData->nReload != 0 && !hangup) {
    if (pMenuData->pMenuFile != nullptr) {
      delete pMenuData->pMenuFile;
      pMenuData->pMenuFile = nullptr;

    }
    memset(pMenuData, 0, sizeof(MenuInstanceData));

    pMenuData->nFinished = 0;
    pMenuData->nReload = 0;

    if (!LoadMenuSetup(GetSession()->usernum)) {
      LoadMenuSetup(1);
      ConfigUserMenuSet();
    }
    if (!ValidateMenuSet(pSecondUserRec->szMenuSet)) {
      ConfigUserMenuSet();
    }

    Menus(pMenuData, pSecondUserRec->szMenuSet, "main"); // default starting menu
  }
  if (pMenuData) {
    free(pMenuData);
  }
}

void Menus(MenuInstanceData * pMenuData, const string menuDirectory, const string menuName) {
  strcpy(pMenuData->szPath, menuDirectory.c_str());
  strcpy(pMenuData->szMenu, menuName.c_str());

  if (OpenMenu(pMenuData)) {
    if (pMenuData->header.nNumbers == MENU_NUMFLAG_DIRNUMBER && udir[0].subnum == -1) {
      bout << "\r\nYou cannot currently access the file section.\r\n\n";
      CloseMenu(pMenuData);
      return;
    }
    // if flagged to display help on entrance, then do so
    if (GetSession()->GetCurrentUser()->IsExpert() && pMenuData->header.nForceHelp == MENU_HELP_ONENTRANCE) {
      AMDisplayHelp(pMenuData);
    }

    while (!hangup && pMenuData->nFinished == 0) {
      PrintMenuPrompt(pMenuData);
      string command = GetCommand(pMenuData);
      MenuExecuteCommand(pMenuData, command);
    }
  } else if (IsEqualsIgnoreCase(menuName.c_str(), "main")) {     // default menu name
    hangup = true;
  }
  CloseMenu(pMenuData);
}

void CloseMenu(MenuInstanceData * pMenuData) {
  if (pMenuData->pMenuFile != nullptr && pMenuData->pMenuFile->IsOpen()) {
    pMenuData->pMenuFile->Close();
    delete pMenuData->pMenuFile;
    pMenuData->pMenuFile = nullptr;
  }

  if (pMenuData->index != nullptr) {
    free(pMenuData->index);
    pMenuData->index = nullptr;
  }

  if (pMenuData->szPrompt != nullptr) {
    free(pMenuData->szPrompt);
    pMenuData->szPrompt = nullptr;
  }
}

static bool CreateIndex(MenuInstanceData* pMenuData) {
  time_t start = time(nullptr);
  pMenuData->index = static_cast<MenuRecIndex *>(malloc(pMenuData->nAmountRecs * sizeof(MenuRecIndex) + TEST_PADDING));
  int nAmount = pMenuData->pMenuFile->GetLength() / sizeof(MenuRec);

  for (int nRec = 1; nRec < nAmount; nRec++) {
    MenuRec menu;
    pMenuData->pMenuFile->Seek(nRec * sizeof(MenuRec), File::seekBegin);
    pMenuData->pMenuFile->Read(&menu, sizeof(MenuRec));

    memset(&pMenuData->index[nRec-1], 0, sizeof(MenuRecIndex));
    pMenuData->index[nRec-1].nRec = static_cast<short>(nRec);
    pMenuData->index[nRec-1].nFlags = menu.nFlags;
    strcpy(pMenuData->index[nRec-1].szKey, menu.szKey);
  }
  bout << "Time to create index for menu: " << time(nullptr) - start << " ms." << wwiv::endl;
  return true;
}

bool OpenMenu(MenuInstanceData * pMenuData) {
  CloseMenu(pMenuData);

  // --------------------------
  // Open up the main data file
  // --------------------------
  pMenuData->pMenuFile = new File(GetMenuDirectory(pMenuData->szPath, pMenuData->szMenu, "mnu"));
  pMenuData->pMenuFile->Open(File::modeBinary | File::modeReadOnly, File::shareDenyNone);

  // -----------------------------------
  // Find out how many records there are
  // -----------------------------------
  if (pMenuData->pMenuFile->IsOpen()) {
    long lSize = pMenuData->pMenuFile->GetLength();
    pMenuData->nAmountRecs = static_cast<uint16_t>(lSize / sizeof(MenuRec));
  } else {
    // Unable to open menu
    MenuSysopLog("Unable to open Menu");
    pMenuData->nAmountRecs = 0;
    return false;
  }

  // -------------------------
  // Read the header (control)
  // record into memory
  // -------------------------
  pMenuData->pMenuFile->Seek(0L, File::seekBegin);
  pMenuData->pMenuFile->Read(&pMenuData->header, sizeof(MenuHeader));

  // version numbers can be checked here

  if (!CreateIndex(pMenuData)) {
    MenuSysopLog("Unable to create menu index.");
    pMenuData->nAmountRecs = 0;
    return false;
  }

  // ----------------------------
  // Open/Rease/Close Prompt file
  // ----------------------------
  File filePrompt(GetMenuDirectory(pMenuData->szPath, pMenuData->szMenu, "pro"));
  if (filePrompt.Open(File::modeBinary | File::modeReadOnly, File::shareDenyNone)) {
    long lSize = filePrompt.GetLength();
    pMenuData->szPrompt = static_cast<char *>(malloc(lSize + 10 + TEST_PADDING));
    if (pMenuData->szPrompt != nullptr) {
      lSize = filePrompt.Read(pMenuData->szPrompt, lSize);
      pMenuData->szPrompt[lSize] = 0;

      char* sp = strstr(pMenuData->szPrompt, ".end.");
      if (sp) {
        sp[0] = '\0';
      }
    }
    filePrompt.Close();
  }
  if (!CheckMenuSecurity(&pMenuData->header, true)) {
    MenuSysopLog("< Menu Sec");
    return false;
  }
  if (pMenuData->header.szScript[0]) {
    InterpretCommand(pMenuData, pMenuData->header.szScript);
  }

  return true;
}

bool CheckMenuSecurity(MenuHeader * pHeader, bool bCheckPassword) {
  if ((pHeader->nFlags & MENU_FLAG_DELETED) ||
      (GetSession()->GetEffectiveSl() < pHeader->nMinSL) ||
      (GetSession()->GetCurrentUser()->GetDsl() < pHeader->nMinDSL)) {
    return false;
  }

  // All AR bits specified must match
  for (short int x = 0; x < 16; x++) {
    if (pHeader->uAR & (1 << x)) {
      if (!GetSession()->GetCurrentUser()->HasArFlag(1 << x)) {
        return false;
      }
    }
  }

  // All DAR bits specified must match
  for (short int x = 0; x < 16; x++) {
    if (pHeader->uDAR & (1 << x)) {
      if (!GetSession()->GetCurrentUser()->HasDarFlag(1 << x)) {
        return (GetSession()->GetCurrentUser()->GetDsl() < pHeader->nMinDSL);
      }
    }
  }

  // If any restrictions match, then they arn't allowed
  for (short int x = 0; x < 16; x++) {
    if (pHeader->uRestrict & (1 << x)) {
      if (GetSession()->GetCurrentUser()->HasRestrictionFlag(1 << x)) {
        return (GetSession()->GetCurrentUser()->GetDsl() < pHeader->nMinDSL);
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

bool LoadMenuRecord(MenuInstanceData * pMenuData, string& command, MenuRec * pMenu) {
  memset(pMenu, 0, sizeof(MenuRec));

  // ------------------------------------------------
  // If we have 'numbers set the sub #' turned on
  // then create a command to do so if a # is entered
  // ------------------------------------------------
  if (AMIsNumber(command)) {
    if (pMenuData->header.nNumbers == MENU_NUMFLAG_SUBNUMBER) {
      memset(pMenu, 0, sizeof(MenuRec));
      sprintf(pMenu->szExecute, "SetSubNumber %d", atoi(command.c_str()));
      return true;
    }
    if (pMenuData->header.nNumbers == MENU_NUMFLAG_DIRNUMBER) {
      memset(pMenu, 0, sizeof(MenuRec));
      sprintf(pMenu->szExecute, "SetDirNumber %d", atoi(command.c_str()));
      return true;
    }
  }
  for (int x = 0; x < pMenuData->nAmountRecs; x++) {
    if (IsEqualsIgnoreCase(pMenuData->index[x].szKey, command.c_str())) {
      if ((pMenuData->index[x].nFlags & MENU_FLAG_DELETED) == 0) {
        if (pMenuData->index[x].nRec != 0) {
          // Dont include control record
          pMenuData->pMenuFile->Seek(pMenuData->index[x].nRec * sizeof(MenuRec), File::seekBegin);
          pMenuData->pMenuFile->Read(pMenu, sizeof(MenuRec));

          if (CheckMenuItemSecurity(pMenu, 1)) {
            return true;
          } else {
            std::ostringstream msg;
            msg << "< item security : " << command;
            MenuSysopLog(msg.str());
            return false;
          }
        }
      }
    }
  }
  return false;
}


void MenuExecuteCommand(MenuInstanceData * pMenuData, string& command) {
  MenuRec menu;

  if (LoadMenuRecord(pMenuData, command, &menu)) {
    LogUserFunction(pMenuData, command, &menu);
    InterpretCommand(pMenuData, menu.szExecute);
  } else {
    LogUserFunction(pMenuData, command, &menu);
  }
}


void LogUserFunction(MenuInstanceData * pMenuData, string& command, MenuRec * pMenu) {
  switch (pMenuData->header.nLogging) {
  case MENU_LOGTYPE_KEY:
    sysopchar(command.c_str());
    break;
  case MENU_LOGTYPE_COMMAND:
    sysoplog(pMenu->szExecute);
    break;
  case MENU_LOGTYPE_DESC:
    sysoplog(pMenu->szMenuText[0] ? pMenu->szMenuText : pMenu->szExecute);
    break;
  case MENU_LOGTYPE_NONE:
  default:
    break;
  }
}

void MenuSysopLog(const string msg) {
  std::ostringstream logStream;
  logStream << "*MENU* : " << msg;

  sysopchar(logStream.str().c_str());
  bout << logStream.str();
  bout.nl();
}

void PrintMenuPrompt(MenuInstanceData * pMenuData) {
  if (!GetSession()->GetCurrentUser()->IsExpert() || pMenuData->header.nForceHelp == MENU_HELP_FORCE) {
    AMDisplayHelp(pMenuData);
  }
  TurnMCIOn();
  if (pMenuData->szPrompt) {
    bout << pMenuData->szPrompt;
  }
  TurnMCIOff();
}

string GetHelpFileName(MenuInstanceData * pMenuData) {
  if (GetSession()->GetCurrentUser()->HasAnsi()) {
    if (GetSession()->GetCurrentUser()->HasColor()) {
      string filename = GetMenuDirectory(pMenuData->szPath, pMenuData->szMenu, "ans");
      if (File::Exists(filename)) {
        return filename;
      }
    }
    string filename = GetMenuDirectory(pMenuData->szPath, pMenuData->szMenu, "b&w");
    if (File::Exists(filename)) {
      return filename;
    }
  }
  return GetMenuDirectory(pMenuData->szPath, pMenuData->szMenu, "msg");
}

void AMDisplayHelp(MenuInstanceData * pMenuData) {
  const string filename = GetHelpFileName(pMenuData);
  if (!printfile(filename, true)) {
    GenerateMenu(pMenuData);
  }
}


void TurnMCIOff() {
  if (!(syscfg.sysconfig & sysconfig_enable_mci)) {
    g_flags |= g_flag_disable_mci;
  }
}

void TurnMCIOn() {
  g_flags &= ~g_flag_disable_mci;
}

bool AMIsNumber(string& command) {
  if (!command.length()) {
    return false;
  }

  for (const auto& ch : command) {
    if (isdigit(ch) == 0) {
      return false;
    }
  }
  return true;
}

void ConfigUserMenuSet() {
  ReadMenuSetup();

  if (GetSession()->usernum != nSecondUserRecLoaded) {
    if (!LoadMenuSetup(GetSession()->usernum)) {
      LoadMenuSetup(1);
    }
  }

  nSecondUserRecLoaded = GetSession()->usernum;

  bout.cls();
  printfile(MENUWEL_NOEXT);
  bool bDone = false;
  while (!bDone && !hangup) {
    bout << "   |#1WWIV |#6Menu |#1Editor|#0\r\n\r\n";
    bout << "|#21|06) |#1Menuset      |06: |15" << pSecondUserRec->szMenuSet << wwiv::endl;
    bout << "|#22|06) |#1Use hot keys |06: |15" << (pSecondUserRec->cHotKeys == HOTKEYS_ON ? "Yes" : "No ") <<
                       wwiv::endl;
    bout.nl();
    bout << "|#9[|0212? |08Q|02=Quit|#9] :|#0 ";

    char chKey = onek("Q12?");

    switch (chKey) {
    case 'Q':
      bDone = 1;
      break;

    case '1': {
      ListMenuDirs();
      bout.nl(2);
      bout << "|15Enter the menu set to use : |#0";
      string menuSetName;
      inputl(&menuSetName, 8);
      if (ValidateMenuSet(menuSetName.c_str())) {
        OpenMenuDescriptions();
        bout.nl();
        bout << "|#1Menu Set : |#2" <<  menuSetName.c_str() << "  -  |15" << GetMenuDescription(menuSetName) << wwiv::endl;
        bout << "|#5Use this menu set? ";
        if (noyes()) {
          strcpy(pSecondUserRec->szMenuSet, menuSetName.c_str());
          break;
        }
        CloseMenuDescriptions();
      }
      bout.nl();
      bout << "|#8That menu set does not exists, resetting to the default menu set" << wwiv::endl;
      pausescr();
      if (pSecondUserRec->szMenuSet[0] == '\0') {
        strcpy(pSecondUserRec->szMenuSet, "wwiv");
      }
    }
    break;

    case '2':
      pSecondUserRec->cHotKeys = !pSecondUserRec->cHotKeys;
      break;

    case '3':
      pSecondUserRec->cMenuType = !pSecondUserRec->cMenuType;
      break;

    case '?':
      printfile(MENUWEL_NOEXT);
      continue;                           // bypass the below cls()
    }

    bout.cls();
  }

  // If menu is invalid, it picks the first one it finds
  if (!ValidateMenuSet(pSecondUserRec->szMenuSet)) {
    if (GetSession()->num_languages > 1 && GetSession()->GetCurrentUser()->GetLanguage() != 0) {
      bout << "|#6No menus for " << languages[GetSession()->GetCurrentUser()->GetLanguage()].name <<
                         " language.";
      input_language();
    }
  }

  WriteMenuSetup(GetSession()->usernum);

  MenuSysopLog(StringPrintf("Menu in use : %s - %s - %s", pSecondUserRec->szMenuSet,
          pSecondUserRec->cHotKeys == HOTKEYS_ON ? "Hot" : "Off", pSecondUserRec->cMenuType == MENUTYPE_REGULAR ? "REG" : "PD"));
  bout.nl(2);
}


void QueryMenuSet() {
  user_config tmpcfg;
  bool bSecondUserRecLoaded = false;

  if (!pSecondUserRec) {
    bSecondUserRecLoaded = true;
    pSecondUserRec = &tmpcfg;
  }
  ReadMenuSetup();

  if (GetSession()->usernum != nSecondUserRecLoaded) {
    if (!LoadMenuSetup(GetSession()->usernum)) {
      LoadMenuSetup(1);
    }
  }

  nSecondUserRecLoaded = GetSession()->usernum;

  ValidateMenuSet(pSecondUserRec->szMenuSet);

  bout.nl(2);
  if (pSecondUserRec->szMenuSet[0] == 0) {
    strcpy(pSecondUserRec->szMenuSet, "WWIV");
  }
  bout << "|#7Configurable menu set status:\r\n\r\n";
  bout << "|#8Menu in use  : |#9" << pSecondUserRec->szMenuSet << wwiv::endl;
  bout << "|#8Hot keys are : |#9" << (pSecondUserRec->cHotKeys == HOTKEYS_ON ? "On" : "Off") << wwiv::endl;
  bout.nl();

  bout << "|#7Would you like to change these? (N) ";
  if (yesno()) {
    ConfigUserMenuSet();
  }

  if (bSecondUserRecLoaded) {
    pSecondUserRec = nullptr;
    nSecondUserRecLoaded = 0;
  }
}

bool ValidateMenuSet(const char *pszMenuDir) {
  if (GetSession()->usernum != nSecondUserRecLoaded) {
    if (!LoadMenuSetup(GetSession()->usernum)) {
      LoadMenuSetup(1);
    }
  }

  nSecondUserRecLoaded = GetSession()->usernum;

  // ensure the entry point exists
  return File::Exists(GetMenuDirectory(pszMenuDir), "main.mnu");
}

bool LoadMenuSetup(int nUserNum) {
  if (!pSecondUserRec) {
    MenuSysopLog("Mem Error");
    return false;
  }
  UnloadMenuSetup();


  if (!nUserNum) {
    return false;
  }

  File userConfig(syscfg.datadir, CONFIG_USR);
  if (userConfig.Exists()) {
    WUser user;
    GetApplication()->GetUserManager()->ReadUser(&user, nUserNum);
    if (userConfig.Open(File::modeReadOnly | File::modeBinary)) {
      userConfig.Seek(nUserNum * sizeof(user_config), File::seekBegin);

      int len = userConfig.Read(pSecondUserRec, sizeof(user_config));
      userConfig.Close();

      if (len != sizeof(user_config) || !IsEqualsIgnoreCase(reinterpret_cast<char*>(pSecondUserRec->name), user.GetName())) {
        memset(pSecondUserRec, 0, sizeof(user_config));
        strcpy(reinterpret_cast<char*>(pSecondUserRec->name), user.GetName());
        return 0;
      }
      nSecondUserRecLoaded = nUserNum;

      return true;
    }
  }
  return false;
}

void WriteMenuSetup(int nUserNum) {
  if (!nUserNum) {
    return;
  }

  WUser user;
  GetApplication()->GetUserManager()->ReadUser(&user, nUserNum);
  strcpy(pSecondUserRec->name, user.GetName());

  File userConfig(syscfg.datadir, CONFIG_USR);
  if (!userConfig.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    return;
  }

  userConfig.Seek(nUserNum * sizeof(user_config), File::seekBegin);
  userConfig.Write(pSecondUserRec, sizeof(user_config));
  userConfig.Close();
}

void UnloadMenuSetup() {
  nSecondUserRecLoaded = 0;
  memset(pSecondUserRec, 0, sizeof(user_config));
}

string GetCommand(MenuInstanceData * pMenuData) {
  if (pSecondUserRec->cHotKeys == HOTKEYS_ON) {
    if (pMenuData->header.nNumbers == MENU_NUMFLAG_DIRNUMBER) {
      write_inst(INST_LOC_XFER, udir[GetSession()->GetCurrentFileArea()].subnum, INST_FLAGS_NONE);
      return string(mmkey(1, WSession::mmkeyFileAreas));
    } else if (pMenuData->header.nNumbers == MENU_NUMFLAG_SUBNUMBER) {
      write_inst(INST_LOC_MAIN, usub[GetSession()->GetCurrentMessageArea()].subnum, INST_FLAGS_NONE);
      return string(mmkey(0, WSession::mmkeyMessageAreas));
    } else {
      odc[0] = '/';
      odc[1] = '\0';
      return string(mmkey(2));
    }
  } else {
    string text;
    input(&text, 50);
    return string(text);
  }
}

bool CheckMenuItemSecurity(MenuRec * pMenu, bool bCheckPassword) {
  // if deleted, return as failed
  if ((pMenu->nFlags & MENU_FLAG_DELETED) ||
      (GetSession()->GetEffectiveSl() < pMenu->nMinSL) ||
      (GetSession()->GetEffectiveSl() > pMenu->iMaxSL && pMenu->iMaxSL != 0) ||
      (GetSession()->GetCurrentUser()->GetDsl() < pMenu->nMinDSL) ||
      (GetSession()->GetCurrentUser()->GetDsl() > pMenu->iMaxDSL && pMenu->iMaxDSL != 0)) {
    return false;
  }

  // All AR bits specified must match
  for (int x = 0; x < 16; x++) {
    if (pMenu->uAR & (1 << x)) {
      if (!GetSession()->GetCurrentUser()->HasArFlag(1 << x)) {
        return false;
      }
    }
  }

  // All DAR bits specified must match
  for (int x = 0; x < 16; x++) {
    if (pMenu->uDAR & (1 << x)) {
      if (!GetSession()->GetCurrentUser()->HasDarFlag(1 << x)) {
        return false;
      }
    }
  }

  // If any restrictions match, then they arn't allowed
  for (int x = 0; x < 16; x++) {
    if (pMenu->uRestrict & (1 << x)) {
      if (GetSession()->GetCurrentUser()->HasRestrictionFlag(1 << x)) {
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

void OpenMenuDescriptions() {
  const string menu_description_file = GetMenuDescriptionFile();
  hMenuDesc = fopen(menu_description_file.c_str(), "r");
}

void CloseMenuDescriptions() {
  if (hMenuDesc) {
    fclose(hMenuDesc);
  }

  hMenuDesc = nullptr;
}

const string GetMenuDescription(const string& name) {
  string description;
  if (!hMenuDesc) {
    return "";
  }
  fseek(hMenuDesc, 0, SEEK_SET);

  char szLine[201];
  while (!hangup) {
    if (!fgets(szLine, 200, hMenuDesc)) {
    return "";
    }
    char* pszTemp = strchr(szLine, ' ');
    if (!pszTemp) {
      continue;
    }

    pszTemp[0] = 0;
    ++pszTemp;

    if (name == szLine) {
      description.assign(pszTemp);
      StringTrim(&description);
      if (description.size() > 55) {
        return description.substr(0, 55);
      }
      return description;
    }
  }
  return "";
}

// tokenize a line.
static char *stptok(const char *pszText, char *pszToken, size_t nTokenLength, const char *brk) {
  pszToken[0] = '\0';

  WWIV_ASSERT(pszText);
  WWIV_ASSERT(pszToken);
  WWIV_ASSERT(brk);

  if (!pszText || !*pszText) {
    return nullptr;
  }

  char* lim = pszToken + nTokenLength - 1;
  bool bFoundFirst = false;
  while (*pszText && pszToken < lim) {
    bool bCountThis = true;
    for (const char* b = brk; *b; b++) {
      if (*pszText == *b) {
        if (bFoundFirst) {
          *pszToken = 0;
          return const_cast<char *>(pszText);
        } else {
          bCountThis = false;
        }
      }
    }
    if (bCountThis) {
      *pszToken++ = *pszText++;
      bFoundFirst = true;
    } else {
      pszText++;
    }
  }
  *pszToken = '\0';
  return const_cast<char *>(pszText);
}

void SetMenuDescription(const string& name, const string& description) {
  char szLine[MAX_PATH], szTok[26];
  int bWritten = 0;
  bool bMenuOpen = false;

  if (!hMenuDesc) {
    bMenuOpen = false;
    OpenMenuDescriptions();
  } else {
    bMenuOpen = true;
  }

  TextFile tempDescriptionFile(GetMenuDirectory(), TEMP_ION, "wt");

  if (!tempDescriptionFile.IsOpen()) {
    MenuSysopLog("Unable to write description");
    return;
  }
  if (hMenuDesc) {
    fseek(hMenuDesc, 0, SEEK_SET);

    while (!hangup) {
      if (!fgets(szLine, 200, hMenuDesc)) {
        break;
      }

      stptok(szLine, szTok, 25, " ");

      if (IsEqualsIgnoreCase(name.c_str(), szTok)) {
        tempDescriptionFile.WriteFormatted("%s %s\n", name.c_str(), description.c_str());
        bWritten = 1;
      } else {
        tempDescriptionFile.WriteFormatted("%s", szLine);
      }
    }
  }
  if (!bWritten) {
    tempDescriptionFile.WriteFormatted("%s %s\n", name.c_str(), description.c_str());
  }

  tempDescriptionFile.Close();
  CloseMenuDescriptions();

  File descriptionFile(GetMenuDirectory(), DESCRIPT_ION);
  File::Remove(descriptionFile.full_pathname());
  File::Rename(tempDescriptionFile.full_pathname(), descriptionFile.full_pathname());

  if (bMenuOpen) {
    OpenMenuDescriptions();
  }
}

const string GetMenuDescriptionFile() {
  std::ostringstream os;
  os << GetMenuDirectory() << DESCRIPT_ION;
  return string(os.str());
}

const string GetMenuDirectory(const string menuPath) {
  std::ostringstream os;
  os << GetMenuDirectory() << menuPath << File::pathSeparatorString;
  return string(os.str());
}

const string GetMenuDirectory(const string menuPath, const string menuName,
                                   const string extension) {
  std::ostringstream os;
  os << GetMenuDirectory() << menuPath << File::pathSeparatorString << menuName << "." << extension;
  return string(os.str());
}

const string GetMenuDirectory() {
  std::ostringstream os;
  os << GetSession()->language_dir << "menus" << File::pathSeparatorChar;
  return string(os.str());
}

void GenerateMenu(MenuInstanceData * pMenuData) {
  MenuRec menu;

  memset(&menu, 0, sizeof(MenuRec));

  bout.Color(0);
  bout.nl();

  int iDisplayed = 0;
  if (pMenuData->header.nNumbers != MENU_NUMFLAG_NOTHING) {
    bout.bprintf("|#1%-8.8s  |#2%-25.25s  ", "[#]", "Change Sub/Dir #");
    ++iDisplayed;
  }
  for (int x = 0; x < pMenuData->nAmountRecs - 1; x++) {
    if ((pMenuData->index[x].nFlags & MENU_FLAG_DELETED) == 0) {
      if (pMenuData->index[x].nRec != 0) {
        // Dont include control record
        pMenuData->pMenuFile->Seek(pMenuData->index[x].nRec * sizeof(MenuRec), File::seekBegin);
        pMenuData->pMenuFile->Read(&menu, sizeof(MenuRec));

        if (CheckMenuItemSecurity(&menu, false) &&
            menu.nHide != MENU_HIDE_REGULAR &&
            menu.nHide != MENU_HIDE_BOTH) {
          char szKey[30];
          if (strlen(menu.szKey) > 1 && menu.szKey[0] != '/' && pSecondUserRec->cHotKeys == HOTKEYS_ON) {
            sprintf(szKey, "//%s", menu.szKey);
          } else {
            sprintf(szKey, "[%s]", menu.szKey);
          }

          bout.bprintf("|#1%-8.8s  |#2%-25.25s  ", szKey,
                                            menu.szMenuText[0] ? menu.szMenuText : menu.szExecute);

          if (iDisplayed % 2) {
            bout.nl();
          }

          ++iDisplayed;
        }
      }
    }
  }
  if (IsEquals(GetSession()->GetCurrentUser()->GetName(), "GUEST")) {
    if (iDisplayed % 2) {
      bout.nl();
    }
    bout.bprintf("|#1%-8.8s  |#2%-25.25s  ",
                                      pSecondUserRec->cHotKeys == HOTKEYS_ON ? "//APPLY" : "[APPLY]",
                                      "Guest Account Application");
    ++iDisplayed;
  }
  bout.nl(2);
  return;
}

// MenuParseLine(szSrc, szCmd, szParam1, szParam2)
//
// szSrc    = Line to parse
// szCmd    = Returns the command to be executed
// szParam1 = 1st parameter - if it exists
// szParam2 = 2cd paramater - if it exists
//
// return value is where to continue parsing this line
//
// szSrc to be interpreted can be formated like this
// either  cmd param1 param2
// or     cmd(param1, param2)
//   multiple lines are seperated with the ~ key
//   enclose multi word text in quotes
//
char *MenuParseLine(char *pszSrc, char *pszCmd, char *pszParam1, char *pszParam2) {
  char *porig = pszSrc;

  pszCmd[0] = 0;
  pszParam1[0] = 0;
  pszParam2[0] = 0;

  pszSrc = MenuSkipSpaces(pszSrc);
  while (pszSrc[0] == '~' && !hangup) {
    ++pszSrc;
    pszSrc = MenuSkipSpaces(pszSrc);
  }

  int nLen = 0;
  while (isalnum(*pszSrc) && !hangup) {
    if (nLen < 30) {
      pszCmd[nLen++] = *pszSrc;
    }
    ++pszSrc;
  }
  pszCmd[nLen] = 0;

  pszSrc = MenuSkipSpaces(pszSrc);

  bool bParen = false;
  if (*pszSrc == '(') {
    ++pszSrc;
    bParen = true;
    pszSrc = MenuSkipSpaces(pszSrc);
  }
  pszSrc = MenuGetParam(pszSrc, pszParam1);
  pszSrc = MenuSkipSpaces(pszSrc);

  if (bParen) {
    pszSrc = MenuDoParenCheck(pszSrc, 1, porig);
  }

  pszSrc = MenuGetParam(pszSrc, pszParam2);
  pszSrc = MenuSkipSpaces(pszSrc);

  if (bParen) {
    pszSrc = MenuDoParenCheck(pszSrc, 0, porig);
  }

  pszSrc = MenuSkipSpaces(pszSrc);

  if (pszSrc[0] != 0 && pszSrc[0] != '~') {
    MenuSysopLog("Expected EOL");
    MenuSysopLog(porig);
  }
  return pszSrc;
}

char *MenuDoParenCheck(char *pszSrc, int bMore, char *porig) {
  if (pszSrc[0] == ',') {
    if (bMore == 0) {
      MenuSysopLog("Too many paramaters in line of code");
      MenuSysopLog(porig);
    }
    ++pszSrc;
    pszSrc = MenuSkipSpaces(pszSrc);
  } else if (pszSrc[0] == ')') {
    ++pszSrc;
    pszSrc = MenuSkipSpaces(pszSrc);
    if (pszSrc[0] != 0 && pszSrc[0] != '~') {
      MenuSysopLog("Invalid Code, exptected EOL after close parentheses");
      MenuSysopLog(porig);
    }
  } else if (pszSrc[0] == 0 || pszSrc[0] == '~') {
    MenuSysopLog("Unexpected end of line (wanted a close parentheses)");
  }

  return pszSrc;
}

char *MenuGetParam(char *pszSrc, char *pszParam) {
  pszSrc = MenuSkipSpaces(pszSrc);
  pszParam[0] = 0;

  if (pszSrc[0] == 0 || pszSrc[0] == '~') {
    return pszSrc;
  }

  int nLen = 0;
  if (*pszSrc == '"') {
    ++pszSrc;
    nLen = 0;
    while (*pszSrc != '"' && *pszSrc != 0) {
      if (nLen < 50) {
        pszParam[nLen++] = *pszSrc;
      }
      ++pszSrc;
    }
    if (*pszSrc) {                           // We should either be on nullptr or the CLOSING QUOTE
      ++pszSrc;                              // If on the Quote, advance one
    }
    pszParam[nLen] = '\0';
  } else {
    nLen = 0;
    while (*pszSrc != ' ' && *pszSrc != 0 && *pszSrc != '~') {
      if (nLen < 50) {
        pszParam[nLen++] = *pszSrc;
      }
      ++pszSrc;
    }
    pszParam[nLen] = '\0';
  }
  return pszSrc;
}


char *MenuSkipSpaces(char *pszSrc) {
  while (isspace(pszSrc[0]) && pszSrc[0] != '\0') {
    ++pszSrc;
  }
  return pszSrc;
}


