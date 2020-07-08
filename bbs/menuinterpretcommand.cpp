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

struct MenuItem {
  MenuItem(std::string desc, std::function<void(MenuItemContext&)> f)
      : description_(std::move(desc)), f_(std::move(f)) {}
  MenuItem(std::function<void(MenuItemContext&)> f)
      : description_(""), f_(std::move(f)) {}

  std::string description_;
  std::function<void(MenuItemContext&)> f_;

};

map<string, MenuItem, wwiv::stl::ci_less> CreateCommandMap();

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
      functions.at(cmd).f_(context);
      if (menudata) {
        menudata->reload = context.need_reload;
        menudata->finished = (context.finished || context.need_reload);
      }
    }
  }
}

map<string, MenuItem, ci_less> CreateCommandMap() {
  return {
    { "MENU", MenuItem([](MenuItemContext& context) {
      if (context.pMenuData) {
        MenuInstance new_menu(context.pMenuData->menu_directory(), context.param1);
        new_menu.RunMenu();
      }
    } ) },
    { "ReturnFromMenu", MenuItem([](MenuItemContext& context) {
      if (context.pMenuData) {
        InterpretCommand(context.pMenuData, context.pMenuData->header.szExitScript);
        context.finished = true;
      }
    } ) },
    { "DLFreeFile", MenuItem([](MenuItemContext& context) {
      const auto s = aligns(context.param2);
      MenuDownload(context.param1, s, true, true);
    } ) },
    { "DLFile", MenuItem([](MenuItemContext& context) {
      const auto s = aligns(context.param2);
      MenuDownload(context.param1, s, false, true);
    } ) },
    { "RunDoor", MenuItem([](MenuItemContext& context) {
      MenuRunDoorName(context.param1.c_str(), false);
    } ) },
    { "RunDoorFree", MenuItem([](MenuItemContext& context) {
      MenuRunDoorName(context.param1.c_str(), true);
    } ) },
    { "RunDoorNumber", MenuItem([](MenuItemContext& context) {
      auto t = to_number<int>(context.param1);
      MenuRunDoorNumber(t, false);
    } ) },
    { "RunDoorNumberFree", MenuItem([](MenuItemContext& context) {
      auto t = to_number<int>(context.param1);
      MenuRunDoorNumber(t, true);
    } ) },
    { "RunBasic", MenuItem([](MenuItemContext& context) {
      // Runs a basic script from GFILES/
      wwiv::bbs::RunBasicScript(context.param1);
    } ) },
    { "PrintFile", MenuItem([](MenuItemContext& context) {
      printfile(context.param1, true);
    } ) },
    { "PrintFileNA", MenuItem([](MenuItemContext& context) {
      printfile(context.param1, false);
    } ) },
    { "SetSubNumber", MenuItem([](MenuItemContext& context) {
      SetSubNumber(context.param1.c_str());
    } ) },
    { "SetDirNumber", MenuItem([](MenuItemContext& context) {
      SetDirNumber(context.param1.c_str());
    } ) },
    { "SetMsgConf", MenuItem([](MenuItemContext& context) {
      SetMsgConf(context.param1.c_str()[0]);
    } ) },
    { "SetMsgConf", MenuItem([](MenuItemContext& context) {
      SetMsgConf(context.param1.c_str()[0]);
    } ) },
    { "SetDirConf", MenuItem([](MenuItemContext& context) {
      SetDirConf(context.param1.c_str()[0]);
    } ) },
    { "EnableConf", MenuItem([](MenuItemContext&) {
      EnableConf();
    } ) },
    { "DisableConf", MenuItem([](MenuItemContext&) {
      DisableConf();
    } ) },
    { "Pause", MenuItem([](MenuItemContext&) {
      pausescr();
    } ) },
    { "ConfigUserMenuSet", MenuItem([](MenuItemContext& context) {
      ConfigUserMenuSet();
      context.need_reload = true;
    } ) },
    { "DisplayHelp", MenuItem([](MenuItemContext& context) {
      if (context.pMenuData && a()->user()->IsExpert()) {
        context.pMenuData->DisplayMenu();
      }
    } ) },
    {"DisplayMenu", MenuItem([](MenuItemContext& context) {
      if (context.pMenuData && a()->user()->IsExpert()) {
        context.pMenuData->DisplayMenu();
      }
    } ) },
    { "SelectSub", MenuItem([](MenuItemContext&) {
      ChangeSubNumber();
    } ) },
    { "SelectDir", MenuItem([](MenuItemContext&) {
      ChangeDirNumber();
    } ) },
    { "SubList", MenuItem([](MenuItemContext&) {
      SubList();
    } ) },
    { "UpSubConf", MenuItem([](MenuItemContext&) {
      UpSubConf();
    } ) },
    { "DownSubConf", MenuItem([](MenuItemContext&) {
      DownSubConf();
    } ) },
    { "UpSub", MenuItem([](MenuItemContext&) {
      UpSub();
    } ) },
    { "DownSub", MenuItem([](MenuItemContext&) {
      DownSub();
    } ) },
    { "ValidateUser", MenuItem([](MenuItemContext&) {
      ValidateUser();
    } ) },
    { "Doors", MenuItem([](MenuItemContext&) {
      Chains();
    } ) },
    { "TimeBank", MenuItem([](MenuItemContext&) {
      TimeBank();
    } ) },
    { "AutoMessage", MenuItem([](MenuItemContext&) {
      AutoMessage();
    } ) },
    { "BBSList", MenuItem([](MenuItemContext&) {
      NewBBSList();
    } ) },
    { "RequestChat", MenuItem([](MenuItemContext&) {
      RequestChat();
    } ) },
    { "Defaults", MenuItem([](MenuItemContext& context) {
      Defaults(context.need_reload);
    } ) },
    { "SendEMail", MenuItem([](MenuItemContext&) {
      SendEMail();
    } ) },
    { "Feedback", MenuItem([](MenuItemContext&) {
      FeedBack();
    } ) },
    { "Bulletins", MenuItem([](MenuItemContext&) {
      Bulletins();
    } ) },
    { "HopSub", MenuItem([](MenuItemContext&) {
      HopSub();
    } ) },
    { "SystemInfo", MenuItem([](MenuItemContext&) {
      SystemInfo();
    } ) },
    { "JumpSubConf", MenuItem([](MenuItemContext&) {
      JumpSubConf();
    } ) },
    { "KillEMail", MenuItem([](MenuItemContext&) {
      KillEMail();
    } ) },
    { "LastCallers", MenuItem([](MenuItemContext&) {
      LastCallers();
    } ) },
    { "ReadEMail", MenuItem([](MenuItemContext&) {
      ReadEMail();
    } ) },
    { "NewMessageScan", MenuItem([](MenuItemContext&) {
      NewMessageScan();
    } ) },
    { "Goodbye", MenuItem([](MenuItemContext&) {
      GoodBye();
    } ) },
    { "PostMessage", MenuItem([](MenuItemContext&) {
      WWIV_PostMessage();
    } ) },
    { "NewMsgScanCurSub", MenuItem([](MenuItemContext&) {
      ScanSub();
    } ) },
    { "RemovePost", MenuItem([](MenuItemContext&) {
      RemovePost();
    } ) },
    { "TitleScan", MenuItem([](MenuItemContext&) {
      TitleScan();
    } ) },
    { "ListUsers", MenuItem([](MenuItemContext&) {
      ListUsers();
    } ) },
    { "Vote", MenuItem([](MenuItemContext&) {
      Vote();
    } ) },
    { "ToggleExpert", MenuItem([](MenuItemContext&) {
      ToggleExpert();
    } ) },
    { "YourInfo", MenuItem([](MenuItemContext&) {
      YourInfo();
    } ) },
    { "WWIVVer", MenuItem([](MenuItemContext&) {
      WWIVVersion();
    } ) },
    { "ConferenceEdit", MenuItem([](MenuItemContext&) {
      JumpEdit();
    } ) },
    { "SubEdit", MenuItem([](MenuItemContext&) {
      BoardEdit();
    } ) },
    { "ChainEdit", MenuItem([](MenuItemContext&) {
      ChainEdit();
    } ) },
    { "ToggleAvailable", MenuItem([](MenuItemContext&) {
      ToggleChat();
    } ) },
    { "ChangeUser", MenuItem([](MenuItemContext&) {
      ChangeUser();
    } ) },
    { "DirEdit", MenuItem([](MenuItemContext&) {
      DirEdit();
    } ) },
    { "Edit", MenuItem([](MenuItemContext&) {
      EditText();
    } ) },
    { "BulletinEdit", MenuItem([](MenuItemContext&) {
      EditBulletins();
    } ) },
    { "LoadText", MenuItem([](MenuItemContext&) {
      LoadTextFile();
    } ) },
    { "ReadAllMail", MenuItem([](MenuItemContext&) {
      ReadAllMail();
    } ) },
    { "ReloadMenus", MenuItem([](MenuItemContext&) {
      ReloadMenus();
    } ) },
    { "ResetQscan", MenuItem([](MenuItemContext&) {
      ResetQscan();
    } ) },
    { "MemStat", MenuItem([](MenuItemContext&) {
      MemoryStatus();
    } ) },
    { "VoteEdit", MenuItem([](MenuItemContext&) {
      InitVotes();
    } ) },
    { "Log", MenuItem([](MenuItemContext&) {
      ReadLog();
    } ) },
    { "NetLog", MenuItem([](MenuItemContext&) {
      ReadNetLog();
    } ) },
    { "Pending", MenuItem([](MenuItemContext&) {
      PrintPending();
    } ) },
    { "Status", MenuItem([](MenuItemContext&) {
      PrintStatus();
    } ) },
    { "TextEdit", MenuItem([](MenuItemContext&) {
      TextEdit();
    } ) },
    { "VotePrint", MenuItem([](MenuItemContext&) {
      VotePrint();
    } ) },
    { "YLog", MenuItem([](MenuItemContext&) {
      YesterdaysLog();
    } ) },
    { "ZLog", MenuItem([](MenuItemContext&) {
      ZLog();
    } ) },
    { "ViewNetDataLog", MenuItem([](MenuItemContext&) {
      ViewNetDataLog();
    } ) },
    { "UploadPost", MenuItem([](MenuItemContext&) {
      UploadPost();
    } ) },
    { "cls", MenuItem([](MenuItemContext&) {
      bout.cls();
    } ) },
    { "NetListing", MenuItem([](MenuItemContext&) {
      print_net_listing(false);
    } ) },
    { "WHO", MenuItem([](MenuItemContext&) {
      WhoIsOnline();
    } ) },
    { "NewMsgsAllConfs", MenuItem([](MenuItemContext&) {
      // /A NewMsgsAllConfs
      NewMsgsAllConfs();
    } ) },
    { "MultiEMail", MenuItem([](MenuItemContext&) {
      // /E "MultiEMail"
      MultiEmail();
    } ) },
    { "NewMsgScanFromHere", MenuItem([](MenuItemContext&) {
      NewMsgScanFromHere();
    } ) },
    { "ValidatePosts", MenuItem([](MenuItemContext&) {
      ValidateScan();
    } ) },
    { "ChatRoom", MenuItem([](MenuItemContext&) {
      ChatRoom();
    } ) },
    { "ClearQScan", MenuItem([](MenuItemContext&) {
      ClearQScan();
    } ) },
    { "FastGoodBye", MenuItem([](MenuItemContext&) {
      FastGoodBye();
    } ) },
    { "NewFilesAllConfs", MenuItem([](MenuItemContext&) {
      NewFilesAllConfs();
    } ) },
    { "ReadIDZ", MenuItem([](MenuItemContext&) {
      ReadIDZ();
    } ) },
    { "UploadAllDirs", MenuItem([](MenuItemContext&) {
      UploadAllDirs();
    } ) },
    { "UploadCurDir", MenuItem([](MenuItemContext&) {
      UploadCurDir();
    } ) },
    { "RenameFiles", MenuItem([](MenuItemContext&) {
      RenameFiles();
    } ) },
    { "MoveFiles", MenuItem([](MenuItemContext&) {
      MoveFiles();
    } ) },
    { "SortDirs", MenuItem([](MenuItemContext&) {
      SortDirs();
    } ) },
    { "ReverseSortDirs", MenuItem([](MenuItemContext&) {
      ReverseSort();
    } ) },
    { "AllowEdit", MenuItem([](MenuItemContext&) {
      AllowEdit();
    } ) },
    { "UploadFilesBBS", MenuItem([](MenuItemContext&) {
      UploadFilesBBS();
    } ) },
    { "DirList", MenuItem([](MenuItemContext&) {
      DirList();
    } ) },
    { "UpDirConf", MenuItem([](MenuItemContext&) {
      UpDirConf();
    } ) },
    { "UpDir", MenuItem([](MenuItemContext&) {
      UpDir();
    } ) },
    { "DownDirConf", MenuItem([](MenuItemContext&) {
      DownDirConf();
    } ) },
    { "DownDir", MenuItem([](MenuItemContext&) {
      DownDir();
    } ) },
    { "ListUsersDL", MenuItem([](MenuItemContext&) {
      ListUsersDL();
    } ) },
    { "PrintDSZLog", MenuItem([](MenuItemContext&) {
      PrintDSZLog();
    } ) },
    { "PrintDevices", MenuItem([](MenuItemContext&) {
      PrintDevices();
    } ) },
    { "ViewArchive", MenuItem([](MenuItemContext&) {
      ViewArchive();
    } ) },
    { "BatchMenu", MenuItem([](MenuItemContext&) {
      BatchMenu();
    } ) },
    { "Download", MenuItem([](MenuItemContext&) {
      Download();
    } ) },
    { "TempExtract", MenuItem([](MenuItemContext&) {
      TempExtract();
    } ) },
    { "FindDescription", MenuItem([](MenuItemContext&) {
      FindDescription();
    } ) },
    { "ArchiveMenu", MenuItem([](MenuItemContext&) {
      TemporaryStuff();
    } ) },
    { "HopDir", MenuItem([](MenuItemContext&) {
      HopDir();
    } ) },
    { "JumpDirConf", MenuItem([](MenuItemContext&) {
      JumpDirConf();
    } ) },
    { "ListFiles", MenuItem([](MenuItemContext&) {
      ListFiles();
    } ) },
    { "NewFileScan", MenuItem([](MenuItemContext&) {
      NewFileScan();
    } ) },
    { "SetNewFileScanDate", MenuItem([](MenuItemContext&) {
      SetNewFileScanDate();
    } ) },
    { "RemoveFiles", MenuItem([](MenuItemContext&) {
      RemoveFiles();
    } ) },
    { "SearchAllFiles", MenuItem([](MenuItemContext&) {
      SearchAllFiles();
    } ) },
    { "XferDefaults", MenuItem([](MenuItemContext&) {
      XferDefaults();
    } ) },
    { "Upload", MenuItem([](MenuItemContext&) {
      Upload();
    } ) },
    { "YourInfoDL", MenuItem([](MenuItemContext&) {
      Upload();
    } ) },
    { "YourInfoDL", MenuItem([](MenuItemContext&) {
      YourInfoDL();
    } ) },
    { "UploadToSysop", MenuItem([](MenuItemContext&) {
      UploadToSysop();
    } ) },
    { "ReadAutoMessage", MenuItem([](MenuItemContext&) {
      ReadAutoMessage();
    } ) },
    { "SetNewScanMsg", MenuItem([](MenuItemContext&) {
      SetNewScanMsg();
    } ) },
    { "LoadTextFile", MenuItem([](MenuItemContext&) {
      LoadTextFile();
    } ) },
    { "GuestApply", MenuItem([](MenuItemContext&) {
      GuestApply();
    } ) },
    { "ConfigFileList", MenuItem([](MenuItemContext&) {
      ConfigFileList();
    } ) },
    { "ListAllColors", MenuItem([](MenuItemContext&) {
      ListAllColors();
    } ) },
    { "RemoveNotThere", MenuItem([](MenuItemContext&) {
      RemoveNotThere();
    } ) },
    { "AttachFile", MenuItem([](MenuItemContext&) {
      AttachFile();
    } ) },
    { "InternetEmail", MenuItem([](MenuItemContext&) {
      InternetEmail();
    } ) },
    { "UnQScan", MenuItem([](MenuItemContext&) {
      UnQScan();
    } ) },
    { "Packers", MenuItem([](MenuItemContext&) {
      Packers();
    } ) },
    { "InitVotes", MenuItem([](MenuItemContext&) {
      InitVotes();
    } ) },
    { "TurnMCIOn", MenuItem([](MenuItemContext&) {
      TurnMCIOn();
    } ) },
    { "TurnMCIOff", MenuItem([](MenuItemContext&) {
      TurnMCIOff();
    } ) },
//    { "", MenuItem([](MenuItemContext& context) {
//    } ) },
  };
}

}
