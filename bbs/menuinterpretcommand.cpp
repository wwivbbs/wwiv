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

// Categories
static const std::string MENU_CAT_MSGS = "Message";
static const std::string MENU_CAT_EMAIL = "EMail";
static const std::string MENU_CAT_FILE = "File";
static const std::string MENU_CAT_BBSLIST = "BBSList";
static const std::string MENU_CAT_AUTOMSG = "AutoMessage";
static const std::string MENU_CAT_CHAIN = "Chain";
static const std::string MENU_CAT_NET = "Net";
static const std::string MENU_CAT_QWK = "QWK";
static const std::string MENU_CAT_VOTE = "Vote";
static const std::string MENU_CAT_GFILES = "GFiles";
static const std::string MENU_CAT_SYSOP = "SYSOP";
static const std::string MENU_CAT_MENU = "Menu";
static const std::string MENU_CAT_SYS = "System";
static const std::string MENU_CAT_CONF = "Conference";
static const std::string MENU_CAT_USER = "User";

struct MenuItemContext {
  MenuItemContext(MenuInstance* m, std::string p1, std::string p2)
      : pMenuData(m), param1(std::move(p1)), param2(std::move(p2)), p2_(std::move(p2)) {}
  // May be null if not invoked from an actual menu.
  MenuInstance* pMenuData;
  string param1;
  string param2;
  bool finished{false};
  bool need_reload{false};
  const std::string& p2_;
};

struct MenuItem {
  MenuItem(std::string desc, std::string category, std::function<void(MenuItemContext&)> f)
      : description_(std::move(desc)), category_(std::move(category)), f_(std::move(f)) {}
  MenuItem(std::string desc, std::function<void(MenuItemContext&)> f)
      : description_(std::move(desc)), f_(std::move(f)) {}
  MenuItem(std::function<void(MenuItemContext&)> f)
      : description_(""), f_(std::move(f)) {}

  std::string description_;
  std::string category_;
  std::function<void(MenuItemContext&)> f_;
  // Set at the end of CreateMenuMap.
  std::string cmd_;
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
  map<string, MenuItem, ci_less> m;
  m.emplace("MENU", MenuItem(R"(<menu>
  Loads up and starts running a new menu set, where <menu> equals the name of
  the menu to load.
)", MENU_CAT_MENU, [](MenuItemContext& context) {
      if (context.pMenuData) {
        MenuInstance new_menu(context.pMenuData->menu_directory(), context.param1);
        new_menu.RunMenu();
      }
    } ));
  m.emplace("ReturnFromMenu", MenuItem(R"()", MENU_CAT_MENU, [](MenuItemContext& context) {
      if (context.pMenuData) {
        InterpretCommand(context.pMenuData, context.pMenuData->header.szExitScript);
        context.finished = true;
      }
    } ));
  m.emplace("DLFreeFile", MenuItem(R"(
<dirfname> <filename>

  This will download a file, but not check ratios or charge a download charge.
  You must specify the dir filename, which is the name of the data file in
  the transfer editor.  filename is the name of the file being downloaded.
)", MENU_CAT_FILE, [](MenuItemContext& context) {
      const auto s = aligns(context.param2);
      MenuDownload(context.param1, s, true, true);
    } ));
  m.emplace("DLFile", MenuItem(R"(
<dirfname> <filename>

  This will download a file, with a check for ratios and will update the
  kb downloaded and number of files downloaded.
  You must specify the dirfilename, which is the name of the data file in
  the transfer editor.  filename is the name of the file being downloaded.
)", MENU_CAT_FILE, [](MenuItemContext& context) {
      const auto s = aligns(context.param2);
      MenuDownload(context.param1, s, false, true);
    } ));
  m.emplace("RunDoor", MenuItem(R"(<door name>

  Runs a door (chain) with doorname matching, exactly, the description you have
  given the door in //CHEDIT
)", MENU_CAT_CHAIN, [](MenuItemContext& context) {
      MenuRunDoorName(context.param1.c_str(), false);
    } ));
  m.emplace("RunDoorFree", MenuItem(R"(<door name>

  Runs a door (chain) with doorname matching, exactly, the description you have
  given the door in //CHEDIT, but this function bypasses the check to see if
  the user is allowed to run the door.
)", MENU_CAT_CHAIN, [](MenuItemContext& context) {
      MenuRunDoorName(context.param1.c_str(), true);
    } ));
  m.emplace("RunDoorNumber", MenuItem(R"(<door number>

  Like RunDoor, but you must specify the #1 in //CHEDIT instead of the
  description.
)", MENU_CAT_CHAIN, [](MenuItemContext& context) {
      const auto t = to_number<int>(context.param1);
      MenuRunDoorNumber(t, false);
    } ));
  m.emplace("RunDoorNumberFree", MenuItem(R"(<door number>

  Like RunDoorFree, but you must specify the #1 in //CHEDIT instead of the
  description.
)", MENU_CAT_CHAIN, [](MenuItemContext& context) {
      const auto t = to_number<int>(context.param1);
      MenuRunDoorNumber(t, true);
    } ));
  m.emplace("RunBasic", MenuItem(R"(<script name>

Runs a WWIVbasic Script
)", MENU_CAT_SYS, [](MenuItemContext& context) {
      // Runs a basic script from GFILES/
      wwiv::bbs::RunBasicScript(context.param1);
    } ));
  m.emplace("PrintFile", MenuItem(R"(<filename>

  Prints a file, first checking to see if you specified an absolute path,
  then the language dir, then the gfilesdir.  It will use the usual checks to
  determin .ANS, or .MSG if not specified.
)", MENU_CAT_SYS, [](MenuItemContext& context) {
      printfile(context.param1, true);
    } ));
  m.emplace("PrintFileNA", MenuItem(R"(<filename>

  Just like PrintFile, but the user can not abort it with the space bar.
)", MENU_CAT_SYS, [](MenuItemContext& context) {
      printfile(context.param1, false);
    } ));
  m.emplace("SetSubNumber", MenuItem(R"(<key>

  Equivalent to typing in a number at the main menu, it sets the current sub
  number.
)", MENU_CAT_MSGS, [](MenuItemContext& context) {
      SetSubNumber(context.param1.c_str());
    } ));
  m.emplace("SetDirNumber", MenuItem(R"( <key>

  Equivalent to typing in a number at the xfer menu, it sets the current dir
  number.
)", MENU_CAT_FILE, [](MenuItemContext& context) {
      SetDirNumber(context.param1.c_str());
    } ));
  m.emplace("SetMsgConf", MenuItem(R"(<key>

  Sets the subboards conference to key
)", MENU_CAT_MSGS, [](MenuItemContext& context) {
      SetMsgConf(context.param1.c_str()[0]);
    } ));
  m.emplace("SetDirConf", MenuItem(R"(<key>

  Sets the xfer section conference to key
)", MENU_CAT_FILE, [](MenuItemContext& context) {
      SetDirConf(context.param1.c_str()[0]);
    } ));
  m.emplace("EnableConf", MenuItem(R"(

  Turns conferencing on
)", MENU_CAT_CONF, [](MenuItemContext&) {
      EnableConf();
    } ));
  m.emplace("DisableConf", MenuItem(R"(

  Turns conferencing off
)", MENU_CAT_CONF, [](MenuItemContext&) {
      DisableConf();
    } ));
  m.emplace("Pause", MenuItem(R"(

  Pauses the screen, like 'pausescr()' in C code
)", MENU_CAT_SYS, [](MenuItemContext&) {
      pausescr();
    } ));
  m.emplace("ConfigUserMenuSet", MenuItem(R"(

  Takes the user into the user menu config so they can select which menuset
  they want to use, etc...
)", MENU_CAT_USER, [](MenuItemContext& context) {
      ConfigUserMenuSet();
      context.need_reload = true;
    } ));
  m.emplace("DisplayHelp", MenuItem(R"( <filename>

  An alias for DisplayMenu. This alias is deprecated, please use DisplayMenu.
)", MENU_CAT_MENU, [](MenuItemContext& context) {
      if (context.pMenuData && a()->user()->IsExpert()) {
        context.pMenuData->DisplayMenu();
      }
    } ));
    m.emplace("DisplayMenu", MenuItem(R"( <filename>

  Prints the 'novice menus' for the current menu set, or if one doesn't exist,
  it will generate one using the menu definitions.
)", MENU_CAT_MENU, [](MenuItemContext& context) {
      if (context.pMenuData && a()->user()->IsExpert()) {
        context.pMenuData->DisplayMenu();
      }
    } ));
  m.emplace("SelectSub", MenuItem(R"(

  This will prompt the user to enter a sub to change to.  However, it does not
  first show the subs (like Renegade).  However, you can stack a sublist and
  then this command to mimic the action.
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      ChangeSubNumber();
    } ));
  m.emplace("SelectDir", MenuItem(R"(

  Like SelectSub, but for the xfer section.
)", MENU_CAT_FILE, [](MenuItemContext&) {
      ChangeDirNumber();
    } ));
  m.emplace("SubList", MenuItem(R"(

  List the subs available
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      SubList();
    } ));
  m.emplace("UpSubConf", MenuItem(R"(

  Increment ()) to the previous conference number
)", MENU_CAT_CONF, [](MenuItemContext&) {
      UpSubConf();
    } ));
  m.emplace("DownSubConf", MenuItem(R"(

  Decrement ({) to the next sub conference
)", MENU_CAT_CONF, [](MenuItemContext&) {
      DownSubConf();
    } ));
  m.emplace("UpSub", MenuItem(R"(

  Increment the current sub# (+)
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      UpSub();
    } ));
  m.emplace("DownSub", MenuItem(R"(

  Decrement the current sub number (-)
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      DownSub();
    } ));
  m.emplace("ValidateUser", MenuItem(R"(
  Validate a new users.  I think this '!'
)", MENU_CAT_SYSOP, [](MenuItemContext&) {
      ValidateUser();
    } ));
  m.emplace("Doors", MenuItem(R"(
  Enter the doors, or chains section.  Like '.'
)", MENU_CAT_CHAIN, [](MenuItemContext&) {
      Chains();
    } ));
  m.emplace("TimeBank", MenuItem(R"(
  Enter the time bank
)", MENU_CAT_SYS, [](MenuItemContext&) {
      TimeBank();
    } ));
  m.emplace("AutoMessage", MenuItem(R"(
  Read the auto message
)", MENU_CAT_AUTOMSG, [](MenuItemContext&) {
      AutoMessage();
    } ));
  m.emplace("BBSList", MenuItem(R"(
  Read the bbslist
)", MENU_CAT_BBSLIST, [](MenuItemContext&) {
      NewBBSList();
    } ));
  m.emplace("RequestChat", MenuItem(R"(
  Request chat from the sysop
)", MENU_CAT_SYS, [](MenuItemContext&) {
      RequestChat();
    } ));
  m.emplace("Defaults", MenuItem(R"(
  Enter the normal 'defaults' section
)", MENU_CAT_SYS, [](MenuItemContext& context) {
      Defaults(context.need_reload);
    } ));
  m.emplace("SendEMail", MenuItem(R"(
  Enter and send email 'E' from the main menu
)", MENU_CAT_EMAIL, [](MenuItemContext&) {
      SendEMail();
    } ));
  m.emplace("Feedback", MenuItem(R"(
  Leave feedback to the syosp.  'F'
)", MENU_CAT_SYS, [](MenuItemContext&) {
      FeedBack();
    } ));
  m.emplace("Bulletins", MenuItem(R"(
  Enter the bulletins (or 'gfiles') section.  'G'
)", MENU_CAT_GFILES, [](MenuItemContext&) {
      Bulletins();
    } ));
  m.emplace("HopSub", MenuItem(R"(
  Hop to another sub.  'H'
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      HopSub();
    } ));
  m.emplace("SystemInfo", MenuItem(R"(
  View the system info
)", MENU_CAT_SYS, [](MenuItemContext&) {
      SystemInfo();
    } ));
  m.emplace("JumpSubConf", MenuItem(R"(
  Jump to another sub conference.
)", MENU_CAT_CONF, [](MenuItemContext&) {
      JumpSubConf();
    } ));
  m.emplace("KillEMail", MenuItem(R"(
  Kill email that you have sent 'K'
)", MENU_CAT_EMAIL, [](MenuItemContext&) {
      KillEMail();
    } ));
  m.emplace("LastCallers", MenuItem(R"(
  View the last few callers
)", MENU_CAT_SYS, [](MenuItemContext&) {
      LastCallers();
    } ));
  m.emplace("ReadEMail", MenuItem(R"(
  Read your email
)", MENU_CAT_EMAIL, [](MenuItemContext&) {
      ReadEMail();
    } ));
  m.emplace("NewMessageScan", MenuItem(R"(
  Do a new message scan
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      NewMessageScan();
    } ));
  m.emplace("Goodbye", MenuItem(R"(
  Normal logoff 'O'
)", MENU_CAT_SYS, [](MenuItemContext&) {
      GoodBye();
    } ));
  m.emplace("PostMessage", MenuItem(R"(
  Post a message in the current sub
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      WWIV_PostMessage();
    } ));
  m.emplace("NewMsgScanCurSub", MenuItem(R"(
  Scan new messages in the current message sub
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      ScanSub();
    } ));
  m.emplace("RemovePost", MenuItem(R"(
  Remove a post
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      RemovePost();
    } ));
  m.emplace("TitleScan", MenuItem(R"(
  Scan the titles of the messages in the current sub
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      TitleScan();
    } ));
  m.emplace("ListUsers", MenuItem(R"(
  List users who have access to the current sub
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      ListUsers();
    } ));
  m.emplace("Vote", MenuItem(R"(
  Enter the voting both
)", MENU_CAT_VOTE, [](MenuItemContext&) {
      Vote();
    } ));
  m.emplace("ToggleExpert", MenuItem(R"(
  Turn 'X'pert mode on or off (toggle)
)", MENU_CAT_SYS, [](MenuItemContext&) {
      ToggleExpert();
    } ));
  m.emplace("YourInfo", MenuItem(R"(
  Display the yourinfo screen
)", MENU_CAT_SYS, [](MenuItemContext&) {
      YourInfo();
    } ));
  m.emplace("WWIVVer", MenuItem(R"(
  Get the wwiv version
)", MENU_CAT_SYS, [](MenuItemContext&) {
      WWIVVersion();
    } ));
  m.emplace("ConferenceEdit", MenuItem(R"(
  Sysop command ot edit the conferences
)", MENU_CAT_SYSOP, [](MenuItemContext&) {
      JumpEdit();
    } ));
  m.emplace("SubEdit", MenuItem(R"(
  Sysop command to edit the subboards
)", MENU_CAT_SYSOP, [](MenuItemContext&) {
      BoardEdit();
    } ));
  m.emplace("ChainEdit", MenuItem(R"(
  Sysop command to edit the doors or chains
)", MENU_CAT_SYSOP, [](MenuItemContext&) {
      ChainEdit();
    } ));
  m.emplace("ToggleAvailable", MenuItem(R"(
  Toggle the sysop availability for chat
)", MENU_CAT_SYSOP, [](MenuItemContext&) {
      ToggleChat();
    } ));
  m.emplace("ChangeUser", MenuItem(R"(
  Sysop command equal to //CHUSER, to change into another users
)", MENU_CAT_SYSOP, [](MenuItemContext&) {
      ChangeUser();
    } ));
  m.emplace("DirEdit", MenuItem(R"(
  Sysop command to edit the directory records
)", MENU_CAT_SYSOP, [](MenuItemContext&) {
      DirEdit();
    } ));
  m.emplace("Edit", MenuItem(R"(
  Sysop command to edit a text file
)", MENU_CAT_SYSOP, [](MenuItemContext&) {
      EditText();
    } ));
  m.emplace("BulletinEdit", MenuItem(R"(
  Sysop command to edit the bulletins 'gfiles'
)", MENU_CAT_GFILES, [](MenuItemContext&) {
      EditBulletins();
    } ));
  m.emplace("LoadText", MenuItem(R"(
  Sysop command to load a text file that will be edited in the text editor
)", MENU_CAT_SYS, [](MenuItemContext&) {
      LoadTextFile();
    } ));
  m.emplace("ReadAllMail", MenuItem(R"(
  Sysop command to read all mail
)", MENU_CAT_EMAIL, [](MenuItemContext&) {
      ReadAllMail();
    } ));
  m.emplace("ReloadMenus", MenuItem(R"(
  This is probably obsolete.
)", MENU_CAT_SYSOP, [](MenuItemContext&) {
      ReloadMenus();
    } ));
  m.emplace("ResetQscan", MenuItem(R"(
  Set all messages to read (I think)
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      ResetQscan();
    } ));
  m.emplace("MemStat", MenuItem(R"()", MENU_CAT_SYSOP, [](MenuItemContext&) {
      MemoryStatus();
    } ));
  m.emplace("VoteEdit", MenuItem(R"(
  Sysop command to edit the voting both
)", MENU_CAT_SYSOP, [](MenuItemContext&) {
      InitVotes();
    } ));
  m.emplace("Log", MenuItem(R"(
  Syosp command to view the log file
)", MENU_CAT_SYS, [](MenuItemContext&) {
      ReadLog();
    } ));
  m.emplace("NetLog", MenuItem(R"(
  Sysop command to view the network log
)", MENU_CAT_NET, [](MenuItemContext&) {
      ReadNetLog();
    } ));
  m.emplace("Pending", MenuItem(R"(
  Shows which net files are ready to be sent
)", MENU_CAT_NET, [](MenuItemContext&) {
      PrintPending();
    } ));
  m.emplace("Status", MenuItem(R"()", MENU_CAT_SYSOP, [](MenuItemContext&) {
      PrintStatus();
    } ));
  m.emplace("TextEdit", MenuItem(R"(
  Edit a text file
)", MENU_CAT_SYS, [](MenuItemContext&) {
      TextEdit();
    } ));
  m.emplace("VotePrint", MenuItem(R"(
  Show the voting statistics
)", MENU_CAT_VOTE, [](MenuItemContext&) {
      VotePrint();
    } ));
  m.emplace("YLog", MenuItem(R"(
  View yesterdays log
)", MENU_CAT_SYS, [](MenuItemContext&) {
      YesterdaysLog();
    } ));
  m.emplace("ZLog", MenuItem(R"(
  View the ZLog
)", MENU_CAT_SYS, [](MenuItemContext&) {
      ZLog();
    } ));
  m.emplace("ViewNetDataLog", MenuItem(R"(
  View the net data logs
)", MENU_CAT_SYSOP, [](MenuItemContext&) {
      ViewNetDataLog();
    } ));
  m.emplace("UploadPost", MenuItem(R"(
  Allow a user to upload a post that will be posted
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      UploadPost();
    } ));
  m.emplace("cls", MenuItem(R"(
  Clear the screen
)", MENU_CAT_SYS, [](MenuItemContext&) {
      bout.cls();
    } ));
  m.emplace("NetListing", MenuItem(R"(
  Show networks
)", MENU_CAT_NET, [](MenuItemContext&) {
      print_net_listing(false);
    } ));
  m.emplace("WHO", MenuItem(R"(
  Show who else is online
)", MENU_CAT_SYS, [](MenuItemContext&) {
      WhoIsOnline();
    } ));
  m.emplace("NewMsgsAllConfs", MenuItem(R"(
  Do a new message scan for all subs in all conferences '/A'
)", MENU_CAT_CONF, [](MenuItemContext&) {
      // /A NewMsgsAllConfs
      NewMsgsAllConfs();
    } ));
  m.emplace("MultiEMail", MenuItem(R"(
  Send multi-email
)", MENU_CAT_EMAIL, [](MenuItemContext&) {
      // /E "MultiEMail"
      MultiEmail();
    } ));
  m.emplace("NewMsgScanFromHere", MenuItem(R"(
  Read new messages starting from the current sub
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      NewMsgScanFromHere();
    } ));
  m.emplace("ValidatePosts", MenuItem(R"(
  Sysop command to validate unvalidated posts
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      ValidateScan();
    } ));
  m.emplace("ChatRoom", MenuItem(R"(
  Go into the multiuser chat room
)", MENU_CAT_SYS, [](MenuItemContext&) {
      ChatRoom();
    } ));
  m.emplace("ClearQScan", MenuItem(R"(
  Marks messages unread.
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      ClearQScan();
    } ));
  m.emplace("FastGoodBye", MenuItem(R"(
  Logoff fast '/O'
)", MENU_CAT_SYS, [](MenuItemContext&) {
      FastGoodBye();
    } ));
  m.emplace("NewFilesAllConfs", MenuItem(R"(
  New file scan in all directories in all conferences
)", MENU_CAT_FILE, [](MenuItemContext&) {
      NewFilesAllConfs();
    } ));
  m.emplace("ReadIDZ", MenuItem(R"(
  Sysop command to read the file_id.diz and add it to the extended description
)", MENU_CAT_FILE, [](MenuItemContext&) {
      ReadIDZ();
    } ));
  m.emplace("UploadAllDirs", MenuItem(R"(
  Syosp command to add any files sitting in the directories, but not in
  the file database to wwiv's file database
)", MENU_CAT_FILE, [](MenuItemContext&) {
      UploadAllDirs();
    } ));
  m.emplace("UploadCurDir", MenuItem(R"(
  Sysop command to scan the current directory for any files that are not in
  wwiv's file database and adds them to it.
)", MENU_CAT_FILE, [](MenuItemContext&) {
      UploadCurDir();
    } ));
  m.emplace("RenameFiles", MenuItem(R"(
  Sysop command to edit and rename files
)", MENU_CAT_FILE, [](MenuItemContext&) {
      RenameFiles();
    } ));
  m.emplace("MoveFiles", MenuItem(R"(
  Sysop command to move files
)", MENU_CAT_FILE, [](MenuItemContext&) {
      MoveFiles();
    } ));
  m.emplace("SortDirs", MenuItem(R"(
  Sort the directory by date or name
)", MENU_CAT_FILE, [](MenuItemContext&) {
      SortDirs();
    } ));
  m.emplace("ReverseSortDirs", MenuItem(R"(
  Sort the directory by date or name, backwards.
)", MENU_CAT_FILE, [](MenuItemContext&) {
      ReverseSort();
    } ));
  m.emplace("AllowEdit", MenuItem(R"(
  Sysop command to enter the 'ALLOW.DAT' editor.
)", MENU_CAT_FILE, [](MenuItemContext&) {
      AllowEdit();
    } ));
  m.emplace("UploadFilesBBS", MenuItem(R"(
  Import a files.bbs (probably a CD) into the wwiv's file database
)", MENU_CAT_FILE, [](MenuItemContext&) {
      UploadFilesBBS();
    } ));
  m.emplace("DirList", MenuItem(R"(
  List the directory names in the xfer section
)", MENU_CAT_FILE, [](MenuItemContext&) {
      DirList();
    } ));
  m.emplace("UpDirConf", MenuItem(R"(
  Go to the next directory conference '}'
)", MENU_CAT_CONF, [](MenuItemContext&) {
      UpDirConf();
    } ));
  m.emplace("UpDir", MenuItem(R"(
  Go to the next directory number '+'
)", MENU_CAT_FILE, [](MenuItemContext&) {
      UpDir();
    } ));
  m.emplace("DownDirConf", MenuItem(R"(
  Go to the prior directory conference '{'
)", MENU_CAT_CONF, [](MenuItemContext&) {
      DownDirConf();
    } ));
  m.emplace("DownDir", MenuItem(R"(
  Go to the prior directory number '-'
)", MENU_CAT_FILE, [](MenuItemContext&) {
      DownDir();
    } ));
  m.emplace("ListUsersDL", MenuItem(R"(
  List users with access to the current xfer sub
)", MENU_CAT_FILE, [](MenuItemContext&) {
      ListUsersDL();
    } ));
  m.emplace("PrintDSZLog", MenuItem(R"(
  View the DSZ log
)", MENU_CAT_FILE, [](MenuItemContext&) {
      PrintDSZLog();
    } ));
  m.emplace("PrintDevices", MenuItem(R"(
  Show the 'devices'.  I have no idea why.
)", MENU_CAT_SYS, [](MenuItemContext&) {
      PrintDevices();
    } ));
  m.emplace("ViewArchive", MenuItem(R"(
  List an archive's contents
)", MENU_CAT_FILE, [](MenuItemContext&) {
      ViewArchive();
    } ));
  m.emplace("BatchMenu", MenuItem(R"(
  Enter the batch menu 'B'
)", MENU_CAT_FILE, [](MenuItemContext&) {
      BatchMenu();
    } ));
  m.emplace("Download", MenuItem(R"(
  Download a file 'D'
)", MENU_CAT_FILE, [](MenuItemContext&) {
      Download();
    } ));
  m.emplace("FindDescription", MenuItem(R"(
  Search for a file by description
)", MENU_CAT_FILE, [](MenuItemContext&) {
      FindDescription();
    } ));
  m.emplace("HopDir", MenuItem(R"(
  Hop to another directory number 'H'
)", MENU_CAT_FILE, [](MenuItemContext&) {
      HopDir();
    } ));
  m.emplace("JumpDirConf", MenuItem(R"(
  Jump to another directory conference 'J'
)", MENU_CAT_CONF, [](MenuItemContext&) {
      JumpDirConf();
    } ));
  m.emplace("ListFiles", MenuItem(R"(
  List the file in the current directory
)", MENU_CAT_FILE, [](MenuItemContext&) {
      ListFiles();
    } ));
  m.emplace("NewFileScan", MenuItem(R"(
  List files that are new since your 'New Scan Date (usually last call)' 'N'
)", MENU_CAT_FILE, [](MenuItemContext&) {
      NewFileScan();
    } ));
  m.emplace("SetNewFileScanDate", MenuItem(R"(
  Set the 'New Scan Date' to a new date
)", MENU_CAT_FILE, [](MenuItemContext&) {
      SetNewFileScanDate();
    } ));
  m.emplace("RemoveFiles", MenuItem(R"(
  Remove a file you uploaded
)", MENU_CAT_FILE, [](MenuItemContext&) {
      RemoveFiles();
    } ));
  m.emplace("SearchAllFiles", MenuItem(R"(
  Search all files???
)", MENU_CAT_FILE, [](MenuItemContext&) {
      SearchAllFiles();
    } ));
  m.emplace("XferDefaults", MenuItem(R"(
  Enter the xfer section defaults
)", MENU_CAT_FILE, [](MenuItemContext&) {
      XferDefaults();
    } ));
  m.emplace("Upload", MenuItem(R"(
  User upload a file
)", MENU_CAT_FILE, [](MenuItemContext&) {
      Upload();
    } ));
  m.emplace("YourInfoDL", MenuItem(R"(
  Prints user info for downloads
)", MENU_CAT_SYS, [](MenuItemContext&) {
      YourInfoDL();
    } ));
  m.emplace("UploadToSysop", MenuItem(R"(
  Upload a file into dir#0, the sysop dir.
)", MENU_CAT_FILE, [](MenuItemContext&) {
      UploadToSysop();
    } ));
  m.emplace("ReadAutoMessage", MenuItem(R"(
  Read the auto message
)", MENU_CAT_AUTOMSG, [](MenuItemContext&) {
      ReadAutoMessage();
    } ));
  m.emplace("SetNewScanMsg", MenuItem(R"(
  Enter the menu so that a user can set which subs he want to scan when doing
  a new message scan
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      SetNewScanMsg();
    } ));
  m.emplace("LoadTextFile", MenuItem(R"(
  Looks like a duplicate to 'LoadText'
)", MENU_CAT_SYS, [](MenuItemContext&) {
      LoadTextFile();
    } ));
  m.emplace("GuestApply", MenuItem(R"(
  Allows a guest to apply for access
)", MENU_CAT_SYS, [](MenuItemContext&) {
      GuestApply();
    } ));
  m.emplace("ConfigFileList", MenuItem(R"(
  Enter the List+ configurator so the user can set it up to look like he wants
)", MENU_CAT_FILE, [](MenuItemContext&) {
      ConfigFileList();
    } ));
  m.emplace("ListAllColors", MenuItem(R"(
  Display all colors available for use.
)", MENU_CAT_SYS, [](MenuItemContext&) {
      ListAllColors();
    } ));
  m.emplace("RemoveNotThere", MenuItem(R"(
  SYSOP command to remove files that do not exist.
)", MENU_CAT_FILE, [](MenuItemContext&) {
      RemoveNotThere();
    } ));
  m.emplace("AttachFile", MenuItem(R"()", MENU_CAT_EMAIL, [](MenuItemContext&) {
      AttachFile();
    } ));
  m.emplace("InternetEmail", MenuItem(R"()", MENU_CAT_EMAIL, [](MenuItemContext&) {
      InternetEmail();
    } ));
  m.emplace("UnQScan", MenuItem(R"(
  Marks messages as unread
)", MENU_CAT_MSGS, [](MenuItemContext&) {
      UnQScan();
    } ));
  m.emplace("Packers", MenuItem(R"(
  Executes the QWK menu.
)", MENU_CAT_QWK, [](MenuItemContext&) {
      Packers();
    } ));
  m.emplace("InitVotes", MenuItem(R"()", MENU_CAT_VOTE, [](MenuItemContext&) {
      InitVotes();
    } ));
  m.emplace("TurnMCIOn", MenuItem(R"(
  Enable MCI codes
)", MENU_CAT_SYS, [](MenuItemContext&) {
      TurnMCIOn();
    } ));
  m.emplace("TurnMCIOff", MenuItem(R"(
  Disable MCI codes
)", MENU_CAT_SYS, [](MenuItemContext&) {
      TurnMCIOff();
    } ));
//  m.emplace("", MenuItem(R"()", "", [](MenuItemContext& context) {
//    } ));
  // Set the cmd names.    
  for (auto& i : m) {
    i.second.cmd_ = i.first;
  }
  LOG(INFO) << sizeof(m);
  return m;
}

void emit_menu(const std::string& cmd, const std::string& cat, const std::string& desc, bool markdown, bool group_by_cat) {
  const auto lines = SplitString(desc, "\n");
  const auto c = group_by_cat || cat.empty() ? "" : StrCat(" [", cat ,"]");
  if (markdown) {
    std::cout << "### ";
  }
  std::cout << cmd << c << std::endl;
  for (const auto& d : lines) {
    std::cout << "    " << StringTrim(d) << std::endl;
  }
  std::cout << std::endl << std::endl;
}

void emit_category_name(const std::string& cat, bool output_markdown) {
  if (cat.empty()) {
    return;
  }

  if (output_markdown) {
    std::cout << "## ";
  }  
  std::cout << "Category: " << StringTrim(cat) << std::endl;
  std::cout << "***" << std::endl;
  std::cout << std::endl;
}

void PrintMenuCommands(const std::string& arg) {
  const auto category_group = arg.find('c') != std::string::npos;
  const auto output_markdown = arg.find('m') != std::string::npos;

  if (output_markdown) {
    std::cout << R"(
# WWIV Menu Commands
***

Here is the list of all WWIV Menu Commands available in the Menu Editor broken
out by category.

)";
  }

  auto raw_commands = CreateCommandMap();
  auto& commands = raw_commands;
  if (category_group) {
    std::map<std::string, std::vector<MenuItem>> cat_commands;
    for (const auto& c : raw_commands) {
      cat_commands[c.second.category_].emplace_back(c.second);
    }

    for (const auto& c : cat_commands) {
      emit_category_name(c.first, output_markdown);
      for (const auto& m : c.second) {
        emit_menu(m.cmd_, "", m.description_, output_markdown, true);
      }
    }
    return;
  }
  for (const auto& c : commands) {
    const auto cmd = c.first;
    const auto cat = c.second.category_;
    const auto desc = c.second.description_;
    emit_menu(cmd, cat, desc, output_markdown, false);
  }
}

}
