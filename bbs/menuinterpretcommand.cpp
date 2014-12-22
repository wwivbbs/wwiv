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
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "bbs/bbslist.h"
#include "bbs/menu.h"
#include "bbs/menuspec.h"
#include "bbs/menusupp.h"
#include "bbs/menu_parser.h"
#include "bbs/new_bbslist.h"
#include "bbs/printfile.h"
#include "bbs/wwiv.h"
#include "core/inifile.h"
#include "core/stl.h"
#include "core/strings.h"

using std::map;
using std::string;
using std::unique_ptr;

using wwiv::core::FilePath;
using wwiv::core::IniFile;
using wwiv::bbslist::NewBBSList;

using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv {
namespace menus {

map<string, std::function<void(MenuInstanceData*, const string&, const string&)>, wwiv::stl::ci_less> CreateCommandMap();

bool UseNewBBSList() {
  IniFile iniFile(FilePath(application()->GetHomeDir(), WWIV_INI), INI_TAG);
  if (iniFile.IsOpen()) {
    return iniFile.GetBooleanValue("USE_NEW_BBSLIST", true);
  }
  return false;
}

void InterpretCommand(MenuInstanceData* pMenuData, const char *pszScript) {
  static map<string, std::function<void(MenuInstanceData* pMenuData, const string&, const string&)>, wwiv::stl::ci_less> functions = CreateCommandMap();

  char szCmd[31], szParam1[51], szParam2[51];
  char szTempScript[255];
  memset(szTempScript, 0, sizeof(szTempScript));
  strncpy(szTempScript, pszScript, 250);

  if (pszScript[0] == 0) {
    return;
  }

  const char* pszScriptPointer = szTempScript;
  while (pszScriptPointer && !hangup) {
    pszScriptPointer = MenuParseLine(pszScriptPointer, szCmd, szParam1, szParam2);

    if (szCmd[0] == 0) {    // || !pszScriptPointer || !*pszScriptPointer
      break;
    }

    string cmd(szCmd);
    if (contains(functions, cmd)) {
      functions.at(cmd)(pMenuData, szParam1, szParam2);
    }
  }
}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4100 )  // unreferenced formal parameter for pMenuData, param1, param2
#endif

map<string, std::function<void(MenuInstanceData*, const string&, const string&)>, wwiv::stl::ci_less> CreateCommandMap() {
  return {
    { "MENU", [&](MenuInstanceData* menu_data, const string& param1, const string& param2) {
      unique_ptr<MenuInstanceData> new_menu(new MenuInstanceData{});
      new_menu->Menus(menu_data->path, param1);
    } },
    { "ReturnFromMenu", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      InterpretCommand(pMenuData, pMenuData->header.szExitScript);
      pMenuData->finished = true;
    } },
    { "EditMenuSet", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      EditMenus();           // flag if we are editing this menu
      pMenuData->finished = true;
      pMenuData->reload = true;
    } },
    { "DLFreeFile", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      char s[MAX_PATH];
      strcpy(s, param2.c_str());
      align(s);
      MenuDownload(param1.c_str(), s, true, true);
    } },
    { "DLFile", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      char s[MAX_PATH];
      strcpy(s, param2.c_str());
      align(s);
      MenuDownload(param1.c_str(), s, false, true);
    } },
    { "RunDoor", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      MenuRunDoorName(param1.c_str(), false);
    } },
    { "RunDoorFree", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      MenuRunDoorName(param1.c_str(), true);
    } },
    { "RunDoorNumber", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      int nTemp = atoi(param1.c_str());
      MenuRunDoorNumber(nTemp, false);
    } },
    { "RunDoorNumberFree", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      int nTemp = atoi(param1.c_str());
      MenuRunDoorNumber(nTemp, true);
    } },
    { "PrintFile", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      printfile(param1, true);
    } },
    { "PrintFileNA", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      printfile(param1, false);
    } },
    { "SetSubNumber", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      SetSubNumber(param1.c_str());
    } },
    { "SetDirNumber", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      SetDirNumber(param1.c_str());
    } },
    { "SetMsgConf", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      SetMsgConf(param1.c_str()[0]);
    } },
    { "SetMsgConf", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      SetMsgConf(param1.c_str()[0]);
    } },
    { "SetDirConf", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      SetDirConf(param1.c_str()[0]);
    } },
    { "EnableConf", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      EnableConf();
    } },
    { "DisableConf", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      DisableConf();
    } },
    { "Pause", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      pausescr();
    } },
    { "ConfigUserMenuSet", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ConfigUserMenuSet();
      pMenuData->finished = true;
      pMenuData->reload = true;
    } },
    { "DisplayHelp", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      if (session()->user()->IsExpert()) {
        pMenuData->DisplayHelp();
      }
    } },
    { "SelectSub", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ChangeSubNumber();
    } },
    { "SelectDir", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ChangeDirNumber();
    } },
    { "SubList", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      SubList();
    } },
    { "UpSubConf", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      UpSubConf();
    } },
    { "DownSubConf", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      DownSubConf();
    } },
    { "UpSub", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      UpSub();
    } },
    { "DownSub", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      DownSub();
    } },
    { "ValidateUser", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ValidateUser();
    } },
    { "Doors", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      Chains();
    } },
    { "TimeBank", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      TimeBank();
    } },
    { "AutoMessage", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      AutoMessage();
    } },
    { "BBSList", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      if (UseNewBBSList()) {
        NewBBSList();
      } else {
        LegacyBBSList();
      }
    } },
    { "RequestChat", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      RequestChat();
    } },
    { "Defaults", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      Defaults(pMenuData);
    } },
    { "SendEMail", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      SendEMail();
    } },
    { "Feedback", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      FeedBack();
    } },
    { "Bulletins", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      Bulletins();
    } },
    { "HopSub", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      HopSub();
    } },
    { "SystemInfo", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      SystemInfo();
    } },
    { "JumpSubConf", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      JumpSubConf();
    } },
    { "KillEMail", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      KillEMail();
    } },
    { "LastCallers", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      LastCallers();
    } },
    { "ReadEMail", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ReadEMail();
    } },
    { "NewMessageScan", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      NewMessageScan();
    } },
    { "Goodbye", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      GoodBye();
    } },
    { "PostMessage", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      WWIV_PostMessage();
    } },
    { "NewMsgScanCurSub", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ScanSub();
    } },
    { "RemovePost", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      RemovePost();
    } },
    { "TitleScan", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      TitleScan();
    } },
    { "ListUsers", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ListUsers();
    } },
    { "Vote", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      Vote();
    } },
    { "ToggleExpert", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ToggleExpert();
    } },
    { "YourInfo", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      YourInfo();
    } },
    { "ExpressScan", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ExpressScan();
    } },
    { "WWIVVer", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      WWIVVersion();
    } },
    { "InstanceEdit", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      InstanceEdit();
    } },
    { "ConferenceEdit", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      JumpEdit();
    } },
    { "SubEdit", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      BoardEdit();
    } },
    { "ChainEdit", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ChainEdit();
    } },
    { "ToggleAvailable", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ToggleChat();
    } },
    { "ChangeUser", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ChangeUser();
    } },
    { "CLOUT", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      CallOut();
    } },
    { "Debug", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      Debug();
    } },
    { "DirEdit", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
    } },
    { "", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      DirEdit();
    } },
    { "Edit", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      EditText();
    } },
    { "BulletinEdit", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      EditBulletins();
    } },
    { "LoadText", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      LoadTextFile();
    } },
    { "ReadAllMail", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ReadAllMail();
    } },
    { "Reboot", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      RebootComputer();
    } },
    { "ReloadMenus", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ReloadMenus();
    } },
    { "ResetUserIndex", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ResetFiles();
    } },
    { "ResetQscan", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ResetQscan();
    } },
    { "MemStat", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      MemoryStatus();
    } },
    { "PackMsgs", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      PackMessages();
    } },
    { "VoteEdit", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      InitVotes();
    } },
    { "Log", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ReadLog();
    } },
    { "NetLog", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ReadNetLog();
    } },
    { "Pending", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      PrintPending();
    } },
    { "Status", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      PrintStatus();
    } },
    { "TextEdit", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      TextEdit();
    } },
    { "UserEdit", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      UserEdit();
    } },
    { "VotePrint", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      VotePrint();
    } },
    { "YLog", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      YesturdaysLog();
    } },
    { "ZLog", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ZLog();
    } },
    { "ViewNetDataLog", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ViewNetDataLog();
    } },
    { "UploadPost", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      UploadPost();
    } },
    { "cls", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      bout.cls();
    } },
    { "NetListing", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      NetListing();
    } },
    { "WHO", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      WhoIsOnline();
    } },
    { "NewMsgsAllConfs", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      // /A NewMsgsAllConfs
      NewMsgsAllConfs();
    } },
    { "MultiEMail", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      // /E "MultiEMail"
      MultiEmail();
    } },
    { "NewMsgScanFromHere", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      NewMsgScanFromHere();
    } },
    { "ValidatePosts", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ValidateScan();
    } },
    { "ChatRoom", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ChatRoom();
    } },
    { "DownloadPosts", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      DownloadPosts();
    } },
    { "DownloadFileList", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      DownloadFileList();
    } },
    { "ClearQScan", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ClearQScan();
    } },
    { "FastGoodBye", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      FastGoodBye();
    } },
    { "NewFilesAllConfs", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      NewFilesAllConfs();
    } },
    { "ReadIDZ", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ReadIDZ();
    } },
    { "UploadAllDirs", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      UploadAllDirs();
    } },
    { "UploadCurDir", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      UploadCurDir();
    } },
    { "RenameFiles", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      RenameFiles();
    } },
    { "MoveFiles", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      MoveFiles();
    } },
    { "SortDirs", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      SortDirs();
    } },
    { "ReverseSortDirs", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ReverseSort();
    } },
    { "AllowEdit", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      AllowEdit();
    } },
    { "UploadFilesBBS", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      UploadFilesBBS();
    } },
    { "DirList", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      DirList();
    } },
    { "UpDirConf", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      UpDirConf();
    } },
    { "UpDir", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      UpDir();
    } },
    { "DownDirConf", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      DownDirConf();
    } },
    { "DownDir", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      DownDir();
    } },
    { "ListUsersDL", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ListUsersDL();
    } },
    { "PrintDSZLog", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      PrintDSZLog();
    } },
    { "PrintDevices", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      PrintDevices();
    } },
    { "ViewArchive", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ViewArchive();
    } },
    { "BatchMenu", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      BatchMenu();
    } },
    { "Download", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      Download();
    } },
    { "TempExtract", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      TempExtract();
    } },
    { "FindDescription", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      FindDescription();
    } },
    { "ArchiveMenu", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      TemporaryStuff();
    } },
    { "HopDir", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      HopDir();
    } },
    { "JumpDirConf", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      JumpDirConf();
    } },
    { "ListFiles", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ListFiles();
    } },
    { "NewFileScan", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      NewFileScan();
    } },
    { "SetNewFileScanDate", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      SetNewFileScanDate();
    } },
    { "RemoveFiles", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      RemoveFiles();
    } },
    { "SearchAllFiles", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      SearchAllFiles();
    } },
    { "XferDefaults", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      XferDefaults();
    } },
    { "Upload", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      Upload();
    } },
    { "YourInfoDL", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      Upload();
    } },
    { "YourInfoDL", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      YourInfoDL();
    } },
    { "UploadToSysop", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      UploadToSysop();
    } },
    { "ReadAutoMessage", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ReadAutoMessage();
    } },
    { "SetNewScanMsg", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      SetNewScanMsg();
    } },
    { "ReadMessages", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ReadMessages();
    } },
    { "EventEdit", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      EventEdit();
    } },
    { "LoadTextFile", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      LoadTextFile();
    } },
    { "GuestApply", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      GuestApply();
    } },
    { "ConfigFileList", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ConfigFileList();
    } },
    { "ListAllColors", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      ListAllColors();
    } },
    { "RemoveNotThere", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      RemoveNotThere();
    } },
    { "AttachFile", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      AttachFile();
    } },
    { "InternetEmail", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      InternetEmail();
    } },
    { "UnQScan", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      UnQScan();
    } },
    { "Packers", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      Packers();
    } },
    { "ColorConfig", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      color_config();
    } },
    { "InitVotes", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      InitVotes();
    } },
    { "TurnMCIOn", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      TurnMCIOn();
    } },
    { "TurnMCIOff", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
      TurnMCIOff();
    } },
//    { "", [&](MenuInstanceData* pMenuData, const string& param1, const string& param2) {
//    } },
  };
}

#if defined( _MSC_VER )
#pragma warning(pop)
#endif

}  // namespace menus
}  // namespace wwiv
