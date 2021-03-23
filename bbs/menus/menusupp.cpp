/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include "bbs/menus/menusupp.h"

#include "bbs/attach.h"
#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/chains.h"
#include "bbs/chat.h"
#include "bbs/chnedit.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/defaults.h"
#include "bbs/diredit.h"
#include "bbs/dirlist.h"
#include "bbs/dropfile.h"
#include "bbs/execexternal.h"
#include "bbs/external_edit.h"
#include "bbs/finduser.h"
#include "bbs/gfileedit.h"
#include "bbs/gfiles.h"
#include "bbs/instmsg.h"
#include "bbs/listplus.h"
#include "bbs/misccmd.h"
#include "bbs/msgbase1.h"
#include "bbs/multinst.h"
#include "bbs/multmail.h"
#include "bbs/netsup.h"
#include "bbs/newuser.h"
#include "bbs/readmail.h"
#include "bbs/stuffin.h"
#include "bbs/subedit.h"
#include "bbs/sysopf.h"
#include "bbs/sysoplog.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "bbs/tag.h"
#include "bbs/utility.h"
#include "bbs/valscan.h"
#include "bbs/vote.h"
#include "bbs/voteedit.h"
#include "bbs/wqscn.h"
#include "bbs/xfer.h"
#include "bbs/xferovl.h"
#include "bbs/xferovl1.h"
#include "bbs/xfertmp.h"
#include "common/com.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/menus/menu_data_util.h"
#include "common/output.h"
#include "common/pause.h"
#include "common/quote.h"
#include "common/workspace.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "fmt/format.h"
#include "local_io/keycodes.h"
#include "local_io/wconstants.h"
#include "sdk/filenames.h"
#include "sdk/status.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/files/dirs.h"
#include "sdk/net/networks.h"

#include <memory>
#include <string>

using namespace wwiv::bbs;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::bbs::menus {

void UnQScan() {
  bout.nl();
  bout << "|#9Mark messages as unread on [C]urrent sub or [A]ll subs (A/C/Q)? ";
  switch (const char ch = onek("QAC\r"); ch) {
  case 'Q':
  case RETURN:
    break;
  case 'A': {
    for (int i = 0; i < a()->config()->max_subs(); i++) {
      a()->sess().qsc_p[i] = 0;
    }
    bout << "\r\nQ-Scan pointers reset.\r\n\n";
  }
  break;
  case 'C': {
    bout.nl();
    a()->sess().qsc_p[a()->current_user_sub().subnum] = 0;
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
  if (!okconf(a()->user())) {
    return;
  }
  const auto cn = a()->sess().current_user_sub_conf_num();
  const auto ncn = cn < size_int(a()->uconfsub) - 1 ? cn + 1 : 0;
  a()->sess().set_current_user_sub_conf_num(ncn);
  setuconf(ConferenceType::CONF_SUBS, ncn, -1);
}

void DownSubConf() {
  if (!okconf(a()->user())) {
    return;
  }
  const auto cn = a()->sess().current_user_sub_conf_num();
  const auto ncn = cn > 0 ? cn - 1 : size_int(a()->uconfsub) - 1;
  a()->sess().set_current_user_sub_conf_num(ncn);
  setuconf(ConferenceType::CONF_SUBS, a()->sess().current_user_sub_conf_num(), -1);
}

void DownSub() {
  const auto cn = a()->current_user_sub_num();
  const auto ncn = cn > 0 ? cn - 1 : size_int(a()->usub) - 1;
  a()->set_current_user_sub_num(ncn);
}

void UpSub() {
  const auto cn = a()->current_user_sub_num();
  const auto last = size_int(a()->usub) - 1;
  const auto ncn = cn < last ? cn + 1 : 0;
  a()->set_current_user_sub_num(ncn);
}

void ValidateUser() {
  bout.nl(2);
  bout << "|#9Enter user name or number:\r\n:";
  const auto user_name = bin.input_upper(30);
  if (const auto user_number = finduser1(user_name); user_number > 0) {
    sysoplog() << "@ Validated user #" << user_number;
    valuser(user_number);
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

void Defaults(bool& need_menu_reload) {
  if (GuestCheck()) {
    write_inst(INST_LOC_DEFAULTS, 0, INST_FLAGS_NONE);
    if (bout.printfile(DEFAULTS_NOEXT)) {
      bout.pausescr();
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
  bout.printfile(GFILES_NOEXT);
  gfiles();
}

void SystemInfo() {
  WWIVVersion();

  if (bout.printfile(LOGON_NOEXT)) {
    // Only display the pause if the file is not empty and contains information
    bout.pausescr();
  }

  if (bout.printfile(SYSTEM_NOEXT)) {
    bout.pausescr();
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
      a()->config()->newuser_config().use_address_city_state != newuser_item_type_t::unused) {
    bout << "|#2Number Name/Handle               Time  Date  City            ST Cty Modem    ##\r\n";
  } else {
    bout << "|#2Number Name/Handle               Language   Time  Date  Speed                ##\r\n";
  }
  const char filler_char = okansi() ? '\xCD' : '=';
  bout << "|#7" << std::string(79, filler_char) << wwiv::endl;
  bout.printfile(LASTON_TXT);
}

void ReadEMail() {
  readmail(false);
}

void NewMessageScan() {
  if (okconf(a()->user())) {
    bout.nl();
    bout << "|#5New message scan in all conferences? ";
    if (bin.noyes()) {
      NewMsgsAllConfs();
      return;
    }
  }
  write_inst(INST_LOC_SUBS, 65535, INST_FLAGS_NONE);
  bout.newline = false;
  nscan();
  bout.newline = true;
}

void GoodBye() {

  if (a()->batch().numbatchdl() != 0) {
    bout.nl();
    bout << "|#2Download files in your batch queue (|#1Y/n|#2)? ";
    if (bin.noyes()) {
      batchdl(1);
    }
  }
  auto filename = FilePath(a()->sess().dirs().current_menu_gfiles_directory(), LOGOFF_MAT);
  if (!File::Exists(filename)) {
    filename = FilePath(a()->config()->gfilesdir(), LOGOFF_MAT);
  }
  if (File::Exists(filename)) {
    int cycle = 0;
    do {
      bout.cls();
      bout.printfile(filename.string());
      switch (int ch = onek("QFTO", true); ch) {
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
        auto used_this_session =
            (std::chrono::system_clock::now() - a()->sess().system_logon_time());
        auto secs_used = std::chrono::duration_cast<std::chrono::seconds>(used_this_session);
        bout <<  "Time on   = " << ctim(static_cast<long>(secs_used.count())) << wwiv::endl;
        {
          TempDisablePause disable_pause(bout);
          bout.printfile(LOGOFF_NOEXT);
        }
        a()->user()->SetLastSubNum(a()->current_user_sub_num());
        a()->user()->SetLastDirNum(a()->current_user_dir_num());
        if (okconf(a()->user())) {
          a()->user()->SetLastSubConf(a()->sess().current_user_sub_conf_num());
          a()->user()->SetLastDirConf(a()->sess().current_user_dir_conf_num());
        }
        LogOffCmd();
        a()->Hangup();
        break;
      }
    } while (cycle == 0);
  } else {
    bout.nl(2);
    bout << "|#5Log Off? ";
    if (bin.yesno()) {
      write_inst(INST_LOC_LOGOFF, 0, INST_FLAGS_NONE);
      bout.cls();
      const auto used_this_session =
          (std::chrono::system_clock::now() - a()->sess().system_logon_time());
      const auto sec_used = static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(used_this_session).count());
      bout << "Time on   = " << ctim(sec_used) << wwiv::endl;
      {
        TempDisablePause disable_pause(bout);
        bout.printfile(LOGOFF_NOEXT);
      }
      a()->user()->SetLastSubNum(a()->current_user_sub_num());
      a()->user()->SetLastDirNum(a()->current_user_dir_num());
      if (okconf(a()->user())) {
        a()->user()->SetLastSubConf(a()->sess().current_user_sub_conf_num());
        a()->user()->SetLastDirConf(a()->sess().current_user_dir_conf_num());
      }
      LogOffCmd();
      a()->Hangup();
    }
  }
}

void WWIV_PostMessage() {
  a()->sess().clear_irt();
  clear_quotes(a()->sess());
  if (!a()->usub.empty()) {
    post(PostData());
  }
}

void ScanSub() {
  if (!a()->usub.empty()) {
    write_inst(INST_LOC_SUBS, a()->current_user_sub().subnum, INST_FLAGS_NONE);
    bool nextsub = false;
    qscan(a()->current_user_sub_num(), nextsub);
  }
}

void RemovePost() {
  if (!a()->usub.empty()) {
    write_inst(INST_LOC_SUBS, a()->current_user_sub().subnum, INST_FLAGS_NONE);
    remove_post();
  }
}

void TitleScan() {
  if (!a()->usub.empty()) {
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

void ToggleExpert(const std::string& data) {
  const common::menus::menu_data_and_options_t opts(data);
  a()->user()->toggle_flag(User::flag_expert);
  auto o = opts.opts("quiet");
  if (const auto quiet = !o.empty() && *std::begin(o) == "off"; !quiet) {
    bout << "|#3Expert mode is: " << (a()->user()->IsExpert() ? "On" : "Off") << wwiv::endl;
  }
}

void WWIVVersion() {
  bout.cls();
  bout << "|#9WWIV Bulletin Board System " << full_version() << wwiv::endl;
  bout << "|#9Copyright (C) 1998-2021, WWIV Software Services.\r\n";
  bout << "|#9All Rights Reserved.\r\n\r\n";
  bout << "|#9Licensed under the Apache License, Version 2.0." << wwiv::endl;
  bout << "|#9Please see |#1http://www.wwivbbs.org/ |#9for more information"
       << wwiv::endl << wwiv::endl;
  bout << "|#9Compile Time    : |#2" << wwiv_compile_datetime() << wwiv::endl;
  bout << "|#9SysOp Name      : |#2" << a()->config()->sysop_name() << wwiv::endl;
  bout << "|#9OS              : |#2" << os::os_version_string() << wwiv::endl;
  bout << "|#9Instance        : |#2" << a()->sess().instance_number() << wwiv::endl;

  if (!a()->nets().empty()) {
    const auto status = a()->status_manager()->get_status();
    a()->status_manager()->reload_status();
    bool first = true;
    bout << "|#9Network Version : |#2" << "net" << status->status_net_version() << wwiv::endl;
    bout << "|#9Networks        : ";
    for (const auto& n : a()->nets().networks()) {
      if (n.sysnum) {
        if (!first) {
          bout << "                : ";
        }
        first = false;
        bout << to_string(n) << wwiv::endl;
      }
    }
  }

  bout.nl(3);
  bout.pausescr();
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
  const bool bOldAvail = sysop2();
  ToggleScrollLockKey();
  const bool bNewAvail = sysop2();
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

void LoadTextFile() {
  bout.nl();
  bout << "|#9Enter Filename: ";
  const auto fileName = bin.input_path("", 50);
  if (!fileName.empty()) {
    bout.nl();
    bout << "|#5Allow editing? ";
    if (bin.yesno()) {
      bout.nl();
      LoadFileIntoWorkspace(a()->context(), fileName, false);
    } else {
      bout.nl();
      LoadFileIntoWorkspace(a()->context(), fileName, true);
    }
  }
}

void EditText() {
  write_inst(INST_LOC_TEDIT, 0, INST_FLAGS_NONE);
  bout.nl();
  bout << "|#7Enter Filespec: ";
  const auto fn = bin.input_path(50);
  if (!fn.empty()) {
    fsed_text_edit(fn, "", 500, MSGED_FLAG_NO_TAGLINE);
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

void ResetQscan() {
  bout << "|#5Reset all QScan/NScan pointers (For All Users)? ";
  if (bin.yesno()) {
    write_inst(INST_LOC_RESETQSCAN, 0, INST_FLAGS_NONE);
    for (int i = 0; i <= a()->users()->num_user_records(); i++) {
      read_qscn(i, a()->sess().qsc, true);
      memset(a()->sess().qsc_p, 0,
             a()->config()->qscn_len() - 4 * (1 + ((a()->config()->max_dirs() + 31) / 32) +
                                              ((a()->config()->max_subs() + 31) / 32)));
      write_qscn(i, a()->sess().qsc, true);
    }
    read_qscn(1, a()->sess().qsc, false);
    close_qscn();
  }
}

void MemoryStatus() {
  const auto status = a()->status_manager()->get_status();
  bout.nl();
  bout << "Qscanptr        : " << status->qscanptr() << wwiv::endl;
}

void InitVotes() {
  write_inst(INST_LOC_VOTE, 0, INST_FLAGS_NONE);
  sysoplog() << "@ Ran Ivotes";
  ivotes();
}

void ReadLog() {
  const auto sysop_log_file = sysoplog_filename(date());
  bout.print_local_file(sysop_log_file);
}

void ReadNetLog() {
  bout.print_local_file(NET_LOG);
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

void VotePrint() {
  write_inst(INST_LOC_VOTEPRINT, 0, INST_FLAGS_NONE);
  voteprint();
}

void YesterdaysLog() {
  const auto status = a()->status_manager()->get_status();
  bout.print_local_file(status->log_filename(1));
}

void ZLog() {
  zlog();
}

void ViewNetDataLog() {
  while (!a()->sess().hangup()) {
    bout.nl();
    bout << "|#9Which NETDAT log (0-2,Q)? ";
    const auto netdat_num= onek("Q012");
    if (netdat_num == 'Q') {
      return;
    }
    bout.print_local_file(fmt::format("netdat{}.log", netdat_num));
  }
}

void UploadPost() {
  upload_post();
}

void WhoIsOnline() {
  multi_instance();
  bout.nl();
  bout.pausescr();
}

void NewMsgsAllConfs() {
  bool ac = false;

  write_inst(INST_LOC_SUBS, a()->current_user_sub().subnum, INST_FLAGS_NONE);
  bout.newline = false;
  if (ok_multiple_conf(a()->user(), a()->uconfsub)) {
    ac = true;
    tmp_disable_conf(true);
  }
  nscan();
  bout.newline = true;
  if (ac == true) {
    tmp_disable_conf(false);
  }
}

void MultiEmail() {
  slash_e();
}

void NewMsgScanFromHere() {
  bout.newline = false;
  nscan(a()->current_user_sub_num());
  bout.newline = true;
}

void ValidateScan() {
  bout.newline = false;
  valscan();
  bout.newline = true;
}

void ChatRoom() {
  write_inst(INST_LOC_CHATROOM, 0, INST_FLAGS_NONE);
  chat_room();
}

void ClearQScan() {
  bout.nl();
  bout << "|#5Mark messages as read on [C]urrent sub or [A]ll subs (A/C/Q)? ";
  switch (const auto ch = onek("QAC\r"); ch) {
  case 'Q':
  case RETURN:
    break;
  case 'A': {
    const auto status = a()->status_manager()->get_status();
    for (int i = 0; i < a()->config()->max_subs(); i++) {
      a()->sess().qsc_p[i] = status->qscanptr() - 1L;
    }
    bout.nl();
    bout << "Q-Scan pointers cleared.\r\n";
  }
  break;
  case 'C':
    const auto status = a()->status_manager()->get_status();
    bout.nl();
    a()->sess().qsc_p[a()->current_user_sub().subnum] = status->qscanptr() - 1L;
    bout << "Messages on " << a()->subs().sub(a()->current_user_sub().subnum).name
         << " marked as read.\r\n";
    break;
  }
}

void FastGoodBye() {
  if (a()->batch().numbatchdl() != 0) {
    bout.nl();
    bout << "|#2Download files in your batch queue (|#1Y/n|#2)? ";
    if (bin.noyes()) {
      batchdl(1);
    }
  }
  a()->user()->SetLastSubNum(a()->current_user_sub_num());
  a()->user()->SetLastDirNum(a()->current_user_dir_num());
  if (okconf(a()->user())) {
    a()->user()->SetLastSubConf(a()->sess().current_user_sub_conf_num());
    a()->user()->SetLastDirConf(a()->sess().current_user_dir_conf_num());
  }
  LogOffCmd();
  a()->Hangup();
}

void NewFilesAllConfs() {
  bout.nl();
  int ac = 0;
  if (ok_multiple_conf(a()->user(), a()->uconfsub)) {
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
  if (bin.yesno()) {
    read_idz_all();
  } else {
    read_idz(true, a()->current_user_dir_num());
  }
}

void RemoveNotThere() {
  removenotthere();
}

void UploadAllDirs() {
  bout.nl(2);
  auto ok = true;
  for (auto dn = 0;
       dn < size_int(a()->udir) && a()->udir[dn].subnum >= 0 && ok && !a()->sess().hangup(); dn++) {
    bout << "|#9Now uploading files for: |#2" << a()->dirs()[a()->udir[dn].subnum].name
         << wwiv::endl;
    ok = uploadall(dn);
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
  bool bSortAll = bin.yesno();
  bout.nl();
  bout << "|#5Sort by date? ";

  int nType = 0;
  if (bin.yesno()) {
    nType = 2;
  }

  TempDisablePause disable_pause(bout);
  if (bSortAll) {
    sort_all(nType);
  } else {
    sortdir(a()->current_user_dir().subnum, nType);
  }
}

void ReverseSort() {
  bout.nl();
  bout << "|#5Sort all dirs? ";
  bool bSortAll = bin.yesno();
  bout.nl();
  TempDisablePause disable_pause(bout);
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
  bout.nl();
  bout << "|#21|#9) PCB, RBBS   - <filename> <size> <date> <description>\r\n";
  bout << "|#22|#9) QBBS format - <filename> <description>\r\n";
  bout.nl();
  bout << "|#Select Format (1,2,Q) : ";
  char ch = onek("Q12");
  bout.nl();
  if (ch != 'Q') {
    bout << "|#9Enter Filename (wildcards allowed).\r\n|#7: ";
    bout.mpl(77);
    const auto filespec = bin.input_text(80);
    const auto type = (ch == '1') ? 2 : 0;
    upload_files(filespec, a()->current_user_dir_num(), type);
  }
}

void UpDirConf() {
  if (!okconf(a()->user())) {
    return;
  }
  const auto cn = a()->sess().current_user_dir_conf_num();
  const auto last = size_int(a()->uconfdir) - 1;
  a()->sess().set_current_user_dir_conf_num( (cn < last) ? cn + 1 : 0);
  setuconf(ConferenceType::CONF_DIRS, a()->sess().current_user_dir_conf_num(), -1);
}

void UpDir() {
  const auto cd = a()->current_user_dir_num();
  const auto last = size_int(a()->udir) - 1;
  if (cd < last) {
    a()->set_current_user_dir_num(cd + 1);
  } else {
    a()->set_current_user_dir_num(0);
  }
}

void DownDirConf() {
  if (!okconf(a()->user())) {
    return;
  }
  const auto cn = a()->sess().current_user_dir_conf_num();
  const auto ncn = cn > 0 ? cn - 1 : size_int(a()->uconfdir) - 1;
  a()->sess().set_current_user_dir_conf_num(ncn);
  setuconf(ConferenceType::CONF_DIRS, ncn, -1);
}

void DownDir() {
  const auto cd = a()->current_user_dir_num();
  const auto last = size_int(a()->udir) - 1;
  if (cd > 0) {
    a()->set_current_user_dir_num(cd - 1);
  } else {
    a()->set_current_user_dir_num(last);
  }
}

void ListUsersDL() {
  list_users(LIST_USERS_FILE_AREA);
}

void PrintDSZLog() {
  if (File::Exists(a()->dsz_logfile_name_)) {
    bout.print_local_file(a()->dsz_logfile_name_);
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
  bout.printfile(DOWNLOAD_NOEXT);
  download();
}

void FindDescription() {
  finddescription();
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
  SetNewFileScanDate();
  bool abort = false;
  bool need_title = true;
  bout.nl();
  bout << "|#5Search all directories? ";
  if (bin.yesno()) {
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
  bout.printfile(UPLOAD_NOEXT);
  if (a()->user()->restrict_validate() || a()->user()->restrict_upload() ||
      (a()->config()->sysconfig_flags() & sysconfig_all_sysop)) {
    if (a()->config()->new_uploads_dir() < a()->dirs().size()) {
      upload(static_cast<int>(a()->config()->new_uploads_dir()));
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
  bout.printfile(ZUPLOAD_NOEXT);
  bout.nl(2);
  bout << "Sending file to sysop :-\r\n\n";
  upload(0);
}

void GuestApply() {
  if (a()->user()->guest_user()) {
    newuser();
  } else {
    bout << "You already have an account on here!\r\n\r\n";
  }
}

void AttachFile() {
  attach_file(0);
}

bool GuestCheck() {
  if (a()->user()->guest_user()) {
    bout << "|#6This command is only for registered users.\r\n";
    return false;
  }
  return true;
}

void SetSubNumber(const MenuContext& context) {
  for (auto i = 0; i < size_int(a()->usub); i++) {
    if (a()->usub[i].keys == context.data) {
      a()->set_current_user_sub_num(i);
    }
  }
}

void SetDirNumber(const MenuContext& context) {
  for (auto i = 0; i < ssize(a()->udir); i++) {
    if (a()->udir[i].keys == context.data) {
      a()->set_current_user_dir_num(i);
    }
  }
}

void LogOffCmd() {
  if (a()->logoff_cmd.empty()) {
    return;
  }

  bout.nl();
  const auto cmd = stuff_in(a()->logoff_cmd, create_chain_file(), "", "", "", "");
  ExecuteExternalProgram(cmd, a()->spawn_option(SPAWNOPT_LOGOFF));
  bout.nl(2);
}


}
