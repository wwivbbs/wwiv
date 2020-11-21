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
#include "bbs/menus/menucommands.h"
#include "bbs/basic/basic.h"
#include "bbs/bbs.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsovl3.h"
#include "bbs/hop.h"
#include "bbs/menus/config_menus.h"
#include "bbs/menus/mainmenu.h"
#include "bbs/menus/menuspec.h"
#include "bbs/menus/menusupp.h"
#include "bbs/misccmd.h"
#include "bbs/new_bbslist.h"
#include "bbs/sublist.h"
#include "bbs/syschat.h"
#include "bbs/sysopf.h"
#include "bbs/xferovl1.h"
#include "common/output.h"
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
using wwiv::bbslist::NewBBSList;
using wwiv::core::IniFile;

using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv::bbs::menus {

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

std::optional<MenuContext> InterpretCommand(Menu* menu, const std::string& cmd,
                                            const std::string& data) {
  if (cmd.empty()) {
    return std::nullopt;
  }
  static auto functions = CreateCommandMap(); // NOLINT(clang-diagnostic-exit-time-destructors)

  if (contains(functions, cmd)) {
    MenuContext context(menu, data);
    at(functions, cmd).f_(context);
    if (menu) {
      menu->reload = context.need_reload;
    }
    return {context};
  }
  return std::nullopt;
}

map<string, MenuItem, ci_less> CreateCommandMap() {
  map<string, MenuItem, ci_less> m;

  m.emplace("MENU", MenuItem(R"(<menu>
  Loads up and starts running a new menu set, where <menu> equals the name of
  the menu to load.)",
                             MENU_CAT_MENU, [](MenuContext& context) {
                               context.menu_action = menu_command_action_t::push_menu;
                             }));
  m.emplace("ReturnFromMenu", MenuItem(R"()", MENU_CAT_MENU, [](MenuContext& context) {
              context.menu_action = menu_command_action_t::return_from_menu;
            }));
  m.emplace("DLFreeFile", MenuItem(R"(
<dirfname> <filename>

  This will download a file, but not check ratios or charge a download charge.
  You must specify the dir filename, which is the name of the data file in
  the transfer editor.  filename is the name of the file being downloaded.
)",
                                   MENU_CAT_FILE, [](MenuContext& context) {
                                     MenuDownload(context.data, true, true);
                                   }));
  m.emplace("DLFile", MenuItem(R"(
<dirfname> <filename>

  This will download a file, with a check for ratios and will update the
  kb downloaded and number of files downloaded.
  You must specify the dirfilename, which is the name of the data file in
  the transfer editor.  filename is the name of the file being downloaded.
)",
                               MENU_CAT_FILE, [](MenuContext& context) {
                                 MenuDownload(context.data, false, true);
                               }));
  m.emplace("RunDoor", MenuItem(R"(<door name>

  Runs a door (chain) with doorname matching, exactly, the description you have
  given the door in //CHEDIT
)",
                                MENU_CAT_CHAIN, [](MenuContext& context) {
                                  MenuRunDoorName(context.data, false);
                                }));
  m.emplace("RunDoorFree", MenuItem(R"(<door name>

  Runs a door (chain) with doorname matching, exactly, the description you have
  given the door in //CHEDIT, but this function bypasses the check to see if
  the user is allowed to run the door.
)",
                                    MENU_CAT_CHAIN, [](MenuContext& context) {
                                      MenuRunDoorName(context.data, true);
                                    }));
  m.emplace("RunDoorNumber", MenuItem(R"(<door number>

  Like RunDoor, but you must specify the #1 in //CHEDIT instead of the
  description.
)",
                                      MENU_CAT_CHAIN, [](MenuContext& context) {
                                        const auto t = to_number<int>(context.data);
                                        MenuRunDoorNumber(t, false);
                                      }));
  m.emplace("RunDoorNumberFree", MenuItem(R"(<door number>

  Like RunDoorFree, but you must specify the #1 in //CHEDIT instead of the
  description.
)",
                                          MENU_CAT_CHAIN, [](MenuContext& context) {
                                            const auto t = to_number<int>(context.data);
                                            MenuRunDoorNumber(t, true);
                                          }));
  m.emplace("RunBasic", MenuItem(R"(<script name>

Runs a WWIVbasic Script
)",
                                 MENU_CAT_SYS, [](MenuContext& context) {
                                   // Runs a basic script from GFILES/
                                   basic::RunBasicScript(context.data);
                                 }));
  m.emplace("PrintFile", MenuItem(R"(<filename>

  Prints a file, first checking to see if you specified an absolute path,
  then the language dir, then the gfilesdir.  It will use the usual checks to
  determine .ANS, or .MSG if not specified.
)",
                                  MENU_CAT_SYS, [](MenuContext& context) {
                                    bout.printfile(context.data, true);
                                  }));
  m.emplace("PrintFileNA", MenuItem(R"(<filename>

  Just like PrintFile, but the user can not abort it with the space bar.
)",
                                    MENU_CAT_SYS, [](MenuContext& context) {
                                      bout.printfile(context.data, false);
                                    }));
  m.emplace("SetSubNumber", MenuItem(R"(<key>

  Equivalent to typing in a number at the main menu, it sets the current sub
  number.
)",
                                     MENU_CAT_MSGS, SetSubNumber));
  m.emplace("SetDirNumber", MenuItem(R"( <key>

  Equivalent to typing in a number at the xfer menu, it sets the current dir
  number.
)",
                                     MENU_CAT_FILE, SetDirNumber));
  m.emplace("SetMsgConf", MenuItem(R"(<key>

  Sets the subboards conference to key
)",
                                   MENU_CAT_MSGS, [](MenuContext& context) {
                                     SetMsgConf(context.data.c_str()[0]);
                                   }));
  m.emplace("SetDirConf", MenuItem(R"(<key>

  Sets the xfer section conference to key
)",
                                   MENU_CAT_FILE, [](MenuContext& context) {
                                     SetDirConf(context.data.c_str()[0]);
                                   }));
  m.emplace("EnableConf", MenuItem(R"(

  Turns conferencing on
)",
                                   MENU_CAT_CONF, [](MenuContext&) { EnableConf(); }));
  m.emplace("DisableConf", MenuItem(R"(

  Turns conferencing off
)",
                                    MENU_CAT_CONF, [](MenuContext&) { DisableConf(); }));
  m.emplace("Pause", MenuItem(R"(

  Pauses the screen, like 'pausescr()' in C code
)",
                              MENU_CAT_SYS, [](MenuContext&) { bout.pausescr(); }));
  m.emplace("ConfigUserMenuSet", MenuItem(R"(

  Takes the user into the user menu config so they can select which menuset
  they want to use, etc...
)",
                                          MENU_CAT_USER, [](MenuContext& context) {
                                            ConfigUserMenuSet();
                                            context.need_reload = true;
                                          }));
  m.emplace("DisplayHelp", MenuItem(R"( <filename>

  An alias for DisplayMenu. 
  This alias is deprecated, please use DisplayMenu.
)",
                                    MENU_CAT_MENU, [](MenuContext& context) {
                                      if (context.cur_menu && a()->user()->IsExpert()) {
                                        context.cur_menu->DisplayMenu();
                                      }
                                    }));
  m.emplace("DisplayMenu", MenuItem(R"( <filename>

  Prints the 'novice menus' for the current menu set, or if one doesn't exist,
  it will generate one using the menu definitions.
)",
                                    MENU_CAT_MENU, [](MenuContext& context) {
                                      if (context.cur_menu && a()->user()->IsExpert()) {
                                        context.cur_menu->DisplayMenu();
                                      }
                                    }));
  m.emplace("SelectSub", MenuItem(R"(

  This will prompt the user to enter a sub to change to.  However, it does not
  first show the subs (like Renegade).  However, you can stack a sublist and
  then this command to mimic the action.
)",
                                  MENU_CAT_MSGS, [](MenuContext&) { ChangeSubNumber(); }));
  m.emplace("SelectDir", MenuItem(R"(

  Like SelectSub, but for the xfer section.
)",
                                  MENU_CAT_FILE, [](MenuContext&) { ChangeDirNumber(); }));
  m.emplace("SubList", MenuItem(R"(

  List the subs available
)",
                                MENU_CAT_MSGS, [](MenuContext&) { SubList(); }));
  m.emplace("UpSubConf", MenuItem(R"(

  Increment ()) to the previous conference number
)",
                                  MENU_CAT_CONF, [](MenuContext&) { UpSubConf(); }));
  m.emplace("DownSubConf", MenuItem(R"(

  Decrement ({) to the next sub conference
)",
                                    MENU_CAT_CONF, [](MenuContext&) { DownSubConf(); }));
  m.emplace("UpSub", MenuItem(R"(

  Increment the current sub# (+)
)",
                              MENU_CAT_MSGS, [](MenuContext&) { UpSub(); }));
  m.emplace("DownSub", MenuItem(R"(

  Decrement the current sub number (-)
)",
                                MENU_CAT_MSGS, [](MenuContext&) { DownSub(); }));
  m.emplace("ValidateUser", MenuItem(R"(
  Validate a new users.  I think this '!'
)",
                                     MENU_CAT_SYSOP, [](MenuContext&) { ValidateUser(); }));
  m.emplace("Doors", MenuItem(R"(
  Enter the doors, or chains section.  Like '.'
)",
                              MENU_CAT_CHAIN, [](MenuContext&) { Chains(); }));
  m.emplace("TimeBank", MenuItem(R"(
  Enter the time bank
)",
                                 MENU_CAT_SYS, [](MenuContext&) { TimeBank(); }));
  m.emplace("AutoMessage", MenuItem(R"(
  Read the auto message
)",
                                    MENU_CAT_AUTOMSG, [](MenuContext&) { AutoMessage(); }));
  m.emplace("BBSList", MenuItem(R"(
  Read the bbslist
)",
                                MENU_CAT_BBSLIST, [](MenuContext&) { NewBBSList(); }));
  m.emplace("RequestChat", MenuItem(R"(
  Request chat from the sysop
)",
                                    MENU_CAT_SYS, [](MenuContext&) { RequestChat(); }));
  m.emplace("Defaults", MenuItem(R"(
  Enter the normal 'defaults' section
)",
                                 MENU_CAT_SYS,
                                 [](MenuContext& context) { Defaults(context.need_reload); }));
  m.emplace("SendEMail", MenuItem(R"(
  Enter and send email 'E' from the main menu
)",
                                  MENU_CAT_EMAIL, [](MenuContext&) { SendEMail(); }));
  m.emplace("Feedback", MenuItem(R"(
  Leave feedback to the syosp.  'F'
)",
                                 MENU_CAT_SYS, [](MenuContext&) { FeedBack(); }));
  m.emplace("Bulletins", MenuItem(R"(
  Enter the bulletins (or 'gfiles') section.  'G'
)",
                                  MENU_CAT_GFILES, [](MenuContext&) { Bulletins(); }));
  m.emplace("HopSub", MenuItem(R"(
  Hop to another sub.  'H'
)",
                               MENU_CAT_MSGS, [](MenuContext&) { HopSub(); }));
  m.emplace("SystemInfo", MenuItem(R"(
  View the system info
)",
                                   MENU_CAT_SYS, [](MenuContext&) { SystemInfo(); }));
  m.emplace("JumpSubConf", MenuItem(R"(
  Jump to another sub conference.
)",
                                    MENU_CAT_CONF, [](MenuContext&) { JumpSubConf(); }));
  m.emplace("KillEMail", MenuItem(R"(
  Kill email that you have sent 'K'
)",
                                  MENU_CAT_EMAIL, [](MenuContext&) { KillEMail(); }));
  m.emplace("LastCallers", MenuItem(R"(
  View the last few callers
)",
                                    MENU_CAT_SYS, [](MenuContext&) { LastCallers(); }));
  m.emplace("ReadEMail", MenuItem(R"(
  Read your email
)",
                                  MENU_CAT_EMAIL, [](MenuContext&) { ReadEMail(); }));
  m.emplace("NewMessageScan", MenuItem(R"(
  Do a new message scan
)",
                                       MENU_CAT_MSGS, [](MenuContext&) { NewMessageScan(); }));
  m.emplace("Goodbye", MenuItem(R"(
  Normal logoff 'O'
)",
                                MENU_CAT_SYS, [](MenuContext&) { GoodBye(); }));
  m.emplace("PostMessage", MenuItem(R"(
  Post a message in the current sub
)",
                                    MENU_CAT_MSGS, [](MenuContext&) { WWIV_PostMessage(); }));
  m.emplace("NewMsgScanCurSub", MenuItem(R"(
  Scan new messages in the current message sub
)",
                                         MENU_CAT_MSGS, [](MenuContext&) { ScanSub(); }));
  m.emplace("RemovePost", MenuItem(R"(
  Remove a post
)",
                                   MENU_CAT_MSGS, [](MenuContext&) { RemovePost(); }));
  m.emplace("TitleScan", MenuItem(R"(
  Scan the titles of the messages in the current sub
)",
                                  MENU_CAT_MSGS, [](MenuContext&) { TitleScan(); }));
  m.emplace("ListUsers", MenuItem(R"(
  List users who have access to the current sub
)",
                                  MENU_CAT_MSGS, [](MenuContext&) { ListUsers(); }));
  m.emplace("Vote", MenuItem(R"(
  Enter the voting both
)",
                             MENU_CAT_VOTE, [](MenuContext&) { Vote(); }));
  m.emplace("ToggleExpert", MenuItem(R"(
  Turn 'X'pert mode on or off (toggle)
)",
                                     MENU_CAT_SYS, [](MenuContext&) { ToggleExpert(); }));
  m.emplace("YourInfo", MenuItem(R"(
  Display the yourinfo screen
)",
                                 MENU_CAT_SYS, [](MenuContext&) { YourInfo(); }));
  m.emplace("WWIVVer", MenuItem(R"(
  Get the wwiv version
)",
                                MENU_CAT_SYS, [](MenuContext&) { WWIVVersion(); }));
  m.emplace("ConferenceEdit", MenuItem(R"(
  Sysop command ot edit the conferences
)",
                                       MENU_CAT_SYSOP, [](MenuContext&) { JumpEdit(); }));
  m.emplace("SubEdit", MenuItem(R"(
  Sysop command to edit the subboards
)",
                                MENU_CAT_SYSOP, [](MenuContext&) { BoardEdit(); }));
  m.emplace("ChainEdit", MenuItem(R"(
  Sysop command to edit the doors or chains
)",
                                  MENU_CAT_SYSOP, [](MenuContext&) { ChainEdit(); }));
  m.emplace("ToggleAvailable", MenuItem(R"(
  Toggle the sysop availability for chat
)",
                                        MENU_CAT_SYSOP, [](MenuContext&) { ToggleChat(); }));
  m.emplace("ChangeUser", MenuItem(R"(
  Sysop command equal to //CHUSER, to change into another users
)",
                                   MENU_CAT_SYSOP, [](MenuContext&) { ChangeUser(); }));
  m.emplace("DirEdit", MenuItem(R"(
  Sysop command to edit the directory records
)",
                                MENU_CAT_SYSOP, [](MenuContext&) { DirEdit(); }));
  m.emplace("Edit", MenuItem(R"(
  Sysop command to edit a text file
)",
                             MENU_CAT_SYSOP, [](MenuContext&) { EditText(); }));
  m.emplace("BulletinEdit", MenuItem(R"(
  Sysop command to edit the bulletins 'gfiles'
)",
                                     MENU_CAT_GFILES, [](MenuContext&) { EditBulletins(); }));
  m.emplace("LoadText", MenuItem(R"(
  Sysop command to load a text file that will be edited in the text editor
)",
                                 MENU_CAT_SYS, [](MenuContext&) { LoadTextFile(); }));
  m.emplace("ReadAllMail", MenuItem(R"(
  Sysop command to read all mail
)",
                                    MENU_CAT_EMAIL, [](MenuContext&) { ReadAllMail(); }));
  m.emplace("ReloadMenus", MenuItem(R"(
  This is probably obsolete.
)",
                                    MENU_CAT_SYSOP, [](MenuContext&) { ReloadMenus(); }));
  m.emplace("ResetQscan", MenuItem(R"(
  Set all messages to read (I think)
)",
                                   MENU_CAT_MSGS, [](MenuContext&) { ResetQscan(); }));
  m.emplace("MemStat", MenuItem(R"()", MENU_CAT_SYSOP, [](MenuContext&) { MemoryStatus(); }));
  m.emplace("VoteEdit", MenuItem(R"(
  Sysop command to edit the voting both
)",
                                 MENU_CAT_SYSOP, [](MenuContext&) { InitVotes(); }));
  m.emplace("Log", MenuItem(R"(
  Syosp command to view the log file
)",
                            MENU_CAT_SYS, [](MenuContext&) { ReadLog(); }));
  m.emplace("NetLog", MenuItem(R"(
  Sysop command to view the network log
)",
                               MENU_CAT_NET, [](MenuContext&) { ReadNetLog(); }));
  m.emplace("Pending", MenuItem(R"(
  Shows which net files are ready to be sent
)",
                                MENU_CAT_NET, [](MenuContext&) { PrintPending(); }));
  m.emplace("Status", MenuItem(R"()", MENU_CAT_SYSOP, [](MenuContext&) { PrintStatus(); }));
  m.emplace("TextEdit", MenuItem(R"(
  Edit a text file
)",
                                 MENU_CAT_SYS, [](MenuContext&) { TextEdit(); }));
  m.emplace("VotePrint", MenuItem(R"(
  Show the voting statistics
)",
                                  MENU_CAT_VOTE, [](MenuContext&) { VotePrint(); }));
  m.emplace("YLog", MenuItem(R"(
  View yesterdays log
)",
                             MENU_CAT_SYS, [](MenuContext&) { YesterdaysLog(); }));
  m.emplace("ZLog", MenuItem(R"(
  View the ZLog
)",
                             MENU_CAT_SYS, [](MenuContext&) { ZLog(); }));
  m.emplace("ViewNetDataLog", MenuItem(R"(
  View the net data logs
)",
                                       MENU_CAT_SYSOP, [](MenuContext&) { ViewNetDataLog(); }));
  m.emplace("UploadPost", MenuItem(R"(
  Allow a user to upload a post that will be posted
)",
                                   MENU_CAT_MSGS, [](MenuContext&) { UploadPost(); }));
  m.emplace("cls", MenuItem(R"(
  Clear the screen
)",
                            MENU_CAT_SYS, [](MenuContext&) { bout.cls(); }));
  m.emplace("NetListing",
            MenuItem(R"(
  Show networks
)",
                     MENU_CAT_NET, [](MenuContext&) { print_net_listing(false); }));
  m.emplace("WHO", MenuItem(R"(
  Show who else is online
)",
                            MENU_CAT_SYS, [](MenuContext&) { WhoIsOnline(); }));
  m.emplace("NewMsgsAllConfs", MenuItem(R"(
  Do a new message scan for all subs in all conferences '/A'
)",
                                        MENU_CAT_CONF, [](MenuContext&) {
                                          // /A NewMsgsAllConfs
                                          NewMsgsAllConfs();
                                        }));
  m.emplace("MultiEMail", MenuItem(R"(
  Send multi-email
)",
                                   MENU_CAT_EMAIL, [](MenuContext&) {
                                     // /E "MultiEMail"
                                     MultiEmail();
                                   }));
  m.emplace("NewMsgScanFromHere",
            MenuItem(R"(
  Read new messages starting from the current sub
)",
                     MENU_CAT_MSGS, [](MenuContext&) { NewMsgScanFromHere(); }));
  m.emplace("ValidatePosts", MenuItem(R"(
  Sysop command to validate unvalidated posts
)",
                                      MENU_CAT_MSGS, [](MenuContext&) { ValidateScan(); }));
  m.emplace("ChatRoom", MenuItem(R"(
  Go into the multiuser chat room
)",
                                 MENU_CAT_SYS, [](MenuContext&) { ChatRoom(); }));
  m.emplace("ClearQScan", MenuItem(R"(
  Marks messages unread.
)",
                                   MENU_CAT_MSGS, [](MenuContext&) { ClearQScan(); }));
  m.emplace("FastGoodBye", MenuItem(R"(
  Logoff fast '/O'
)",
                                    MENU_CAT_SYS, [](MenuContext&) { FastGoodBye(); }));
  m.emplace("NewFilesAllConfs",
            MenuItem(R"(
  New file scan in all directories in all conferences
)",
                     MENU_CAT_FILE, [](MenuContext&) { NewFilesAllConfs(); }));
  m.emplace("ReadIDZ", MenuItem(R"(
  Sysop command to read the file_id.diz and add it to the extended description
)",
                                MENU_CAT_FILE, [](MenuContext&) { ReadIDZ(); }));
  m.emplace("UploadAllDirs", MenuItem(R"(
  Syosp command to add any files sitting in the directories, but not in
  the file database to wwiv's file database
)",
                                      MENU_CAT_FILE, [](MenuContext&) { UploadAllDirs(); }));
  m.emplace("UploadCurDir", MenuItem(R"(
  Sysop command to scan the current directory for any files that are not in
  wwiv's file database and adds them to it.
)",
                                     MENU_CAT_FILE, [](MenuContext&) { UploadCurDir(); }));
  m.emplace("RenameFiles", MenuItem(R"(
  Sysop command to edit and rename files
)",
                                    MENU_CAT_FILE, [](MenuContext&) { RenameFiles(); }));
  m.emplace("MoveFiles", MenuItem(R"(
  Sysop command to move files
)",
                                  MENU_CAT_FILE, [](MenuContext&) { MoveFiles(); }));
  m.emplace("SortDirs", MenuItem(R"(
  Sort the directory by date or name
)",
                                 MENU_CAT_FILE, [](MenuContext&) { SortDirs(); }));
  m.emplace("ReverseSortDirs", MenuItem(R"(
  Sort the directory by date or name, backwards.
)",
                                        MENU_CAT_FILE, [](MenuContext&) { ReverseSort(); }));
  m.emplace("AllowEdit", MenuItem(R"(
  Sysop command to enter the 'ALLOW.DAT' editor.
)",
                                  MENU_CAT_FILE, [](MenuContext&) { AllowEdit(); }));
  m.emplace("UploadFilesBBS", MenuItem(R"(
  Import a files.bbs (probably a CD) into the wwiv's file database
)",
                                       MENU_CAT_FILE, [](MenuContext&) { UploadFilesBBS(); }));
  m.emplace("DirList", MenuItem(R"(
  List the directory names in the xfer section
)",
                                MENU_CAT_FILE, [](MenuContext&) { DirList(); }));
  m.emplace("UpDirConf", MenuItem(R"(
  Go to the next directory conference '}'
)",
                                  MENU_CAT_CONF, [](MenuContext&) { UpDirConf(); }));
  m.emplace("UpDir", MenuItem(R"(
  Go to the next directory number '+'
)",
                              MENU_CAT_FILE, [](MenuContext&) { UpDir(); }));
  m.emplace("DownDirConf", MenuItem(R"(
  Go to the prior directory conference '{'
)",
                                    MENU_CAT_CONF, [](MenuContext&) { DownDirConf(); }));
  m.emplace("DownDir", MenuItem(R"(
  Go to the prior directory number '-'
)",
                                MENU_CAT_FILE, [](MenuContext&) { DownDir(); }));
  m.emplace("ListUsersDL", MenuItem(R"(
  List users with access to the current xfer sub
)",
                                    MENU_CAT_FILE, [](MenuContext&) { ListUsersDL(); }));
  m.emplace("PrintDSZLog", MenuItem(R"(
  View the DSZ log
)",
                                    MENU_CAT_FILE, [](MenuContext&) { PrintDSZLog(); }));
  m.emplace("PrintDevices", MenuItem(R"(
  Show the 'devices'.  I have no idea why.
)",
                                     MENU_CAT_SYS, [](MenuContext&) { PrintDevices(); }));
  m.emplace("ViewArchive", MenuItem(R"(
  List an archive's contents
)",
                                    MENU_CAT_FILE, [](MenuContext&) { ViewArchive(); }));
  m.emplace("BatchMenu", MenuItem(R"(
  Enter the batch menu 'B'
)",
                                  MENU_CAT_FILE, [](MenuContext&) { BatchMenu(); }));
  m.emplace("Download", MenuItem(R"(
  Download a file 'D'
)",
                                 MENU_CAT_FILE, [](MenuContext&) { Download(); }));
  m.emplace("FindDescription",
            MenuItem(R"(
  Search for a file by description
)",
                     MENU_CAT_FILE, [](MenuContext&) { FindDescription(); }));
  m.emplace("HopDir", MenuItem(R"(
  Hop to another directory number 'H'
)",
                               MENU_CAT_FILE, [](MenuContext&) { HopDir(); }));
  m.emplace("JumpDirConf", MenuItem(R"(
  Jump to another directory conference 'J'
)",
                                    MENU_CAT_CONF, [](MenuContext&) { JumpDirConf(); }));
  m.emplace("ListFiles", MenuItem(R"(
  List the file in the current directory
)",
                                  MENU_CAT_FILE, [](MenuContext&) { ListFiles(); }));
  m.emplace("NewFileScan", MenuItem(R"(
  List files that are new since your 'New Scan Date (usually last call)' 'N'
)",
                                    MENU_CAT_FILE, [](MenuContext&) { NewFileScan(); }));
  m.emplace("SetNewFileScanDate",
            MenuItem(R"(
  Set the 'New Scan Date' to a new date
)",
                     MENU_CAT_FILE, [](MenuContext&) { SetNewFileScanDate(); }));
  m.emplace("RemoveFiles", MenuItem(R"(
  Remove a file you uploaded
)",
                                    MENU_CAT_FILE, [](MenuContext&) { RemoveFiles(); }));
  m.emplace("SearchAllFiles", MenuItem(R"(
  Search all files???
)",
                                       MENU_CAT_FILE, [](MenuContext&) { SearchAllFiles(); }));
  m.emplace("XferDefaults", MenuItem(R"(
  Enter the xfer section defaults
)",
                                     MENU_CAT_FILE, [](MenuContext&) { XferDefaults(); }));
  m.emplace("Upload", MenuItem(R"(
  User upload a file
)",
                               MENU_CAT_FILE, [](MenuContext&) { Upload(); }));
  m.emplace("YourInfoDL", MenuItem(R"(
  Prints user info for downloads
)",
                                   MENU_CAT_SYS, [](MenuContext&) { YourInfoDL(); }));
  m.emplace("UploadToSysop", MenuItem(R"(
  Upload a file into dir#0, the sysop dir.
)",
                                      MENU_CAT_FILE, [](MenuContext&) { UploadToSysop(); }));
  m.emplace("ReadAutoMessage",
            MenuItem(R"(
  Read the auto message
)",
                     MENU_CAT_AUTOMSG, [](MenuContext&) { ReadAutoMessage(); }));
  m.emplace("SetNewScanMsg", MenuItem(R"(
  Enter the menu so that a user can set which subs he want to scan when doing
  a new message scan
)",
                                      MENU_CAT_MSGS, [](MenuContext&) { SetNewScanMsg(); }));
  m.emplace("LoadTextFile", MenuItem(R"(
  Looks like a duplicate to 'LoadText'
)",
                                     MENU_CAT_SYS, [](MenuContext&) { LoadTextFile(); }));
  m.emplace("GuestApply", MenuItem(R"(
  Allows a guest to apply for access
)",
                                   MENU_CAT_SYS, [](MenuContext&) { GuestApply(); }));
  m.emplace("ConfigFileList", MenuItem(R"(
  Enter the List+ configurator so the user can set it up to look like he wants
)",
                                       MENU_CAT_FILE, [](MenuContext&) { ConfigFileList(); }));
  m.emplace("ListAllColors", MenuItem(R"(
  Display all colors available for use.
)",
                                      MENU_CAT_SYS, [](MenuContext&) { ListAllColors(); }));
  m.emplace("RemoveNotThere", MenuItem(R"(
  SYSOP command to remove files that do not exist.
)",
                                       MENU_CAT_FILE, [](MenuContext&) { RemoveNotThere(); }));
  m.emplace("AttachFile", MenuItem(R"()", MENU_CAT_EMAIL, [](MenuContext&) { AttachFile(); }));
  m.emplace("UnQScan", MenuItem(R"(
  Marks messages as unread
)",
                                MENU_CAT_MSGS, [](MenuContext&) { UnQScan(); }));
  m.emplace("Packers", MenuItem(R"(
  Executes the QWK menu.
)",
                                MENU_CAT_QWK, [](MenuContext&) { Packers(); }));
  m.emplace("InitVotes", MenuItem(R"()", MENU_CAT_VOTE, [](MenuContext&) { InitVotes(); }));
  m.emplace("TurnMCIOn", MenuItem(R"(
  Enable MCI codes
)",
                                  MENU_CAT_SYS, [](MenuContext&) { bout.enable_mci(); }));
  m.emplace("TurnMCIOff", MenuItem(R"(
  Disable MCI codes
)",
                                   MENU_CAT_SYS, [](MenuContext&) { bout.disable_mci(); }));
  //  m.emplace("", MenuItem(R"()", "", [](MenuContext& context) {
  //    } ));

  // Set the cmd names.
  for (auto& i : m) {
    i.second.cmd_ = i.first;
  }
  return m;
}

} // namespace wwiv::bbs::menus
