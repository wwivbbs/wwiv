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

#include <memory>
#include <string>

#include "bbs/wwiv.h"
#include "bbs/external_edit.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/keycodes.h"
#include "bbs/menu.h"
#include "bbs/menusupp.h"
#include "bbs/pause.h"
#include "bbs/wconstants.h"
#include "bbs/printfile.h"
#include "bbs/wstatus.h"
#include "core/strings.h"

using std::string;
using wwiv::bbs::InputMode;
using wwiv::bbs::TempDisablePause;

void UnQScan() {
  bout.nl();
  bout << "|#9Mark messages as unread on [C]urrent sub or [A]ll subs (A/C/Q)? ";
  char ch = onek("QAC\r");
  switch (ch) {
  case 'Q':
  case RETURN:
    break;
  case 'A': {
    for (int i = 0; i < session()->GetMaxNumberMessageAreas(); i++) {
      qsc_p[i] = 0;
    }
    bout << "\r\nQ-Scan pointers reset.\r\n\n";
  }
  break;
  case 'C': {
    bout.nl();
    qsc_p[usub[session()->GetCurrentMessageArea()].subnum] = 0;
    bout << "Messages on " << subboards[usub[session()->GetCurrentMessageArea()].subnum].name <<
                       " marked as unread.\r\n";
  }
  break;
  }
}

void DirList() {
  dirlist(0);
}

void UpSubConf() {
  if (okconf(session()->user())) {
    if ((session()->GetCurrentConferenceMessageArea() < subconfnum - 1)
        && (uconfsub[session()->GetCurrentConferenceMessageArea() + 1].confnum >= 0)) {
      session()->SetCurrentConferenceMessageArea(session()->GetCurrentConferenceMessageArea() + 1);
    } else {
      session()->SetCurrentConferenceMessageArea(0);
    }
    setuconf(CONF_SUBS, session()->GetCurrentConferenceMessageArea(), -1);
  }
}

void DownSubConf() {
  if (okconf(session()->user())) {
    if (session()->GetCurrentConferenceMessageArea() > 0) {
      session()->SetCurrentConferenceMessageArea(session()->GetCurrentConferenceMessageArea() - 1);
    } else {
      while (uconfsub[session()->GetCurrentConferenceMessageArea() + 1].confnum >= 0
             && session()->GetCurrentConferenceMessageArea() < subconfnum - 1) {
        session()->SetCurrentConferenceMessageArea(session()->GetCurrentConferenceMessageArea() + 1);
      }
    }
    setuconf(CONF_SUBS, session()->GetCurrentConferenceMessageArea(), -1);
  }
}

void DownSub() {
  if (session()->GetCurrentMessageArea() > 0) {
    session()->SetCurrentMessageArea(session()->GetCurrentMessageArea() - 1);
  } else {
    while (usub[session()->GetCurrentMessageArea() + 1].subnum >= 0 &&
           session()->GetCurrentMessageArea() < session()->num_subs - 1) {
      session()->SetCurrentMessageArea(session()->GetCurrentMessageArea() + 1);
    }
  }
}

void UpSub() {
  if (session()->GetCurrentMessageArea() < session()->num_subs - 1 &&
      usub[session()->GetCurrentMessageArea() + 1].subnum >= 0) {
    session()->SetCurrentMessageArea(session()->GetCurrentMessageArea() + 1);
  } else {
    session()->SetCurrentMessageArea(0);
  }
}

void ValidateUser() {
  bout.nl(2);
  bout << "|#9Enter user name or number:\r\n:";
  string userName;
  input(&userName, 30, true);
  int nUserNum = finduser1(userName.c_str());
  if (nUserNum > 0) {
    sysoplogf("@ Validated user #%d", nUserNum);
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

void Defaults(MenuInstanceData * pMenuData) {
  if (GuestCheck()) {
    write_inst(INST_LOC_DEFAULTS, 0, INST_FLAGS_NONE);
    if (printfile(DEFAULTS_NOEXT)) {
      pausescr();
    }
    defaults(pMenuData);
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
  if (okconf(session()->user())) {
    jump_conf(CONF_SUBS);
  }
}

void KillEMail() {
  if (GuestCheck()) {
    write_inst(INST_LOC_KILLEMAIL, 0, INST_FLAGS_NONE);
    kill_old_email();
  }
}

void LastCallers() {
  std::unique_ptr<WStatus> pStatus(application()->GetStatusManager()->GetStatus());
  if (pStatus->GetNumCallsToday() > 0) {
    if (application()->HasConfigFlag(OP_FLAGS_SHOW_CITY_ST) &&
        (syscfg.sysconfig & sysconfig_extended_info)) {
      bout << "|#2Number Name/Handle               Time  Date  City            ST Cty Modem    ##\r\n";
    } else {
      bout << "|#2Number Name/Handle               Language   Time  Date  Speed                ##\r\n";
    }
    unsigned char i = okansi() ? 205 : '=';
    bout << "|#7" << string(79, i) << wwiv::endl;
  }
  printfile(USER_LOG);
}

void ReadEMail() {
  readmail(0);
}

void NewMessageScan() {
  if (okconf(session()->user())) {
    bout.nl();
    bout << "|#5New message scan in all conferences? ";
    if (noyes()) {
      NewMsgsAllConfs();
      return;
    }
  }
  write_inst(INST_LOC_SUBS, 65535, INST_FLAGS_NONE);
  express = false;
  expressabort = false;
  newline = false;
  preload_subs();
  nscan();
  newline = true;
}

void GoodBye() {
  char szFileName[ MAX_PATH ];
  int cycle;
  int ch;

  if (session()->numbatchdl != 0) {
    bout.nl();
    bout << "|#2Download files in your batch queue (|#1Y/n|#2)? ";
    if (noyes()) {
      batchdl(1);
    }
  }
  sprintf(szFileName, "%s%s", session()->language_dir.c_str(), LOGOFF_MAT);
  if (!File::Exists(szFileName)) {
    sprintf(szFileName, "%s%s", syscfg.gfilesdir, LOGOFF_MAT);
  }
  if (File::Exists(szFileName)) {
    cycle = 0;
    do {
      bout.cls();
      printfile(szFileName);
      ch = onek("QFTO", true);
      switch (ch) {
      case 'Q':
        cycle = 1;
        break;
      case 'F':
        write_inst(INST_LOC_FEEDBACK, 0, INST_FLAGS_ONLINE);
        feedback(false);
        application()->UpdateTopScreen();
        break;
      case 'T':
        write_inst(INST_LOC_BANK, 0, INST_FLAGS_ONLINE);
        time_bank();
        break;
      case 'O':
        cycle = 1;
        write_inst(INST_LOC_LOGOFF, 0, INST_FLAGS_NONE);
        bout.cls();
        bout <<  "Time on   = " << ctim(timer() - timeon) << wwiv::endl;
        {
          TempDisablePause disable_pause;
          printfile(LOGOFF_NOEXT);
        }
        session()->user()->SetLastSubNum(session()->GetCurrentMessageArea());
        session()->user()->SetLastDirNum(session()->GetCurrentFileArea());
        if (okconf(session()->user())) {
          session()->user()->SetLastSubConf(session()->GetCurrentConferenceMessageArea());
          session()->user()->SetLastDirConf(session()->GetCurrentConferenceFileArea());
        }
        hangup = true;
        break;
      }
    } while (cycle == 0);
  } else {
    bout.nl(2);
    bout << "|#5Log Off? ";
    if (yesno()) {
      write_inst(INST_LOC_LOGOFF, 0, INST_FLAGS_NONE);
      bout.cls();
      bout << "Time on   = " << ctim(timer() - timeon) << wwiv::endl;
      {
        TempDisablePause disable_pause;
        printfile(LOGOFF_NOEXT);
      }
      session()->user()->SetLastSubNum(session()->GetCurrentMessageArea());
      session()->user()->SetLastDirNum(session()->GetCurrentFileArea());
      if (okconf(session()->user())) {
        session()->user()->SetLastSubConf(session()->GetCurrentConferenceMessageArea());
        session()->user()->SetLastDirConf(session()->GetCurrentConferenceFileArea());
      }
      hangup = true;
    }
  }
}

void WWIV_PostMessage() {
  irt[0] = 0;
  irt_name[0] = 0;
  grab_quotes(nullptr, nullptr);
  if (usub[0].subnum != -1) {
    post();
  }
}

void ScanSub() {
  if (usub[0].subnum != -1) {
    write_inst(INST_LOC_SUBS, usub[session()->GetCurrentMessageArea()].subnum, INST_FLAGS_NONE);
    int i = 0;
    express = expressabort = false;
    qscan(session()->GetCurrentMessageArea(), &i);
  }
}

void RemovePost() {
  if (usub[0].subnum != -1) {
    write_inst(INST_LOC_SUBS, usub[session()->GetCurrentMessageArea()].subnum, INST_FLAGS_NONE);
    remove_post();
  }
}

void TitleScan() {
  if (usub[0].subnum != -1) {
    write_inst(INST_LOC_SUBS, usub[session()->GetCurrentMessageArea()].subnum, INST_FLAGS_NONE);
    express = false;
    expressabort = false;
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
  session()->user()->ToggleStatusFlag(WUser::expert);
}

void ExpressScan() {
  express = true;
  expressabort = false;
  TempDisablePause disable_pause;
  newline = false;
  preload_subs();
  nscan();
  newline = true;
  express = false;
  expressabort = false;
}

void WWIVVersion() {
  bout.cls();
  bout << "|#9WWIV Bulletin Board System " << wwiv_version << " " << beta_version << wwiv::endl;
  bout << "|#9Copyright (C) 1998-2014, WWIV Software Services.\r\n";
  bout << "|#9All Rights Reserved.\r\n\r\n";
  bout << "|#9Licensed under the Apache License.  " << wwiv::endl;
  bout << "|#9Please see |#1http://wwiv.sourceforge.net |#9for more information"
       << wwiv::endl << wwiv::endl;
  bout << "|#9Compile Time  : |#2" << wwiv_date << wwiv::endl;
  bout << "|#9SysOp Name:   : |#2" << syscfg.sysopname << wwiv::endl;
  bout.nl(3);
  pausescr();
}

void InstanceEdit() {
  instance_edit();
}

void JumpEdit() {
  write_inst(INST_LOC_CONFEDIT, 0, INST_FLAGS_NONE);
  edit_confs();
}

void BoardEdit() {
  write_inst(INST_LOC_BOARDEDIT, 0, INST_FLAGS_NONE);
  sysoplog("@ Ran Board Edit");
  boardedit();
}

void ChainEdit() {
  write_inst(INST_LOC_CHAINEDIT, 0, INST_FLAGS_NONE);
  sysoplog("@ Ran Chain Edit");
  chainedit();
}

void ToggleChat() {
  bout.nl(2);
  bool bOldAvail = sysop2();
  ToggleScrollLockKey();
  bool bNewAvail = sysop2();
  if (bOldAvail != bNewAvail) {
    bout << ((bNewAvail) ? "|#5Sysop now available\r\n" : "|#3Sysop now unavailable\r\n");
    sysoplog("@ Changed sysop available status");
  } else {
    bout << "|#6Unable to toggle Sysop availability (hours restriction)\r\n";
  }
  application()->UpdateTopScreen();
}

void ChangeUser() {
  write_inst(INST_LOC_CHUSER, 0, INST_FLAGS_NONE);
  chuser();
}

void CallOut() {
  force_callout(2);
}

void Debug() {
  int new_level = (File::GetDebugLevel() + 1) % 5;
  File::SetDebugLevel(new_level);
  bout << "|#5New Debug Level: " << new_level << wwiv::endl;
}

void DirEdit() {
  write_inst(INST_LOC_DIREDIT, 0, INST_FLAGS_NONE);
  sysoplog("@ Ran Directory Edit");
  dlboardedit();
}

void EventEdit() {
  write_inst(INST_LOC_EVENTEDIT, 0, INST_FLAGS_NONE);
  sysoplog("- Ran Event Editor");
  eventedit();
}

void LoadTextFile() {
  bout.nl();
  bout << "|#9Enter Filename: ";
  string fileName;
  Input1(&fileName, "", 50, true, InputMode::FULL_PATH_NAME);
  if (!fileName.empty()) {
    bout.nl();
    bout << "|#5Allow editing? ";
    if (yesno()) {
      bout.nl();
      LoadFileIntoWorkspace(fileName.c_str(), false);
    } else {
      bout.nl();
      LoadFileIntoWorkspace(fileName.c_str(), true);
    }
  }
}

void EditText() {
  write_inst(INST_LOC_TEDIT, 0, INST_FLAGS_NONE);
  bout.nl();
  bout << "|#7Enter Filespec: ";
  string fileName;
  input(&fileName, 50);
  if (!fileName.empty()) {
    external_text_edit(fileName.c_str(), "", 500, ".", MSGED_FLAG_NO_TAGLINE);
  }
}

void EditBulletins() {
  write_inst(INST_LOC_GFILEEDIT, 0, INST_FLAGS_NONE);
  sysoplog("@ Ran Gfile Edit");
  gfileedit();
}

void ReadAllMail() {
  write_inst(INST_LOC_MAILR, 0, INST_FLAGS_NONE);
  sysoplog("@ Read mail");
  mailr();
}

void RebootComputer() {
  // Does nothing.
}

void ReloadMenus() {
  write_inst(INST_LOC_RELOAD, 0, INST_FLAGS_NONE);
  read_new_stuff();
}

void ResetFiles() {
  write_inst(INST_LOC_RESETF, 0, INST_FLAGS_NONE);
  reset_files();
}

void ResetQscan() {
  bout << "|#5Reset all QScan/NScan pointers (For All Users)? ";
  if (yesno()) {
    write_inst(INST_LOC_RESETQSCAN, 0, INST_FLAGS_NONE);
    for (int i = 0; i <= application()->users()->GetNumberOfUserRecords(); i++) {
      read_qscn(i, qsc, true);
      memset(qsc_p, 0, syscfg.qscn_len - 4 * (1 + ((session()->GetMaxNumberFileAreas() + 31) / 32) + ((
          session()->GetMaxNumberMessageAreas() + 31) / 32)));
      write_qscn(i, qsc, true);
    }
    read_qscn(1, qsc, false);
    close_qscn();
  }
}

void MemoryStatus() {
  std::unique_ptr<WStatus> pStatus(application()->GetStatusManager()->GetStatus());
  bout.nl();
  bout << "Qscanptr        : " << pStatus->GetQScanPointer() << wwiv::endl;
}

void PackMessages() {
  bout.nl();
  bout << "|#5Pack all subs? ";
  if (yesno()) {
    pack_all_subs();
  } else {
    pack_sub(usub[session()->GetCurrentMessageArea()].subnum);
  }
}

void InitVotes() {
  write_inst(INST_LOC_VOTE, 0, INST_FLAGS_NONE);
  sysoplog("@ Ran Ivotes");
  ivotes();
}

void ReadLog() {
  const string sysop_log_file = GetSysopLogFileName(date());
  print_local_file(sysop_log_file.c_str());
}

void ReadNetLog() {
  print_local_file("NET.LOG");
}

void PrintPending() {
  print_pending_list();
}

void PrintStatus() {
  prstatus(false);
}

void TextEdit() {
  write_inst(INST_LOC_TEDIT, 0, INST_FLAGS_NONE);
  sysoplog("@ Ran Text Edit");
  text_edit();
}

void UserEdit() {
  write_inst(INST_LOC_UEDIT, 0, INST_FLAGS_NONE);
  sysoplog("@ Ran User Edit");
  uedit(session()->usernum, UEDIT_NONE);
}

void VotePrint() {
  write_inst(INST_LOC_VOTEPRINT, 0, INST_FLAGS_NONE);
  voteprint();
}

void YesturdaysLog() {
  std::unique_ptr<WStatus> pStatus(application()->GetStatusManager()->GetStatus());
  print_local_file(pStatus->GetLogFileName());
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

void NetListing() {
  print_net_listing(false);
}

void WhoIsOnline() {
  multi_instance();
  bout.nl();
  pausescr();
}

void NewMsgsAllConfs() {
  bool ac = false;

  write_inst(INST_LOC_SUBS, usub[session()->GetCurrentMessageArea()].subnum, INST_FLAGS_NONE);
  express = false;
  expressabort = false;
  newline = false;
  if (uconfsub[1].confnum != -1 && okconf(session()->user())) {
    ac = true;
    tmp_disable_conf(true);
  }
  preload_subs();
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
  preload_subs();
  nscan(session()->GetCurrentMessageArea());
  newline = true;
}

void ValidateScan() {
  newline = false;
  valscan();
  newline = true;
}

void ChatRoom() {
  write_inst(INST_LOC_CHATROOM, 0, INST_FLAGS_NONE);
  if (File::Exists("WWIVCHAT.EXE")) {
    std::ostringstream cmdline;
    cmdline << "WWIVCHAT.EXE " << create_chain_file();
    ExecuteExternalProgram(cmdline.str(), application()->GetSpawnOptions(SPAWNOPT_CHAT));
  } else {
    chat_room();
  }
}

void DownloadPosts() {
  if (application()->HasConfigFlag(OP_FLAGS_SLASH_SZ)) {
    bout << "|#5This could take quite a while.  Are you sure? ";
    if (yesno()) {
      bout << "Please wait...\r\n";
      session()->localIO()->set_x_only(1, "posts.txt", 0);
      bool ac = false;
      if (uconfsub[1].confnum != -1 && okconf(session()->user())) {
        ac = true;
        tmp_disable_conf(true);
      }
      preload_subs();
      nscan();
      if (ac) {
        tmp_disable_conf(false);
      }
      session()->localIO()->set_x_only(0, nullptr, 0);
      add_arc("offline", "posts.txt", 0);
      download_temp_arc("offline", false);
    }
  }
}

void DownloadFileList() {
  if (application()->HasConfigFlag(OP_FLAGS_SLASH_SZ)) {
    bout << "|#5This could take quite a while.  Are you sure? ";
    if (yesno()) {
      bout << "Please wait...\r\n";
      session()->localIO()->set_x_only(1, "files.txt", 1);
      searchall();
      session()->localIO()->set_x_only(0, nullptr, 0);
      add_arc("temp", "files.txt", 0);
      download_temp_arc("temp", false);
    }
  }
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
    std::unique_ptr<WStatus> pStatus(application()->GetStatusManager()->GetStatus());
    for (int i = 0; i < session()->GetMaxNumberMessageAreas(); i++) {
      qsc_p[i] = pStatus->GetQScanPointer() - 1L;
    }
    bout.nl();
    bout << "Q-Scan pointers cleared.\r\n";
  }
  break;
  case 'C':
    std::unique_ptr<WStatus> pStatus(application()->GetStatusManager()->GetStatus());
    bout.nl();
    qsc_p[usub[session()->GetCurrentMessageArea()].subnum] = pStatus->GetQScanPointer() - 1L;
    bout << "Messages on " << subboards[usub[session()->GetCurrentMessageArea()].subnum].name <<
                       " marked as read.\r\n";
    break;
  }
}

void FastGoodBye() {
  if (session()->numbatchdl != 0) {
    bout.nl();
    bout << "|#2Download files in your batch queue (|#1Y/n|#2)? ";
    if (noyes()) {
      batchdl(1);
    } else {
      hangup = true;
    }
  } else {
    hangup = true;
  }
  session()->user()->SetLastSubNum(session()->GetCurrentMessageArea());
  session()->user()->SetLastDirNum(session()->GetCurrentFileArea());
  if (okconf(session()->user())) {
    session()->user()->SetLastSubConf(session()->GetCurrentConferenceMessageArea());
    session()->user()->SetLastDirConf(session()->GetCurrentConferenceFileArea());
  }
}

void NewFilesAllConfs() {
  bout.nl();
  int ac = 0;
  if (uconfsub[1].confnum != -1 && okconf(session()->user())) {
    ac = 1;
    tmp_disable_conf(true);
  }
  g_num_listed = 0;
  session()->tagging = 1;
  session()->titled = 1;
  nscanall();
  session()->tagging = 0;
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
    read_idz(1, session()->GetCurrentFileArea());
  }
}

void RemoveNotThere() {
  removenotthere();
}

void UploadAllDirs() {
  bout.nl(2);
  bool ok = true;
  for (int nDirNum = 0; nDirNum < session()->num_dirs && udir[nDirNum].subnum >= 0 && ok && !hangup; nDirNum++) {
    bout << "|#9Now uploading files for: |#2" << directories[udir[nDirNum].subnum].name << wwiv::endl;
    ok = uploadall(nDirNum);
  }
}


void UploadCurDir() {
  uploadall(session()->GetCurrentFileArea());
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
    sortdir(udir[session()->GetCurrentFileArea()].subnum, nType);
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
    sortdir(udir[session()->GetCurrentFileArea()].subnum, 1);
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
    upload_files(s2, session()->GetCurrentFileArea(), nType);
  }
}

void UpDirConf() {
  if (okconf(session()->user())) {
    if (session()->GetCurrentConferenceFileArea() < dirconfnum - 1
        && uconfdir[session()->GetCurrentConferenceFileArea() + 1].confnum >= 0) {
      session()->SetCurrentConferenceFileArea(session()->GetCurrentConferenceFileArea() + 1);
    } else {
      session()->SetCurrentConferenceFileArea(0);
    }
    setuconf(CONF_DIRS, session()->GetCurrentConferenceFileArea(), -1);
  }
}

void UpDir() {
  if (session()->GetCurrentFileArea() < session()->num_dirs - 1
      && udir[session()->GetCurrentFileArea() + 1].subnum >= 0) {
    session()->SetCurrentFileArea(session()->GetCurrentFileArea() + 1);
  } else {
    session()->SetCurrentFileArea(0);
  }
}

void DownDirConf() {
  if (okconf(session()->user())) {
    if (session()->GetCurrentConferenceFileArea() > 0) {
      session()->SetCurrentConferenceFileArea(session()->GetCurrentConferenceFileArea());
    } else {
      while (uconfdir[session()->GetCurrentConferenceFileArea() + 1].confnum >= 0
             && session()->GetCurrentConferenceFileArea() < dirconfnum - 1) {
        session()->SetCurrentConferenceFileArea(session()->GetCurrentConferenceFileArea() + 1);
      }
    }
    setuconf(CONF_DIRS, session()->GetCurrentConferenceFileArea(), -1);
  }
}

void DownDir() {
  if (session()->GetCurrentFileArea() > 0) {
    session()->SetCurrentFileArea(session()->GetCurrentFileArea() - 1);
  } else {
    while (udir[session()->GetCurrentFileArea() + 1].subnum >= 0 &&
           session()->GetCurrentFileArea() < session()->num_dirs - 1) {
      session()->SetCurrentFileArea(session()->GetCurrentFileArea() + 1);
    }
  }
}

void ListUsersDL() {
  list_users(LIST_USERS_FILE_AREA);
}

void PrintDSZLog() {
  if (File::Exists(g_szDSZLogFileName)) {
    print_local_file(g_szDSZLogFileName);
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
  session()->tagging = 1;
  finddescription();
  session()->tagging = 0;
}

void TemporaryStuff() {
  temporary_stuff();
}

void JumpDirConf() {
  if (okconf(session()->user())) {
    jump_conf(CONF_DIRS);
  }
}

void ConfigFileList() {
  if (ok_listplus()) {
    config_file_list();
  }
}

void ListFiles() {
  session()->tagging = 1;
  listfiles();
  session()->tagging = 0;
}

void NewFileScan() {
  if (application()->HasConfigFlag(OP_FLAGS_SETLDATE)) {
    SetNewFileScanDate();
  }
  bool abort = false;
  g_num_listed = 0;
  session()->tagging = 1;
  session()->titled = 1;
  bout.nl();
  bout << "|#5Search all directories? ";
  if (yesno()) {
    nscanall();
  } else {
    bout.nl();
    nscandir(session()->GetCurrentFileArea(), &abort);
    if (g_num_listed) {
      endlist(2);
    } else {
      bout.nl();
      bout << "|#2No new files found.\r\n";
    }
  }
  session()->tagging = 0;
}

void RemoveFiles() {
  if (GuestCheck()) {
    removefile();
  }
}

void SearchAllFiles() {
  session()->tagging = 1;
  searchall();
  session()->tagging = 0;
}

void XferDefaults() {
  if (GuestCheck()) {
    xfer_defaults();
  }
}

void Upload() {
  play_sdf(UPLOAD_NOEXT, false);
  printfile(UPLOAD_NOEXT);
  if (session()->user()->IsRestrictionValidate() || session()->user()->IsRestrictionUpload() ||
      (syscfg.sysconfig & sysconfig_all_sysop)) {
    if (syscfg.newuploads < session()->num_dirs) {
      upload(static_cast<int>(syscfg.newuploads));
    } else {
      upload(0);
    }
  } else {
    upload(udir[session()->GetCurrentFileArea()].subnum);
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

void SetSubNumber(char *pszSubKeys) {
  for (int i = 0; (i < session()->num_subs) && (usub[i].subnum != -1); i++) {
    if (wwiv::strings::IsEquals(usub[i].keys, pszSubKeys)) {
      session()->SetCurrentMessageArea(i);
    }
  }
}

void SetDirNumber(char *pszDirectoryKeys) {
  for (int i = 0; i < session()->num_dirs; i++) {
    if (wwiv::strings::IsEquals(udir[i].keys, pszDirectoryKeys)) {
      session()->SetCurrentFileArea(i);
    }
  }
}
