/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include "bbs/attach.h"
#include "bbs/automsg.h"
#include "bbs/batch.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsovl2.h"
#include "bbs/bbsutl1.h"
#include "bbs/chat.h"
#include "bbs/chains.h"
#include "bbs/chnedit.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/datetime.h"
#include "bbs/defaults.h"
#include "bbs/diredit.h"
#include "bbs/dirlist.h"
#include "bbs/dropfile.h"
#include "bbs/events.h"
#include "bbs/external_edit.h"
#include "bbs/finduser.h"
#include "bbs/gfileedit.h"
#include "bbs/gfiles.h"
#include "bbs/input.h"
#include "bbs/inetmsg.h"
#include "bbs/instmsg.h"
#include "bbs/keycodes.h"
#include "bbs/listplus.h"
#include "bbs/menu.h"
#include "bbs/menusupp.h"
#include "bbs/message_file.h"
#include "bbs/misccmd.h"
#include "bbs/msgbase1.h"
#include "bbs/multinst.h"
#include "bbs/multmail.h"
#include "bbs/netsup.h"
#include "bbs/newuser.h"
#include "bbs/quote.h"
#include "bbs/pause.h"
#include "bbs/readmail.h"
#include "bbs/subedit.h"
#include "bbs/sysoplog.h"
#include "bbs/uedit.h"
#include "bbs/valscan.h"
#include "bbs/vote.h"
#include "bbs/voteedit.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/shortmsg.h"
#include "bbs/sr.h"
#include "bbs/subacc.h"
#include "bbs/sysopf.h"
#include "bbs/utility.h"
#include "bbs/wqscn.h"
#include "bbs/xfer.h"
#include "bbs/xferovl.h"
#include "bbs/xferovl1.h"
#include "bbs/xfertmp.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "bbs/workspace.h"
#include "bbs/printfile.h"
#include "sdk/status.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "sdk/filenames.h"
#include "sdk/user.h"

using std::string;
using wwiv::bbs::InputMode;
using wwiv::bbs::TempDisablePause;
using namespace wwiv::menus;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

void UnQScan() {
  bout.nl();
  bout << "|#9Mark messages as unread on [C]urrent sub or [A]ll subs (A/C/Q)? ";
  char ch = onek("QAC\r");
  switch (ch) {
  case 'Q':
  case RETURN:
    break;
  case 'A': {
    for (int i = 0; i < syscfg.max_subs; i++) {
      qsc_p[i] = 0;
    }
    bout << "\r\nQ-Scan pointers reset.\r\n\n";
  }
  break;
  case 'C': {
    bout.nl();
    qsc_p[a()->current_user_sub().subnum] = 0;
    bout << "Messages on " 
         << a()->subs().sub(a()->current_user_sub().subnum).name
         << " marked as unread.\r\n";
  }
  break;
  }
}

void DirList() {
  dirlist(0);
}

void UpSubConf() {
  if (okconf(a()->user())) {
    if ((a()->GetCurrentConferenceMessageArea() < subconfnum - 1)
        && (uconfsub[a()->GetCurrentConferenceMessageArea() + 1].confnum >= 0)) {
      a()->SetCurrentConferenceMessageArea(a()->GetCurrentConferenceMessageArea() + 1);
    } else {
      a()->SetCurrentConferenceMessageArea(0);
    }
    setuconf(ConferenceType::CONF_SUBS, a()->GetCurrentConferenceMessageArea(), -1);
  }
}

void DownSubConf() {
  if (okconf(a()->user())) {
    if (a()->GetCurrentConferenceMessageArea() > 0) {
      a()->SetCurrentConferenceMessageArea(a()->GetCurrentConferenceMessageArea() - 1);
    } else {
      while (uconfsub[a()->GetCurrentConferenceMessageArea() + 1].confnum >= 0
             && a()->GetCurrentConferenceMessageArea() < subconfnum - 1) {
        a()->SetCurrentConferenceMessageArea(a()->GetCurrentConferenceMessageArea() + 1);
      }
    }
    setuconf(ConferenceType::CONF_SUBS, a()->GetCurrentConferenceMessageArea(), -1);
  }
}

void DownSub() {
  if (a()->current_user_sub_num() > 0) {
    a()->set_current_user_sub_num(a()->current_user_sub_num() - 1);
  } else {
    while (a()->usub[a()->current_user_sub_num() + 1].subnum >= 0 &&
           a()->current_user_sub_num() < a()->subs().subs().size() - 1) {
      a()->set_current_user_sub_num(a()->current_user_sub_num() + 1);
    }
  }
}

void UpSub() {
  if (a()->current_user_sub_num() < a()->subs().subs().size() - 1 &&
      a()->usub[a()->current_user_sub_num() + 1].subnum >= 0) {
    a()->set_current_user_sub_num(a()->current_user_sub_num() + 1);
  } else {
    a()->set_current_user_sub_num(0);
  }
}

void ValidateUser() {
  bout.nl(2);
  bout << "|#9Enter user name or number:\r\n:";
  string userName = input(30, true);
  int nUserNum = finduser1(userName.c_str());
  if (nUserNum > 0) {
    sysoplog() << "@ Validated user #" << nUserNum;
    valuser(nUserNum);
  } else {
    bout << "Unknown user.\r\n";
  }
}

void Chains() {
  if (GuestCheck()) {
    write_inst(INST_LOC_CHAINS, 0, INST_FLAGS_NONE);
    play_sdf(CHAINS_NOEXT, false);
    do_chains();
  }
}

void TimeBank() {
  if (GuestCheck()) {
    write_inst(INST_LOC_BANK, 0, INST_FLAGS_NONE);
    time_bank();
  }
}

void AutoMessage() {
  write_inst(INST_LOC_AMSG, 0, INST_FLAGS_NONE);
  do_automessage();
}

void Defaults(bool& need_menu_reload) {
  if (GuestCheck()) {
    write_inst(INST_LOC_DEFAULTS, 0, INST_FLAGS_NONE);
    if (printfile(DEFAULTS_NOEXT)) {
      pausescr();
    }
    defaults(need_menu_reload);
  }
}

void SendEMail() {
  send_email();
}

void FeedBack() {
  write_inst(INST_LOC_FEEDBACK, 0, INST_FLAGS_NONE);
  feedback(false);
}

void Bulletins() {
  write_inst(INST_LOC_GFILES, 0, INST_FLAGS_NONE);
  printfile(GFILES_NOEXT);
  gfiles();
}

void SystemInfo() {
  WWIVVersion();

  if (printfile(LOGON_NOEXT)) {
    // Only display the pause if the file is not empty and contains information
    pausescr();
  }

  if (printfile(SYSTEM_NOEXT)) {
    pausescr();
  }
}

void JumpSubConf() {
  if (okconf(a()->user())) {
    jump_conf(ConferenceType::CONF_SUBS);
  }
}

void KillEMail() {
  if (GuestCheck()) {
    write_inst(INST_LOC_KILLEMAIL, 0, INST_FLAGS_NONE);
    kill_old_email();
  }
}

void LastCallers() {
  if (a()->HasConfigFlag(OP_FLAGS_SHOW_CITY_ST) &&
      (syscfg.sysconfig & sysconfig_extended_info)) {
    bout << "|#2Number Name/Handle               Time  Date  City            ST Cty Modem    ##\r\n";
  } else {
    bout << "|#2Number Name/Handle               Language   Time  Date  Speed                ##\r\n";
  }
  char filler_char = okansi() ? '\xCD' : '=';
  bout << "|#7" << string(79, filler_char) << wwiv::endl;
  printfile(LASTON_TXT);
}

void ReadEMail() {
  readmail(0);
}

void NewMessageScan() {
  if (okconf(a()->user())) {
    bout.nl();
    bout << "|#5New message scan in all conferences? ";
    if (noyes()) {
      NewMsgsAllConfs();
      return;
    }
  }
  write_inst(INST_LOC_SUBS, 65535, INST_FLAGS_NONE);
  newline = false;
  nscan();
  newline = true;
}

void GoodBye() {
  int cycle;
  int ch;

  if (a()->batch().numbatchdl() != 0) {
    bout.nl();
    bout << "|#2Download files in your batch queue (|#1Y/n|#2)? ";
    if (noyes()) {
      batchdl(1);
    }
  }
  string filename = StrCat(a()->language_dir.c_str(), LOGOFF_MAT);
  if (!File::Exists(filename)) {
    filename = StrCat(a()->config()->gfilesdir(), LOGOFF_MAT);
  }
  if (File::Exists(filename)) {
    cycle = 0;
    do {
      bout.cls();
      printfile(filename);
      ch = onek("QFTO", true);
      switch (ch) {
      case 'Q':
        cycle = 1;
        break;
      case 'F':
        write_inst(INST_LOC_FEEDBACK, 0, INST_FLAGS_ONLINE);
        feedback(false);
        a()->UpdateTopScreen();
        break;
      case 'T':
        write_inst(INST_LOC_BANK, 0, INST_FLAGS_ONLINE);
        time_bank();
        break;
      case 'O':
        cycle = 1;
        write_inst(INST_LOC_LOGOFF, 0, INST_FLAGS_NONE);
        bout.cls();
        auto used_this_session = (std::chrono::system_clock::now() - a()->system_logon_time());
        auto secs_used = std::chrono::duration_cast<std::chrono::seconds>(used_this_session);
        bout <<  "Time on   = " << ctim(static_cast<long>(secs_used.count())) << wwiv::endl;
        {
          TempDisablePause disable_pause;
          printfile(LOGOFF_NOEXT);
        }
        a()->user()->SetLastSubNum(a()->current_user_sub_num());
        a()->user()->SetLastDirNum(a()->current_user_dir_num());
        if (okconf(a()->user())) {
          a()->user()->SetLastSubConf(a()->GetCurrentConferenceMessageArea());
          a()->user()->SetLastDirConf(a()->GetCurrentConferenceFileArea());
        }
        Hangup();
        break;
      }
    } while (cycle == 0);
  } else {
    bout.nl(2);
    bout << "|#5Log Off? ";
    if (yesno()) {
      write_inst(INST_LOC_LOGOFF, 0, INST_FLAGS_NONE);
      bout.cls();
      auto used_this_session = (std::chrono::system_clock::now() - a()->system_logon_time());
      auto sec_used = static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(used_this_session).count());
      bout << "Time on   = " << ctim(sec_used) << wwiv::endl;
      {
        TempDisablePause disable_pause;
        printfile(LOGOFF_NOEXT);
      }
      a()->user()->SetLastSubNum(a()->current_user_sub_num());
      a()->user()->SetLastDirNum(a()->current_user_dir_num());
      if (okconf(a()->user())) {
        a()->user()->SetLastSubConf(a()->GetCurrentConferenceMessageArea());
        a()->user()->SetLastDirConf(a()->GetCurrentConferenceFileArea());
      }
      Hangup();
    }
  }
}

void WWIV_PostMessage() {
  irt[0] = 0;
  irt_name[0] = 0;
  grab_quotes(nullptr, nullptr);
  if (a()->usub[0].subnum != -1) {
    post(PostData());
  }
}

void ScanSub() {
  if (a()->usub[0].subnum != -1) {
    write_inst(INST_LOC_SUBS, a()->current_user_sub().subnum, INST_FLAGS_NONE);
    bool nextsub = false;
    qscan(a()->current_user_sub_num(), nextsub);
  }
}

void RemovePost() {
  if (a()->usub[0].subnum != -1) {
    write_inst(INST_LOC_SUBS, a()->current_user_sub().subnum, INST_FLAGS_NONE);
    remove_post();
  }
}

void TitleScan() {
  if (a()->usub[0].subnum != -1) {
    write_inst(INST_LOC_SUBS, a()->current_user_sub().subnum, INST_FLAGS_NONE);
    ScanMessageTitles();
  }
}

void ListUsers() {
  list_users(LIST_USERS_MESSAGE_AREA);
}

void Vote() {
  if (GuestCheck()) {
    write_inst(INST_LOC_VOTE, 0, INST_FLAGS_NONE);
    vote();
  }
}

void ToggleExpert() {
  a()->user()->ToggleStatusFlag(User::expert);
}

void WWIVVersion() {
  bout.cls();
  bout << "|#9WWIV Bulletin Board System " << wwiv_version << beta_version << wwiv::endl;
  bout << "|#9Copyright (C) 1998-2017, WWIV Software Services.\r\n";
  bout << "|#9All Rights Reserved.\r\n\r\n";
  bout << "|#9Licensed under the Apache License.  " << wwiv::endl;
  bout << "|#9Please see |#1http://www.wwivbbs.org/ |#9for more information"
       << wwiv::endl << wwiv::endl;
  bout << "|#9Compile Time  : |#2" << wwiv_date << wwiv::endl;
  bout << "|#9SysOp Name    : |#2" << syscfg.sysopname << wwiv::endl;
  bout << "|#9OS            : |#2" << wwiv::os::os_version_string() << wwiv::endl;
  bout << "|#9Instance      : |#2" << a()->instance_number() << wwiv::endl;

  if (!a()->net_networks.empty()) {
    std::unique_ptr<WStatus> pStatus(a()->status_manager()->GetStatus());
    a()->status_manager()->RefreshStatusCache();
    //bout << wwiv::endl;
    bout << "|#9Networks      : |#2" << "net" << pStatus->GetNetworkVersion() << wwiv::endl;
    for (const auto& n : a()->net_networks) {
      if (!n.sysnum) {
        continue;
      }
      bout << "|#9" << std::setw(14) << std::left << n.name << ":|#2 @" << n.sysnum << wwiv::endl;
    }
  }

  bout.nl(3);
  pausescr();
}

void JumpEdit() {
  write_inst(INST_LOC_CONFEDIT, 0, INST_FLAGS_NONE);
  edit_confs();
}

void BoardEdit() {
  write_inst(INST_LOC_BOARDEDIT, 0, INST_FLAGS_NONE);
  sysoplog() << "@ Ran Board Edit";
  boardedit();
}

void ChainEdit() {
  write_inst(INST_LOC_CHAINEDIT, 0, INST_FLAGS_NONE);
  sysoplog() << "@ Ran Chain Edit";
  chainedit();
}

void ToggleChat() {
  bout.nl(2);
  bool bOldAvail = sysop2();
  ToggleScrollLockKey();
  bool bNewAvail = sysop2();
  if (bOldAvail != bNewAvail) {
    bout << ((bNewAvail) ? "|#5Sysop now available\r\n" : "|#3Sysop now unavailable\r\n");
    sysoplog() << "@ Changed sysop available status";
  } else {
    bout << "|#6Unable to toggle Sysop availability (hours restriction)\r\n";
  }
  a()->UpdateTopScreen();
}

void ChangeUser() {
  write_inst(INST_LOC_CHUSER, 0, INST_FLAGS_NONE);
  chuser();
}

void DirEdit() {
  write_inst(INST_LOC_DIREDIT, 0, INST_FLAGS_NONE);
  sysoplog() << "@ Ran Directory Edit";
  dlboardedit();
}

void EventEdit() {
  write_inst(INST_LOC_EVENTEDIT, 0, INST_FLAGS_NONE);
  sysoplog() << "- Ran Event Editor";
  eventedit();
}

void LoadTextFile() {
  bout.nl();
  bout << "|#9Enter Filename: ";
  string fileName = Input1("", 50, true, InputMode::FULL_PATH_NAME);
  if (!fileName.empty()) {
    bout.nl();
    bout << "|#5Allow editing? ";
    if (yesno()) {
      bout.nl();
      LoadFileIntoWorkspace(fileName, false);
    } else {
      bout.nl();
      LoadFileIntoWorkspace(fileName, true);
    }
  }
}

void EditText() {
  write_inst(INST_LOC_TEDIT, 0, INST_FLAGS_NONE);
  bout.nl();
  bout << "|#7Enter Filespec: ";
  string fileName = input(50);
  if (!fileName.empty()) {
    external_text_edit(fileName.c_str(), "", 500, ".", MSGED_FLAG_NO_TAGLINE);
  }
}

void EditBulletins() {
  write_inst(INST_LOC_GFILEEDIT, 0, INST_FLAGS_NONE);
  sysoplog() << "@ Ran Gfile Edit";
  gfileedit();
}

void ReadAllMail() {
  write_inst(INST_LOC_MAILR, 0, INST_FLAGS_NONE);
  sysoplog() << "@ Read mail";
  mailr();
}

void ReloadMenus() {
  write_inst(INST_LOC_RELOAD, 0, INST_FLAGS_NONE);
  read_new_stuff();
}

void ResetQscan() {
  bout << "|#5Reset all QScan/NScan pointers (For All Users)? ";
  if (yesno()) {
    write_inst(INST_LOC_RESETQSCAN, 0, INST_FLAGS_NONE);
    for (int i = 0; i <= a()->users()->GetNumberOfUserRecords(); i++) {
      read_qscn(i, qsc, true);
      memset(qsc_p, 0, syscfg.qscn_len - 4 * (1 + ((syscfg.max_dirs + 31) / 32) + ((
          syscfg.max_subs + 31) / 32)));
      write_qscn(i, qsc, true);
    }
    read_qscn(1, qsc, false);
    close_qscn();
  }
}

void MemoryStatus() {
  std::unique_ptr<WStatus> pStatus(a()->status_manager()->GetStatus());
  bout.nl();
  bout << "Qscanptr        : " << pStatus->GetQScanPointer() << wwiv::endl;
}

void PackMessages() {
  bout.nl();
  bout << "|#5Pack all subs? ";
  if (yesno()) {
    wwiv::bbs::TempDisablePause disable_pause;
    if (!pack_all_subs()) {
      bout << "|#6Aborted.\r\n";
    }
  } else {
    pack_sub(a()->current_user_sub().subnum);
  }
}

void InitVotes() {
  write_inst(INST_LOC_VOTE, 0, INST_FLAGS_NONE);
  sysoplog() << "@ Ran Ivotes";
  ivotes();
}

void ReadLog() {
  const string sysop_log_file = GetSysopLogFileName(date());
  print_local_file(sysop_log_file.c_str());
}

void ReadNetLog() {
  print_local_file("net.log");
}

void PrintPending() {
  print_pending_list();
}

void PrintStatus() {
  prstatus();
}

void TextEdit() {
  write_inst(INST_LOC_TEDIT, 0, INST_FLAGS_NONE);
  sysoplog() << "@ Ran Text Edit";
  text_edit();
}

void UserEdit() {
  write_inst(INST_LOC_UEDIT, 0, INST_FLAGS_NONE);
  sysoplog() << "@ Ran User Edit";
  uedit(a()->usernum, UEDIT_NONE);
}

void VotePrint() {
  write_inst(INST_LOC_VOTEPRINT, 0, INST_FLAGS_NONE);
  voteprint();
}

void YesterdaysLog() {
  std::unique_ptr<WStatus> pStatus(a()->status_manager()->GetStatus());
  print_local_file(pStatus->GetLogFileName(1));
}

void ZLog() {
  zlog();
}

void ViewNetDataLog() {
  bool done = false;

  while (!done && !hangup) {
    bout.nl();
    bout << "|#9Which NETDAT log (0-2,Q)? ";
    char ch = onek("Q012");
    switch (ch) {
    case 'Q':
      done = true;
      break;
    case '0':
      print_local_file("netdat0.log");
      break;
    case '1':
      print_local_file("netdat1.log");
      break;
    case '2':
      print_local_file("netdat2.log");
      break;
    }
  }
}

void UploadPost() {
  upload_post();
}

void WhoIsOnline() {
  multi_instance();
  bout.nl();
  pausescr();
}

void NewMsgsAllConfs() {
  bool ac = false;

  write_inst(INST_LOC_SUBS, a()->current_user_sub().subnum, INST_FLAGS_NONE);
  newline = false;
  if (uconfsub[1].confnum != -1 && okconf(a()->user())) {
    ac = true;
    tmp_disable_conf(true);
  }
  nscan();
  newline = true;
  if (ac == true) {
    tmp_disable_conf(false);
  }
}

void MultiEmail() {
  slash_e();
}

void InternetEmail() {
  send_inet_email();
}

void NewMsgScanFromHere() {
  newline = false;
  nscan(a()->current_user_sub_num());
  newline = true;
}

void ValidateScan() {
  newline = false;
  valscan();
  newline = true;
}

void ChatRoom() {
  write_inst(INST_LOC_CHATROOM, 0, INST_FLAGS_NONE);
  chat_room();
}

void ClearQScan() {
  bout.nl();
  bout << "|#5Mark messages as read on [C]urrent sub or [A]ll subs (A/C/Q)? ";
  char ch = onek("QAC\r");
  switch (ch) {
  case 'Q':
  case RETURN:
    break;
  case 'A': {
    std::unique_ptr<WStatus> pStatus(a()->status_manager()->GetStatus());
    for (int i = 0; i < syscfg.max_subs; i++) {
      qsc_p[i] = pStatus->GetQScanPointer() - 1L;
    }
    bout.nl();
    bout << "Q-Scan pointers cleared.\r\n";
  }
  break;
  case 'C':
    std::unique_ptr<WStatus> pStatus(a()->status_manager()->GetStatus());
    bout.nl();
    qsc_p[a()->current_user_sub().subnum] = pStatus->GetQScanPointer() - 1L;
    bout << "Messages on " << a()->subs().sub(a()->current_user_sub().subnum).name
         << " marked as read.\r\n";
    break;
  }
}

void FastGoodBye() {
  if (a()->batch().numbatchdl() != 0) {
    bout.nl();
    bout << "|#2Download files in your batch queue (|#1Y/n|#2)? ";
    if (noyes()) {
      batchdl(1);
    }
  }
  a()->user()->SetLastSubNum(a()->current_user_sub_num());
  a()->user()->SetLastDirNum(a()->current_user_dir_num());
  if (okconf(a()->user())) {
    a()->user()->SetLastSubConf(a()->GetCurrentConferenceMessageArea());
    a()->user()->SetLastDirConf(a()->GetCurrentConferenceFileArea());
  }
  Hangup();
}

void NewFilesAllConfs() {
  bout.nl();
  int ac = 0;
  if (uconfsub[1].confnum != -1 && okconf(a()->user())) {
    ac = 1;
    tmp_disable_conf(true);
  }
  nscanall();
  if (ac) {
    tmp_disable_conf(false);
  }
}

void ReadIDZ() {
  bout.nl();
  bout << "|#5Read FILE_ID.DIZ for all directories? ";
  if (yesno()) {
    read_idz_all();
  } else {
    read_idz(1, a()->current_user_dir_num());
  }
}

void RemoveNotThere() {
  removenotthere();
}

void UploadAllDirs() {
  bout.nl(2);
  bool ok = true;
  for (size_t nDirNum = 0; nDirNum < a()->directories.size() && a()->udir[nDirNum].subnum >= 0 && ok && !hangup; nDirNum++) {
    bout << "|#9Now uploading files for: |#2" << a()->directories[a()->udir[nDirNum].subnum].name << wwiv::endl;
    ok = uploadall(nDirNum);
  }
}


void UploadCurDir() {
  uploadall(a()->current_user_dir_num());
}

void RenameFiles() {
  rename_file();
}

void MoveFiles() {
  move_file();
}

void SortDirs() {
  bout.nl();
  bout << "|#5Sort all dirs? ";
  bool bSortAll = yesno();
  bout.nl();
  bout << "|#5Sort by date? ";

  int nType = 0;
  if (yesno()) {
    nType = 2;
  }

  TempDisablePause disable_paise;
  if (bSortAll) {
    sort_all(nType);
  } else {
    sortdir(a()->current_user_dir().subnum, nType);
  }
}

void ReverseSort() {
  bout.nl();
  bout << "|#5Sort all dirs? ";
  bool bSortAll = yesno();
  bout.nl();
  TempDisablePause disable_pause;
  if (bSortAll) {
    sort_all(1);
  } else {
    sortdir(a()->current_user_dir().subnum, 1);
  }
}

void AllowEdit() {
  edit_database();
}

void UploadFilesBBS() {
  char s2[81];

  bout.nl();
  bout << "|#21|#9) PCB, RBBS   - <filename> <size> <date> <description>\r\n";
  bout << "|#22|#9) QBBS format - <filename> <description>\r\n";
  bout.nl();
  bout << "|#Select Format (1,2,Q) : ";
  char ch = onek("Q12");
  bout.nl();
  if (ch != 'Q') {
    int nType = 0;
    bout << "|#9Enter Filename (wildcards allowed).\r\n|#7: ";
    bout.mpl(77);
    inputl(s2, 80);
    switch (ch) {
    case '1':
      nType = 2;
      break;
    case '2':
      nType = 0;
      break;
    default:
      nType = 0;
      break;
    }
    upload_files(s2, a()->current_user_dir_num(), nType);
  }
}

void UpDirConf() {
  if (okconf(a()->user())) {
    if (a()->GetCurrentConferenceFileArea() < dirconfnum - 1
        && uconfdir[a()->GetCurrentConferenceFileArea() + 1].confnum >= 0) {
      a()->SetCurrentConferenceFileArea(a()->GetCurrentConferenceFileArea() + 1);
    } else {
      a()->SetCurrentConferenceFileArea(0);
    }
    setuconf(ConferenceType::CONF_DIRS, a()->GetCurrentConferenceFileArea(), -1);
  }
}

void UpDir() {
  if (a()->current_user_dir_num() < size_int(a()->directories) - 1
      && a()->udir[a()->current_user_dir_num() + 1].subnum >= 0) {
    a()->set_current_user_dir_num(a()->current_user_dir_num() + 1);
  } else {
    a()->set_current_user_dir_num(0);
  }
}

void DownDirConf() {
  if (okconf(a()->user())) {
    if (a()->GetCurrentConferenceFileArea() > 0) {
      a()->SetCurrentConferenceFileArea(a()->GetCurrentConferenceFileArea());
    } else {
      while (uconfdir[a()->GetCurrentConferenceFileArea() + 1].confnum >= 0
             && a()->GetCurrentConferenceFileArea() < dirconfnum - 1) {
        a()->SetCurrentConferenceFileArea(a()->GetCurrentConferenceFileArea() + 1);
      }
    }
    setuconf(ConferenceType::CONF_DIRS, a()->GetCurrentConferenceFileArea(), -1);
  }
}

void DownDir() {
  if (a()->current_user_dir_num() > 0) {
    a()->set_current_user_dir_num(a()->current_user_dir_num() - 1);
  } else {
    while (a()->udir[a()->current_user_dir_num() + 1].subnum >= 0 &&
           a()->current_user_dir_num() < size_int(a()->directories) - 1) {
      a()->set_current_user_dir_num(a()->current_user_dir_num() + 1);
    }
  }
}

void ListUsersDL() {
  list_users(LIST_USERS_FILE_AREA);
}

void PrintDSZLog() {
  if (File::Exists(a()->dsz_logfile_name_)) {
    print_local_file(a()->dsz_logfile_name_);
  }
}

void PrintDevices() {
  print_devices();
}

void ViewArchive() {
  arc_l();
}

void BatchMenu() {
  batchdl(0);
}

void Download() {
  play_sdf(DOWNLOAD_NOEXT, false);
  printfile(DOWNLOAD_NOEXT);
  download();
}

void TempExtract() {
  temp_extract();
}

void FindDescription() {
  finddescription();
}

void TemporaryStuff() {
  temporary_stuff();
}

void JumpDirConf() {
  if (okconf(a()->user())) {
    jump_conf(ConferenceType::CONF_DIRS);
  }
}

void ConfigFileList() {
  if (okansi()) {
    config_file_list();
  }
}

void ListFiles() {
  listfiles();
}

void NewFileScan() {
  if (a()->HasConfigFlag(OP_FLAGS_SETLDATE)) {
    SetNewFileScanDate();
  }
  bool abort = false;
  bool need_title = true;
  bout.nl();
  bout << "|#5Search all directories? ";
  if (yesno()) {
    nscanall();
  } else {
    bout.nl();
    nscandir(a()->current_user_dir_num(), need_title, &abort);
    if (!a()->filelist.empty()) {
      endlist(2);
    } else {
      bout.nl();
      bout << "|#2No new files found.\r\n";
    }
  }
}

void RemoveFiles() {
  if (GuestCheck()) {
    removefile();
  }
}

void SearchAllFiles() {
  searchall();
}

void XferDefaults() {
  if (GuestCheck()) {
    xfer_defaults();
  }
}

void Upload() {
  play_sdf(UPLOAD_NOEXT, false);
  printfile(UPLOAD_NOEXT);
  if (a()->user()->IsRestrictionValidate() || a()->user()->IsRestrictionUpload() ||
      (syscfg.sysconfig & sysconfig_all_sysop)) {
    if (syscfg.newuploads < a()->directories.size()) {
      upload(static_cast<int>(syscfg.newuploads));
    } else {
      upload(0);
    }
  } else {
    upload(a()->current_user_dir().subnum);
  }
}

void YourInfoDL() {
  YourInfo();
}

void UploadToSysop() {
  printfile(ZUPLOAD_NOEXT);
  bout.nl(2);
  bout << "Sending file to sysop :-\r\n\n";
  upload(0);
}

void ReadAutoMessage() {
  read_automessage();
}

void GuestApply() {
  if (guest_user) {
    newuser();
  } else {
    bout << "You already have an account on here!\r\n\r\n";
  }
}

void AttachFile() {
  attach_file(0);
}

bool GuestCheck() {
  if (guest_user) {
    bout << "|#6This command is only for registered users.\r\n";
    return false;
  }
  return true;
}

void SetSubNumber(const char *pszSubKeys) {
  for (size_t i = 0; (i < a()->subs().subs().size()) && (a()->usub[i].subnum != -1); i++) {
    if (wwiv::strings::IsEquals(a()->usub[i].keys, pszSubKeys)) {
      a()->set_current_user_sub_num(i);
    }
  }
}

void SetDirNumber(const char *pszDirectoryKeys) {
  for (size_t i = 0; i < a()->directories.size(); i++) {
    if (IsEquals(a()->udir[i].keys, pszDirectoryKeys)) {
      a()->set_current_user_dir_num(i);
    }
  }
}
