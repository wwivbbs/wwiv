/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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

struct MenuItemContext {
public:
  MenuInstanceData* pMenuData;
  string param1;
  string param2;
};

map<string, std::function<void(MenuItemContext&)>, wwiv::stl::ci_less> CreateCommandMap();

bool UseNewBBSList() {
  IniFile iniFile(FilePath(application()->GetHomeDir(), WWIV_INI), INI_TAG);
  if (iniFile.IsOpen()) {
    return iniFile.GetBooleanValue("USE_NEW_BBSLIST", true);
  }
  return false;
}

void InterpretCommand(MenuInstanceData* pMenuData, const char *pszScript) {
  static map<string, std::function<void(MenuItemContext& context)>, wwiv::stl::ci_less> functions = CreateCommandMap();

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

    if (szCmd[0] == 0) { 
      break;
    }

    string cmd(szCmd);
    if (contains(functions, cmd)) {
      MenuItemContext context{ pMenuData, szParam1, szParam2 };
      functions.at(cmd)(context);
    }
  }
}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4100 )  // unreferenced formal parameter for pMenuData, param1, param2
#endif

map<string, std::function<void(MenuItemContext&)>, wwiv::stl::ci_less> CreateCommandMap() {
  return {
    { "MENU", [](MenuItemContext& context) {
      unique_ptr<MenuInstanceData> new_menu(new MenuInstanceData{});
      //context.pMenuData = new_menu.get();
      new_menu->Menus(context.pMenuData->path_, context.param1);
    } },
    { "ReturnFromMenu", [](MenuItemContext& context) {
      InterpretCommand(context.pMenuData, context.pMenuData->header.szExitScript);
      context.pMenuData->finished = true;
    } },
    { "EditMenuSet", [](MenuItemContext& context) {
      EditMenus();           // flag if we are editing this menu
      context.pMenuData->finished = true;
      context.pMenuData->reload = true;
    } },
    { "DLFreeFile", [](MenuItemContext& context) {
      char s[MAX_PATH];
      strcpy(s, context.param2.c_str());
      align(s);
      MenuDownload(context.param1.c_str(), s, true, true);
    } },
    { "DLFile", [](MenuItemContext& context) {
      char s[MAX_PATH];
      strcpy(s, context.param2.c_str());
      align(s);
      MenuDownload(context.param1.c_str(), s, false, true);
    } },
    { "RunDoor", [](MenuItemContext& context) {
      MenuRunDoorName(context.param1.c_str(), false);
    } },
    { "RunDoorFree", [](MenuItemContext& context) {
      MenuRunDoorName(context.param1.c_str(), true);
    } },
    { "RunDoorNumber", [](MenuItemContext& context) {
      int nTemp = atoi(context.param1.c_str());
      MenuRunDoorNumber(nTemp, false);
    } },
    { "RunDoorNumberFree", [](MenuItemContext& context) {
      int nTemp = atoi(context.param1.c_str());
      MenuRunDoorNumber(nTemp, true);
    } },
    { "PrintFile", [](MenuItemContext& context) {
      printfile(context.param1, true);
    } },
    { "PrintFileNA", [](MenuItemContext& context) {
      printfile(context.param1, false);
    } },
    { "SetSubNumber", [](MenuItemContext& context) {
      SetSubNumber(context.param1.c_str());
    } },
    { "SetDirNumber", [](MenuItemContext& context) {
      SetDirNumber(context.param1.c_str());
    } },
    { "SetMsgConf", [](MenuItemContext& context) {
      SetMsgConf(context.param1.c_str()[0]);
    } },
    { "SetMsgConf", [](MenuItemContext& context) {
      SetMsgConf(context.param1.c_str()[0]);
    } },
    { "SetDirConf", [](MenuItemContext& context) {
      SetDirConf(context.param1.c_str()[0]);
    } },
    { "EnableConf", [](MenuItemContext& context) {
      EnableConf();
    } },
    { "DisableConf", [](MenuItemContext& context) {
      DisableConf();
    } },
    { "Pause", [](MenuItemContext& context) {
      pausescr();
    } },
    { "ConfigUserMenuSet", [](MenuItemContext& context) {
      ConfigUserMenuSet();
      context.pMenuData->finished = true;
      context.pMenuData->reload = true;
    } },
    { "DisplayHelp", [](MenuItemContext& context) {
      if (session()->user()->IsExpert()) {
        context.pMenuData->DisplayHelp();
      }
    } },
    { "SelectSub", [](MenuItemContext& context) {
      ChangeSubNumber();
    } },
    { "SelectDir", [](MenuItemContext& context) {
      ChangeDirNumber();
    } },
    { "SubList", [](MenuItemContext& context) {
      SubList();
    } },
    { "UpSubConf", [](MenuItemContext& context) {
      UpSubConf();
    } },
    { "DownSubConf", [](MenuItemContext& context) {
      DownSubConf();
    } },
    { "UpSub", [](MenuItemContext& context) {
      UpSub();
    } },
    { "DownSub", [](MenuItemContext& context) {
      DownSub();
    } },
    { "ValidateUser", [](MenuItemContext& context) {
      ValidateUser();
    } },
    { "Doors", [](MenuItemContext& context) {
      Chains();
    } },
    { "TimeBank", [](MenuItemContext& context) {
      TimeBank();
    } },
    { "AutoMessage", [](MenuItemContext& context) {
      AutoMessage();
    } },
    { "BBSList", [](MenuItemContext& context) {
      if (UseNewBBSList()) {
        NewBBSList();
      } else {
        LegacyBBSList();
      }
    } },
    { "RequestChat", [](MenuItemContext& context) {
      RequestChat();
    } },
    { "Defaults", [](MenuItemContext& context) {
      Defaults(context.pMenuData);
    } },
    { "SendEMail", [](MenuItemContext& context) {
      SendEMail();
    } },
    { "Feedback", [](MenuItemContext& context) {
      FeedBack();
    } },
    { "Bulletins", [](MenuItemContext& context) {
      Bulletins();
    } },
    { "HopSub", [](MenuItemContext& context) {
      HopSub();
    } },
    { "SystemInfo", [](MenuItemContext& context) {
      SystemInfo();
    } },
    { "JumpSubConf", [](MenuItemContext& context) {
      JumpSubConf();
    } },
    { "KillEMail", [](MenuItemContext& context) {
      KillEMail();
    } },
    { "LastCallers", [](MenuItemContext& context) {
      LastCallers();
    } },
    { "ReadEMail", [](MenuItemContext& context) {
      ReadEMail();
    } },
    { "NewMessageScan", [](MenuItemContext& context) {
      NewMessageScan();
    } },
    { "Goodbye", [](MenuItemContext& context) {
      GoodBye();
    } },
    { "PostMessage", [](MenuItemContext& context) {
      WWIV_PostMessage();
    } },
    { "NewMsgScanCurSub", [](MenuItemContext& context) {
      ScanSub();
    } },
    { "RemovePost", [](MenuItemContext& context) {
      RemovePost();
    } },
    { "TitleScan", [](MenuItemContext& context) {
      TitleScan();
    } },
    { "ListUsers", [](MenuItemContext& context) {
      ListUsers();
    } },
    { "Vote", [](MenuItemContext& context) {
      Vote();
    } },
    { "ToggleExpert", [](MenuItemContext& context) {
      ToggleExpert();
    } },
    { "YourInfo", [](MenuItemContext& context) {
      YourInfo();
    } },
    { "ExpressScan", [](MenuItemContext& context) {
      ExpressScan();
    } },
    { "WWIVVer", [](MenuItemContext& context) {
      WWIVVersion();
    } },
    { "InstanceEdit", [](MenuItemContext& context) {
      InstanceEdit();
    } },
    { "ConferenceEdit", [](MenuItemContext& context) {
      JumpEdit();
    } },
    { "SubEdit", [](MenuItemContext& context) {
      BoardEdit();
    } },
    { "ChainEdit", [](MenuItemContext& context) {
      ChainEdit();
    } },
    { "ToggleAvailable", [](MenuItemContext& context) {
      ToggleChat();
    } },
    { "ChangeUser", [](MenuItemContext& context) {
      ChangeUser();
    } },
    { "CLOUT", [](MenuItemContext& context) {
      CallOut();
    } },
    { "Debug", [](MenuItemContext& context) {
      Debug();
    } },
    { "DirEdit", [](MenuItemContext& context) {
      DirEdit();
    } },
    { "Edit", [](MenuItemContext& context) {
      EditText();
    } },
    { "BulletinEdit", [](MenuItemContext& context) {
      EditBulletins();
    } },
    { "LoadText", [](MenuItemContext& context) {
      LoadTextFile();
    } },
    { "ReadAllMail", [](MenuItemContext& context) {
      ReadAllMail();
    } },
    { "Reboot", [](MenuItemContext& context) {
      RebootComputer();
    } },
    { "ReloadMenus", [](MenuItemContext& context) {
      ReloadMenus();
    } },
    { "ResetUserIndex", [](MenuItemContext& context) {
      ResetFiles();
    } },
    { "ResetQscan", [](MenuItemContext& context) {
      ResetQscan();
    } },
    { "MemStat", [](MenuItemContext& context) {
      MemoryStatus();
    } },
    { "PackMsgs", [](MenuItemContext& context) {
      PackMessages();
    } },
    { "VoteEdit", [](MenuItemContext& context) {
      InitVotes();
    } },
    { "Log", [](MenuItemContext& context) {
      ReadLog();
    } },
    { "NetLog", [](MenuItemContext& context) {
      ReadNetLog();
    } },
    { "Pending", [](MenuItemContext& context) {
      PrintPending();
    } },
    { "Status", [](MenuItemContext& context) {
      PrintStatus();
    } },
    { "TextEdit", [](MenuItemContext& context) {
      TextEdit();
    } },
    { "UserEdit", [](MenuItemContext& context) {
      UserEdit();
    } },
    { "VotePrint", [](MenuItemContext& context) {
      VotePrint();
    } },
    { "YLog", [](MenuItemContext& context) {
      YesturdaysLog();
    } },
    { "ZLog", [](MenuItemContext& context) {
      ZLog();
    } },
    { "ViewNetDataLog", [](MenuItemContext& context) {
      ViewNetDataLog();
    } },
    { "UploadPost", [](MenuItemContext& context) {
      UploadPost();
    } },
    { "cls", [](MenuItemContext& context) {
      bout.cls();
    } },
    { "NetListing", [](MenuItemContext& context) {
      NetListing();
    } },
    { "WHO", [](MenuItemContext& context) {
      WhoIsOnline();
    } },
    { "NewMsgsAllConfs", [](MenuItemContext& context) {
      // /A NewMsgsAllConfs
      NewMsgsAllConfs();
    } },
    { "MultiEMail", [](MenuItemContext& context) {
      // /E "MultiEMail"
      MultiEmail();
    } },
    { "NewMsgScanFromHere", [](MenuItemContext& context) {
      NewMsgScanFromHere();
    } },
    { "ValidatePosts", [](MenuItemContext& context) {
      ValidateScan();
    } },
    { "ChatRoom", [](MenuItemContext& context) {
      ChatRoom();
    } },
    { "DownloadPosts", [](MenuItemContext& context) {
      DownloadPosts();
    } },
    { "DownloadFileList", [](MenuItemContext& context) {
      DownloadFileList();
    } },
    { "ClearQScan", [](MenuItemContext& context) {
      ClearQScan();
    } },
    { "FastGoodBye", [](MenuItemContext& context) {
      FastGoodBye();
    } },
    { "NewFilesAllConfs", [](MenuItemContext& context) {
      NewFilesAllConfs();
    } },
    { "ReadIDZ", [](MenuItemContext& context) {
      ReadIDZ();
    } },
    { "UploadAllDirs", [](MenuItemContext& context) {
      UploadAllDirs();
    } },
    { "UploadCurDir", [](MenuItemContext& context) {
      UploadCurDir();
    } },
    { "RenameFiles", [](MenuItemContext& context) {
      RenameFiles();
    } },
    { "MoveFiles", [](MenuItemContext& context) {
      MoveFiles();
    } },
    { "SortDirs", [](MenuItemContext& context) {
      SortDirs();
    } },
    { "ReverseSortDirs", [](MenuItemContext& context) {
      ReverseSort();
    } },
    { "AllowEdit", [](MenuItemContext& context) {
      AllowEdit();
    } },
    { "UploadFilesBBS", [](MenuItemContext& context) {
      UploadFilesBBS();
    } },
    { "DirList", [](MenuItemContext& context) {
      DirList();
    } },
    { "UpDirConf", [](MenuItemContext& context) {
      UpDirConf();
    } },
    { "UpDir", [](MenuItemContext& context) {
      UpDir();
    } },
    { "DownDirConf", [](MenuItemContext& context) {
      DownDirConf();
    } },
    { "DownDir", [](MenuItemContext& context) {
      DownDir();
    } },
    { "ListUsersDL", [](MenuItemContext& context) {
      ListUsersDL();
    } },
    { "PrintDSZLog", [](MenuItemContext& context) {
      PrintDSZLog();
    } },
    { "PrintDevices", [](MenuItemContext& context) {
      PrintDevices();
    } },
    { "ViewArchive", [](MenuItemContext& context) {
      ViewArchive();
    } },
    { "BatchMenu", [](MenuItemContext& context) {
      BatchMenu();
    } },
    { "Download", [](MenuItemContext& context) {
      Download();
    } },
    { "TempExtract", [](MenuItemContext& context) {
      TempExtract();
    } },
    { "FindDescription", [](MenuItemContext& context) {
      FindDescription();
    } },
    { "ArchiveMenu", [](MenuItemContext& context) {
      TemporaryStuff();
    } },
    { "HopDir", [](MenuItemContext& context) {
      HopDir();
    } },
    { "JumpDirConf", [](MenuItemContext& context) {
      JumpDirConf();
    } },
    { "ListFiles", [](MenuItemContext& context) {
      ListFiles();
    } },
    { "NewFileScan", [](MenuItemContext& context) {
      NewFileScan();
    } },
    { "SetNewFileScanDate", [](MenuItemContext& context) {
      SetNewFileScanDate();
    } },
    { "RemoveFiles", [](MenuItemContext& context) {
      RemoveFiles();
    } },
    { "SearchAllFiles", [](MenuItemContext& context) {
      SearchAllFiles();
    } },
    { "XferDefaults", [](MenuItemContext& context) {
      XferDefaults();
    } },
    { "Upload", [](MenuItemContext& context) {
      Upload();
    } },
    { "YourInfoDL", [](MenuItemContext& context) {
      Upload();
    } },
    { "YourInfoDL", [](MenuItemContext& context) {
      YourInfoDL();
    } },
    { "UploadToSysop", [](MenuItemContext& context) {
      UploadToSysop();
    } },
    { "ReadAutoMessage", [](MenuItemContext& context) {
      ReadAutoMessage();
    } },
    { "SetNewScanMsg", [](MenuItemContext& context) {
      SetNewScanMsg();
    } },
    { "ReadMessages", [](MenuItemContext& context) {
      ReadMessages();
    } },
    { "EventEdit", [](MenuItemContext& context) {
      EventEdit();
    } },
    { "LoadTextFile", [](MenuItemContext& context) {
      LoadTextFile();
    } },
    { "GuestApply", [](MenuItemContext& context) {
      GuestApply();
    } },
    { "ConfigFileList", [](MenuItemContext& context) {
      ConfigFileList();
    } },
    { "ListAllColors", [](MenuItemContext& context) {
      ListAllColors();
    } },
    { "RemoveNotThere", [](MenuItemContext& context) {
      RemoveNotThere();
    } },
    { "AttachFile", [](MenuItemContext& context) {
      AttachFile();
    } },
    { "InternetEmail", [](MenuItemContext& context) {
      InternetEmail();
    } },
    { "UnQScan", [](MenuItemContext& context) {
      UnQScan();
    } },
    { "Packers", [](MenuItemContext& context) {
      Packers();
    } },
    { "ColorConfig", [](MenuItemContext& context) {
      color_config();
    } },
    { "InitVotes", [](MenuItemContext& context) {
      InitVotes();
    } },
    { "TurnMCIOn", [](MenuItemContext& context) {
      TurnMCIOn();
    } },
    { "TurnMCIOff", [](MenuItemContext& context) {
      TurnMCIOff();
    } },
//    { "", [](MenuItemContext& context) {
//    } },
  };
}

#if defined( _MSC_VER )
#pragma warning(pop)
#endif

}  // namespace menus
}  // namespace wwiv
