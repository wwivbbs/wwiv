/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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

#include "bbs/basic.h"
#include "bbs/bbs.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsovl3.h"
#include "bbs/bbsutl.h"
#include "bbs/hop.h"
#include "bbs/menu.h"
#include "bbs/menuspec.h"
#include "bbs/menusupp.h"
#include "bbs/menu_parser.h"
#include "bbs/misccmd.h"
#include "bbs/new_bbslist.h"
#include "bbs/syschat.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/shortmsg.h"
#include "bbs/sr.h"
#include "bbs/subacc.h"
#include "bbs/sublist.h"
#include "bbs/sysopf.h"
#include "bbs/utility.h"

#include "bbs/wqscn.h"
#include "bbs/xfer.h"
#include "bbs/xferovl.h"
#include "bbs/xferovl1.h"
#include "bbs/xfertmp.h"
#include "core/inifile.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/filenames.h"

using std::map;
using std::string;
using std::unique_ptr;

using wwiv::core::IniFile;
using wwiv::bbslist::NewBBSList;

using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv {
namespace menus {

struct MenuItemContext {
public:
  MenuItemContext(MenuInstance* m, const string& p1, const string& p2)
    : pMenuData(m), param1(p1), param2(p2) {}
  // May be null if not invoked from an actual menu.
  MenuInstance* pMenuData;
  string param1;
  string param2;
  bool finished = false;
  bool need_reload = false;
};

map<string, std::function<void(MenuItemContext&)>, wwiv::stl::ci_less> CreateCommandMap();

void InterpretCommand(MenuInstance* menudata, const std::string& script) {
  static map<string, std::function<void(MenuItemContext& context)>, wwiv::stl::ci_less> functions = CreateCommandMap();

  if (script.empty()) {
    return;
  }

  char temp_script[255];
  to_char_array(temp_script, script);

  const char* p = temp_script;
  while (p) {
    char scmd[31], param1[51], param2[51];
    p = MenuParseLine(p, scmd, param1, param2);

    if (scmd[0] == 0) { 
      break;
    }

    string cmd(scmd);
    if (contains(functions, cmd)) {
      MenuItemContext context(menudata, param1, param2);
      functions.at(cmd)(context);
      if (menudata) {
        menudata->reload = context.need_reload;
        menudata->finished = (context.finished || context.need_reload);
      }
    }
  }
}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4100 )  // unreferenced formal parameter for menudata, param1, param2
#endif

map<string, std::function<void(MenuItemContext&)>, wwiv::stl::ci_less> CreateCommandMap() {
  return {
    { "MENU", [](MenuItemContext& context) {
      if (context.pMenuData) {
        MenuInstance new_menu(context.pMenuData->menu_directory(), context.param1);
        new_menu.RunMenu();
      }
    } },
    { "ReturnFromMenu", [](MenuItemContext& context) {
      if (context.pMenuData) {
        InterpretCommand(context.pMenuData, context.pMenuData->header.szExitScript);
        context.finished = true;
      }
    } },
    { "DLFreeFile", [](MenuItemContext& context) {
      const auto s = aligns(context.param2);
      MenuDownload(context.param1, s, true, true);
    } },
    { "DLFile", [](MenuItemContext& context) {
      const auto s = aligns(context.param2);
      MenuDownload(context.param1, s, false, true);
    } },
    { "RunDoor", [](MenuItemContext& context) {
      MenuRunDoorName(context.param1.c_str(), false);
    } },
    { "RunDoorFree", [](MenuItemContext& context) {
      MenuRunDoorName(context.param1.c_str(), true);
    } },
    { "RunDoorNumber", [](MenuItemContext& context) {
      auto t = to_number<int>(context.param1.c_str());
      MenuRunDoorNumber(t, false);
    } },
    { "RunDoorNumberFree", [](MenuItemContext& context) {
      auto t = to_number<int>(context.param1.c_str());
      MenuRunDoorNumber(t, true);
    } },
    { "RunBasic", [](MenuItemContext& context) {
      // Runs a basic script from GFILES/
      wwiv::bbs::RunBasicScript(context.param1);
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
      context.need_reload = true;
    } },
    { "DisplayHelp", [](MenuItemContext& context) {
      if (context.pMenuData && a()->user()->IsExpert()) {
        context.pMenuData->DisplayMenu();
      }
    } },
    {"DisplayMenu", [](MenuItemContext& context) {
      if (context.pMenuData && a()->user()->IsExpert()) {
        context.pMenuData->DisplayMenu();
      }
    }},
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
      NewBBSList();
    } },
    { "RequestChat", [](MenuItemContext& context) {
      RequestChat();
    } },
    { "Defaults", [](MenuItemContext& context) {
      Defaults(context.need_reload);
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
    { "WWIVVer", [](MenuItemContext& context) {
      WWIVVersion();
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
    { "ReloadMenus", [](MenuItemContext& context) {
      ReloadMenus();
    } },
    { "ResetQscan", [](MenuItemContext& context) {
      ResetQscan();
    } },
    { "MemStat", [](MenuItemContext& context) {
      MemoryStatus();
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
    { "VotePrint", [](MenuItemContext& context) {
      VotePrint();
    } },
    { "YLog", [](MenuItemContext& context) {
      YesterdaysLog();
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
      print_net_listing(false);
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
