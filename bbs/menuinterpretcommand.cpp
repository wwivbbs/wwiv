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
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/sublist.h"
#include "bbs/syschat.h"
#include "bbs/sysopf.h"
#include "bbs/xfer.h"
#include "bbs/xferovl1.h"
#include "core/stl.h"
#include "core/strings.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

using std::map;
using std::string;
using std::unique_ptr;
using wwiv::core::IniFile;
using wwiv::bbslist::NewBBSList;

using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv::menus {

struct MenuItemContext {
  MenuItemContext(MenuInstance* m, std::string p1, std::string p2)
    : pMenuData(m), param1(std::move(p1)), param2(std::move(p2)), p2_(p2) {}
  // May be null if not invoked from an actual menu.
  MenuInstance* pMenuData;
  string param1;
  string param2;
  bool finished{false};
  bool need_reload{false};
  const std::string& p2_;
};

map<string, std::function<void(MenuItemContext&)>, wwiv::stl::ci_less> CreateCommandMap();

void InterpretCommand(MenuInstance* menudata, const std::string& script) {
  static auto functions = CreateCommandMap();

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

map<string, std::function<void(MenuItemContext&)>, ci_less> CreateCommandMap() {
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
      auto t = to_number<int>(context.param1);
      MenuRunDoorNumber(t, false);
    } },
    { "RunDoorNumberFree", [](MenuItemContext& context) {
      auto t = to_number<int>(context.param1);
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
    { "EnableConf", [](MenuItemContext&) {
      EnableConf();
    } },
    { "DisableConf", [](MenuItemContext&) {
      DisableConf();
    } },
    { "Pause", [](MenuItemContext&) {
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
    { "SelectSub", [](MenuItemContext&) {
      ChangeSubNumber();
    } },
    { "SelectDir", [](MenuItemContext&) {
      ChangeDirNumber();
    } },
    { "SubList", [](MenuItemContext&) {
      SubList();
    } },
    { "UpSubConf", [](MenuItemContext&) {
      UpSubConf();
    } },
    { "DownSubConf", [](MenuItemContext&) {
      DownSubConf();
    } },
    { "UpSub", [](MenuItemContext&) {
      UpSub();
    } },
    { "DownSub", [](MenuItemContext&) {
      DownSub();
    } },
    { "ValidateUser", [](MenuItemContext&) {
      ValidateUser();
    } },
    { "Doors", [](MenuItemContext&) {
      Chains();
    } },
    { "TimeBank", [](MenuItemContext&) {
      TimeBank();
    } },
    { "AutoMessage", [](MenuItemContext&) {
      AutoMessage();
    } },
    { "BBSList", [](MenuItemContext&) {
      NewBBSList();
    } },
    { "RequestChat", [](MenuItemContext&) {
      RequestChat();
    } },
    { "Defaults", [](MenuItemContext& context) {
      Defaults(context.need_reload);
    } },
    { "SendEMail", [](MenuItemContext&) {
      SendEMail();
    } },
    { "Feedback", [](MenuItemContext&) {
      FeedBack();
    } },
    { "Bulletins", [](MenuItemContext&) {
      Bulletins();
    } },
    { "HopSub", [](MenuItemContext&) {
      HopSub();
    } },
    { "SystemInfo", [](MenuItemContext&) {
      SystemInfo();
    } },
    { "JumpSubConf", [](MenuItemContext&) {
      JumpSubConf();
    } },
    { "KillEMail", [](MenuItemContext&) {
      KillEMail();
    } },
    { "LastCallers", [](MenuItemContext&) {
      LastCallers();
    } },
    { "ReadEMail", [](MenuItemContext&) {
      ReadEMail();
    } },
    { "NewMessageScan", [](MenuItemContext&) {
      NewMessageScan();
    } },
    { "Goodbye", [](MenuItemContext&) {
      GoodBye();
    } },
    { "PostMessage", [](MenuItemContext&) {
      WWIV_PostMessage();
    } },
    { "NewMsgScanCurSub", [](MenuItemContext&) {
      ScanSub();
    } },
    { "RemovePost", [](MenuItemContext&) {
      RemovePost();
    } },
    { "TitleScan", [](MenuItemContext&) {
      TitleScan();
    } },
    { "ListUsers", [](MenuItemContext&) {
      ListUsers();
    } },
    { "Vote", [](MenuItemContext&) {
      Vote();
    } },
    { "ToggleExpert", [](MenuItemContext&) {
      ToggleExpert();
    } },
    { "YourInfo", [](MenuItemContext&) {
      YourInfo();
    } },
    { "WWIVVer", [](MenuItemContext&) {
      WWIVVersion();
    } },
    { "ConferenceEdit", [](MenuItemContext&) {
      JumpEdit();
    } },
    { "SubEdit", [](MenuItemContext&) {
      BoardEdit();
    } },
    { "ChainEdit", [](MenuItemContext&) {
      ChainEdit();
    } },
    { "ToggleAvailable", [](MenuItemContext&) {
      ToggleChat();
    } },
    { "ChangeUser", [](MenuItemContext&) {
      ChangeUser();
    } },
    { "DirEdit", [](MenuItemContext&) {
      DirEdit();
    } },
    { "Edit", [](MenuItemContext&) {
      EditText();
    } },
    { "BulletinEdit", [](MenuItemContext&) {
      EditBulletins();
    } },
    { "LoadText", [](MenuItemContext&) {
      LoadTextFile();
    } },
    { "ReadAllMail", [](MenuItemContext&) {
      ReadAllMail();
    } },
    { "ReloadMenus", [](MenuItemContext&) {
      ReloadMenus();
    } },
    { "ResetQscan", [](MenuItemContext&) {
      ResetQscan();
    } },
    { "MemStat", [](MenuItemContext&) {
      MemoryStatus();
    } },
    { "VoteEdit", [](MenuItemContext&) {
      InitVotes();
    } },
    { "Log", [](MenuItemContext&) {
      ReadLog();
    } },
    { "NetLog", [](MenuItemContext&) {
      ReadNetLog();
    } },
    { "Pending", [](MenuItemContext&) {
      PrintPending();
    } },
    { "Status", [](MenuItemContext&) {
      PrintStatus();
    } },
    { "TextEdit", [](MenuItemContext&) {
      TextEdit();
    } },
    { "VotePrint", [](MenuItemContext&) {
      VotePrint();
    } },
    { "YLog", [](MenuItemContext&) {
      YesterdaysLog();
    } },
    { "ZLog", [](MenuItemContext&) {
      ZLog();
    } },
    { "ViewNetDataLog", [](MenuItemContext&) {
      ViewNetDataLog();
    } },
    { "UploadPost", [](MenuItemContext&) {
      UploadPost();
    } },
    { "cls", [](MenuItemContext&) {
      bout.cls();
    } },
    { "NetListing", [](MenuItemContext&) {
      print_net_listing(false);
    } },
    { "WHO", [](MenuItemContext&) {
      WhoIsOnline();
    } },
    { "NewMsgsAllConfs", [](MenuItemContext&) {
      // /A NewMsgsAllConfs
      NewMsgsAllConfs();
    } },
    { "MultiEMail", [](MenuItemContext&) {
      // /E "MultiEMail"
      MultiEmail();
    } },
    { "NewMsgScanFromHere", [](MenuItemContext&) {
      NewMsgScanFromHere();
    } },
    { "ValidatePosts", [](MenuItemContext&) {
      ValidateScan();
    } },
    { "ChatRoom", [](MenuItemContext&) {
      ChatRoom();
    } },
    { "ClearQScan", [](MenuItemContext&) {
      ClearQScan();
    } },
    { "FastGoodBye", [](MenuItemContext&) {
      FastGoodBye();
    } },
    { "NewFilesAllConfs", [](MenuItemContext&) {
      NewFilesAllConfs();
    } },
    { "ReadIDZ", [](MenuItemContext&) {
      ReadIDZ();
    } },
    { "UploadAllDirs", [](MenuItemContext&) {
      UploadAllDirs();
    } },
    { "UploadCurDir", [](MenuItemContext&) {
      UploadCurDir();
    } },
    { "RenameFiles", [](MenuItemContext&) {
      RenameFiles();
    } },
    { "MoveFiles", [](MenuItemContext&) {
      MoveFiles();
    } },
    { "SortDirs", [](MenuItemContext&) {
      SortDirs();
    } },
    { "ReverseSortDirs", [](MenuItemContext&) {
      ReverseSort();
    } },
    { "AllowEdit", [](MenuItemContext&) {
      AllowEdit();
    } },
    { "UploadFilesBBS", [](MenuItemContext&) {
      UploadFilesBBS();
    } },
    { "DirList", [](MenuItemContext&) {
      DirList();
    } },
    { "UpDirConf", [](MenuItemContext&) {
      UpDirConf();
    } },
    { "UpDir", [](MenuItemContext&) {
      UpDir();
    } },
    { "DownDirConf", [](MenuItemContext&) {
      DownDirConf();
    } },
    { "DownDir", [](MenuItemContext&) {
      DownDir();
    } },
    { "ListUsersDL", [](MenuItemContext&) {
      ListUsersDL();
    } },
    { "PrintDSZLog", [](MenuItemContext&) {
      PrintDSZLog();
    } },
    { "PrintDevices", [](MenuItemContext&) {
      PrintDevices();
    } },
    { "ViewArchive", [](MenuItemContext&) {
      ViewArchive();
    } },
    { "BatchMenu", [](MenuItemContext&) {
      BatchMenu();
    } },
    { "Download", [](MenuItemContext&) {
      Download();
    } },
    { "TempExtract", [](MenuItemContext&) {
      TempExtract();
    } },
    { "FindDescription", [](MenuItemContext&) {
      FindDescription();
    } },
    { "ArchiveMenu", [](MenuItemContext&) {
      TemporaryStuff();
    } },
    { "HopDir", [](MenuItemContext&) {
      HopDir();
    } },
    { "JumpDirConf", [](MenuItemContext&) {
      JumpDirConf();
    } },
    { "ListFiles", [](MenuItemContext&) {
      ListFiles();
    } },
    { "NewFileScan", [](MenuItemContext&) {
      NewFileScan();
    } },
    { "SetNewFileScanDate", [](MenuItemContext&) {
      SetNewFileScanDate();
    } },
    { "RemoveFiles", [](MenuItemContext&) {
      RemoveFiles();
    } },
    { "SearchAllFiles", [](MenuItemContext&) {
      SearchAllFiles();
    } },
    { "XferDefaults", [](MenuItemContext&) {
      XferDefaults();
    } },
    { "Upload", [](MenuItemContext&) {
      Upload();
    } },
    { "YourInfoDL", [](MenuItemContext&) {
      Upload();
    } },
    { "YourInfoDL", [](MenuItemContext&) {
      YourInfoDL();
    } },
    { "UploadToSysop", [](MenuItemContext&) {
      UploadToSysop();
    } },
    { "ReadAutoMessage", [](MenuItemContext&) {
      ReadAutoMessage();
    } },
    { "SetNewScanMsg", [](MenuItemContext&) {
      SetNewScanMsg();
    } },
    { "LoadTextFile", [](MenuItemContext&) {
      LoadTextFile();
    } },
    { "GuestApply", [](MenuItemContext&) {
      GuestApply();
    } },
    { "ConfigFileList", [](MenuItemContext&) {
      ConfigFileList();
    } },
    { "ListAllColors", [](MenuItemContext&) {
      ListAllColors();
    } },
    { "RemoveNotThere", [](MenuItemContext&) {
      RemoveNotThere();
    } },
    { "AttachFile", [](MenuItemContext&) {
      AttachFile();
    } },
    { "InternetEmail", [](MenuItemContext&) {
      InternetEmail();
    } },
    { "UnQScan", [](MenuItemContext&) {
      UnQScan();
    } },
    { "Packers", [](MenuItemContext&) {
      Packers();
    } },
    { "InitVotes", [](MenuItemContext&) {
      InitVotes();
    } },
    { "TurnMCIOn", [](MenuItemContext&) {
      TurnMCIOn();
    } },
    { "TurnMCIOff", [](MenuItemContext&) {
      TurnMCIOff();
    } },
//    { "", [](MenuItemContext& context) {
//    } },
  };
}

}
