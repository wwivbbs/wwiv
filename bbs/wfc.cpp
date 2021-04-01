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
#include "bbs/wfc.h"

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsutl.h"
#include "bbs/chnedit.h"
#include "bbs/confutil.h"
#include "bbs/connect1.h"
#include "bbs/diredit.h"
#include "bbs/exec.h"
#include "bbs/external_edit.h"
#include "bbs/gfileedit.h"
#include "bbs/instmsg.h"
#include "bbs/multinst.h"
#include "bbs/netsup.h"
#include "bbs/readmail.h"
#include "bbs/ssh.h"
#include "bbs/subedit.h"
#include "bbs/sysopf.h"
#include "bbs/sysoplog.h"
#include "bbs/voteedit.h"
#include "bbs/wqscn.h"
#include "common/com.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/output.h"
#include "common/workspace.h"
#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/version.h"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"
#include "local_io/wconstants.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/net/networks.h"

#include <chrono>
#include <memory>
#include <string>

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::strings;

// Local Functions
static std::unique_ptr<char[]> screen_buffer;
static int inst_num;
static constexpr int sysop_usernum = 1;

void wfc_cls(Application* a) {
  bout.ResetColors();
  a->Cls();
  bout.localIO()->SetCursor(LocalIO::cursorNormal);
  // Every time we clear the WFC, reset the lines listed.
  bout.clear_lines_listed();
}

namespace wwiv::bbs {

static void wfc_update() {
  // Every time we update the WFC, reset the lines listed.
  bout.clear_lines_listed();

  bout.localIO()->PutsXYA(57, 18, 15, fmt::format("{:<3}", inst_num));
  if (const auto ir = a()->instances().at(inst_num); ir.online()) {
    const auto unn = a()->names()->UserName(ir.user_number());
    bout.localIO()->PutsXYA(42, 19, 14, fmt::format("{:<25}", unn));
  } else {
    bout.localIO()->PutsXYA(42, 19, 14, fmt::format("{:<25}", "Nobody"));
  }

  auto activity_string = make_inst_str(inst_num, INST_FORMAT_WFC);
  bout.localIO()->PutsXYA(42, 20, 14, fmt::format("{:<25}", activity_string));
  if (num_instances() > 1) {
    do {
      ++inst_num;
      if (inst_num > num_instances()) {
        inst_num = 1;
      }
    } while (inst_num == a()->sess().instance_number());
  }
}

void WFC::Clear() {
  wfc_cls(a_);
  status_ = 0;
}

void WFC::DrawScreen() {
  static steady_clock::time_point wfc_time;
  static steady_clock::time_point poll_time;

  auto nNumNewMessages = check_new_mail(sysop_usernum);
  auto status = a()->status_manager()->get_status();
  if (status_ == 0) {
    bout.localIO()->SetCursor(LocalIO::cursorNone);
    a()->Cls();
    if (!screen_buffer) {
      screen_buffer = std::make_unique<char[]>(80 * 25 * sizeof(uint16_t));
      File wfcFile(FilePath(a()->config()->datadir(), WFC_DAT));
      if (!wfcFile.Open(File::modeBinary | File::modeReadOnly)) {
        Clear();
        LOG(FATAL) << wfcFile << " NOT FOUND.";
      }
      wfcFile.Read(screen_buffer.get(), 80 * 25 * sizeof(uint16_t));
    }
    bout.localIO()->WriteScreenBuffer(screen_buffer.get());
    const auto title = fmt::format("Activity and Statistics of {} Node {}",
                                   a()->config()->system_name(), a()->sess().instance_number());
    bout.localIO()->PutsXYA(1 + (76 - wwiv::strings::ssize(title)) / 2, 4, 15, title);
    bout.localIO()->PutsXYA(8, 1, 14, fulldate());
    bout.localIO()->PutsXYA(40, 1, 3, StrCat("OS: ", wwiv::os::os_version_string()));
    bout.localIO()->PutsXYA(21, 6, 14, std::to_string(status->calls_today()));
    auto feedback_waiting = 0;
    if (const auto sysop = a()->users()->readuser(sysop_usernum)) {
      feedback_waiting = sysop->email_waiting();
    }
    bout.localIO()->PutsXYA(21, 7, 14, std::to_string(feedback_waiting));
    if (nNumNewMessages) {
      bout.localIO()->PutsXYA(29, 7, 3, "New:");
      bout.localIO()->PutsXYA(34, 7, 12, std::to_string(nNumNewMessages));
    }
    bout.localIO()->PutsXYA(21, 8, 14, std::to_string(status->uploads_today()));
    bout.localIO()->PutsXYA(21, 9, 14, std::to_string(status->msgs_today()));
    bout.localIO()->PutsXYA(21, 10, 14, std::to_string(status->localposts()));
    bout.localIO()->PutsXYA(21, 11, 14, std::to_string(status->email_today()));
    bout.localIO()->PutsXYA(21, 12, 14, std::to_string(status->feedback_today()));
    const auto percent = static_cast<double>(status->active_today_minutes()) / 1440.0;
    bout.localIO()->PutsXYA(
        21, 13, 14,
        fmt::format("{} Mins ({:.2}%)", status->active_today_minutes(), 100.0 * percent));
    bout.localIO()->PutsXYA(58, 6, 14, full_version());

    bout.localIO()->PutsXYA(58, 7, 14, std::to_string(status->status_net_version()));
    bout.localIO()->PutsXYA(58, 8, 14, std::to_string(status->num_users()));
    bout.localIO()->PutsXYA(58, 9, 14, std::to_string(status->caller_num()));
    const auto percent_active =
        status->days_active() == 0
            ? "N/A"
            : fmt::sprintf("%.2f", static_cast<float>(status->caller_num()) /
                                       static_cast<float>(status->days_active()));
    bout.localIO()->PutsXYA(58, 10, 14, percent_active);
    bout.localIO()->PutsXYA(58, 11, 14, sysop2() ? "Available    " : "Not Available");

    auto ir = a()->instances().at(a()->sess().instance_number());
    if (ir.user_number() < a()->config()->max_users() && ir.user_number() > 0) {
      const auto unn = a()->names()->UserName(ir.user_number());
      bout.localIO()->PutsXYA(33, 16, 14, fmt::format("{:<20}", unn));
    } else {
      bout.localIO()->PutsXYA(33, 16, 14, fmt::format("{:<20}", "Nobody"));
    }

    status_ = 1;
    wfc_update();
    poll_time = wfc_time = steady_clock::now();
  } else {
    auto screen_saver_time = seconds(a()->screen_saver_time);
    if (a()->screen_saver_time == 0 || steady_clock::now() - wfc_time < screen_saver_time) {
      auto t = times();
      bout.localIO()->PutsXYA(28, 1, 14, t);
      bout.localIO()->PutsXYA(58, 11, 14, sysop2() ? "Available    " : "Not Available");
      if (steady_clock::now() - poll_time > seconds(10)) {
        wfc_update();
        poll_time = steady_clock::now();
      }
    } else {
      if ((steady_clock::now() - poll_time > seconds(10)) || status_ == 1) {
        status_ = 2;
        a_->Cls();
        bout.localIO()->PutsXYA(random_number(38), random_number(24), random_number(14) + 1,
                                "WWIV Screen Saver - Press Any Key For WWIV");
        wfc_time = steady_clock::now() - seconds(a()->screen_saver_time) - seconds(1);
        poll_time = steady_clock::now();
      }
    }
  }
}

WFC::WFC(Application* a) : a_(a) {
  bout.localIO()->SetCursor(LocalIO::cursorNormal);
  status_ = 0;
  inst_num = 1;
  Clear();

  bout.remoteIO()->remote_info().clear();
  a_->frequent_init();
  a_->sess().user_num(0);
  // Since hang_it_up sets hangup_ = true, let's ensure we're always
  // not in this state when we enter the WFC.
  a_->sess().hangup(false);
  a_->set_at_wfc(false);
  write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
  // We'll read the sysop record for defaults, but let's set
  // usernum to 0 here since we don't want to botch up the
  // sysop's record if things go wrong.
  // TODO(rushfan): Let's make record 0 the logon defaults
  // and stop using the sysop record.
  a_->ReadCurrentUser(1);
  read_qscn(1, a_->sess().qsc, false);
  // N.B. This used to be 1.
  a_->sess().user_num(0);

  a_->reset_effective_sl();
  if (a_->user()->deleted()) {
    a_->user()->screen_width(80);
    a_->user()->screen_lines(25);
  }
  a_->sess().num_screen_lines(bout.localIO()->GetDefaultScreenBottom() + 1);
}

WFC::~WFC() {
  a_->set_at_wfc(false);
  
};

std::tuple<wfc_events_t, int> WFC::doWFCEvents() {
  auto* io = bout.localIO();

  auto last_date_status = a()->status_manager()->get_status();
  for (;;) {
    sleep_for(milliseconds(100));
    yield();

    write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
    a_->set_net_num(0);
    a_->set_at_wfc(true);

    // If the date has changed since we last checked, then then run the beginday event.
    if (date() != last_date_status->last_date()) {
      if (a_->GetBeginDayNodeNumber() == 0 ||
          a_->sess().instance_number() == a_->GetBeginDayNodeNumber()) {
        beginday(true);
        Clear();
        // Update the status for the last date now that beginday has run.
        last_date_status = a()->status_manager()->get_status();
        continue;
      }
    }

    a_->SetCurrentSpeed("KB");
    // try to check for packets to send every minute.
    DrawScreen();
    bin.okskey(false);
    if (!io->KeyPressed()) {
      // extra sleep.
      sleep_for(milliseconds(100));
      yield();
      continue;
    }
    a_->set_at_wfc(false);
    a_->ReadCurrentUser(sysop_usernum);
    read_qscn(1, a()->sess().qsc, false);
    a_->set_at_wfc(true);
    auto ch = to_upper_case<char>(io->GetChar());
    if (ch == 0) {
      ch = io->GetChar();
      a_->handle_sysop_key(ch);
      continue;
    }

    a_->set_at_wfc(true);
    bin.okskey(true);
    bin.resetnsp();
    io->SetCursor(LocalIO::cursorNormal);
    switch (ch) {
      // Local Logon
    case SPACE: {
      auto [ll, unx] = LocalLogon();
      if (ll == local_logon_t::fast) {
        return std::make_tuple(wfc_events_t::login_fast, unx);
      }
      if (ll == local_logon_t::prompt) {
        return std::make_tuple(wfc_events_t::login, -1);
      }
      break;
    }
    // Show WFC Menu
    case '?': {
      std::string helpFileName = SWFC_NOEXT;
      char chHelp;
      do {
        io->Cls();
        bout.nl();
        bout.print_help_file(helpFileName);
        chHelp = bin.getkey();
        helpFileName = (helpFileName == SWFC_NOEXT) ? SONLINE_NOEXT : SWFC_NOEXT;
      } while (chHelp != SPACE && chHelp != ESC);
    } break;
    // Force Network Callout
    case '/':
      if (a_->current_net().sysnum) {
        force_callout();
      }
      break;
    case ',':
      // Print NetLogs
      if (a_->current_net().sysnum > 0 || !a_->nets().empty()) {
        io->GotoXY(2, 23);
        bout << "|#7(|#2Q|#7=|#2Quit|#7) Display Which NETDAT Log File (|#10|#7-|#12|#7): ";
        const auto netdat_num = onek("Q012");
        switch (netdat_num) {
        case '0':
        case '1':
        case '2': {
          bout.print_local_file(fmt::format("netdat{}.log", netdat_num));
        } break;
        default:;
        }
      }
      break;
      // Net List
    case '`':
      if (a_->current_net().sysnum) {
        query_print_net_listing(true);
      }
      break;
      // [ESC] Quit the BBS
    case ESC:
      io->GotoXY(2, 23);
      bout << "|#7Exit the BBS? ";
      if (bin.yesno()) {
        return std::make_tuple(wfc_events_t::exit, -1);
      }
      io->Cls();
      break;
      // BoardEdit
    case 'B':
      write_inst(INST_LOC_BOARDEDIT, 0, INST_FLAGS_NONE);
      boardedit();
      cleanup_net();
      break;
      // ChainEdit
    case 'C':
      write_inst(INST_LOC_CHAINEDIT, 0, INST_FLAGS_NONE);
      chainedit();
      break;
      // DirEdit
    case 'D':
      write_inst(INST_LOC_DIREDIT, 0, INST_FLAGS_NONE);
      dlboardedit();
      break;
      // Send Email
    case 'E':
      Clear();
      a_->sess().user_num(1);
      bout.bputs("|#1Send Email:");
      send_email();
      a_->WriteCurrentUser(sysop_usernum);
      cleanup_net();
      break;
    // GfileEdit
    case 'G':
      write_inst(INST_LOC_GFILEEDIT, 0, INST_FLAGS_NONE);
      gfileedit();
      break;
    // ConfEdit
    case 'J':
      Clear();
      edit_confs();
      break;
      // SendMailFile
    case 'K': {
      Clear();
      a_->sess().user_num(1);
      bout << "|#1Send any Text File in Email:\r\n\n|#2Filename: ";
      auto buffer = bin.input_path(50);
      LoadFileIntoWorkspace(a()->context(), buffer, false);
      send_email();
      a_->WriteCurrentUser(sysop_usernum);
      cleanup_net();
    } break;
    // Print Log Daily logs
    case 'L': {
      Clear();
      const auto status = a()->status_manager()->get_status();
      bout.print_local_file(status->log_filename(0));
    } break;
    // Read User Mail
    case 'M': {
      Clear();
      a_->sess().user_num(sysop_usernum);
      readmail(false);
      a_->WriteCurrentUser(sysop_usernum);
      cleanup_net();
    } break;
    // Print Net Log
    case 'N': {
      Clear();
      bout.print_local_file(NET_LOG);
    } break;
    // EditTextFile
    case 'O': {
      Clear();
      write_inst(INST_LOC_TEDIT, 0, INST_FLAGS_NONE);
      bout << "\r\n|#1Edit any Text File: \r\n\n|#2Filename: ";
      const auto current_dir_slash = File::EnsureTrailingSlash(File::current_directory());
      auto net_filename = bin.input_path(current_dir_slash, 50);
      if (!net_filename.empty()) {
        fsed_text_edit(net_filename, "", 500, MSGED_FLAG_NO_TAGLINE);
      }
    } break;
    // Print Network Pending list
    case 'P': {
      Clear();
      print_pending_list();
    } break;
    // Quit BBS
    case 'Q':
      io->GotoXY(2, 23);
      return std::make_tuple(wfc_events_t::exit, -1);
      // Read All Mail
    case 'R':
      Clear();
      write_inst(INST_LOC_MAILR, 0, INST_FLAGS_NONE);
      mailr();
      break;
      // Print Current Status
    case 'S':
      prstatus();
      bin.getkey();
      break;
    case 'T':
      if (a()->terminal_command.empty()) {
        bout << "Terminal Command not specified. " << wwiv::endl
             << " Please set TERMINAL_CMD in wwiv.ini" << wwiv::endl;
        bin.getkey();
        break;
      }
      exec_cmdline(a()->terminal_command, INST_FLAGS_NONE);
      break;
    case 'U': {
      // User edit
      auto exe = FilePath(a()->bindir(), "wwivconfig");
#ifdef _WIN32
      // CreateProcess is failing on Windows with the .exe extension, and since
      // we don't use MakeAbsCmd on this, it's without .exe.
      exe.replace_extension(".exe");
#endif // _WIN32
      const auto cmd = StrCat(exe.string(), " --user_editor");
      exec_cmdline(cmd, INST_FLAGS_NONE);
    } break;
    case 'V': {
      // InitVotes
      Clear();
      write_inst(INST_LOC_VOTEEDIT, 0, INST_FLAGS_NONE);
      ivotes();
    } break;
    // Edit Gfile
    case 'W': {
      Clear();
      write_inst(INST_LOC_TEDIT, 0, INST_FLAGS_NONE);
      bout << "|#1Edit " << a()->config()->gfilesdir() << "<filename>: \r\n";
      text_edit();
    } break;
    // Print Yesterday's Log
    case 'Y': {
      Clear();
      const auto status = a()->status_manager()->get_status();
      bout.print_local_file(status->log_filename(1));
    } break;
    // Print Activity (Z) Log
    case 'Z': {
      zlog();
      bout.nl();
      bin.getkey();
    } break;
    }
    Clear();
    a_->frequent_init();
    a_->ReadCurrentUser(sysop_usernum);
    read_qscn(1, a()->sess().qsc, false);
    a_->reset_effective_sl();
    a_->sess().user_num(sysop_usernum);

    catsl();
    write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
  }
}

std::tuple<local_logon_t, int> WFC::LocalLogon() {
  bout.localIO()->GotoXY(2, 23);
  bout << "|#9Log on to the BBS?";
  auto d = steady_clock::now();
  // TODO(rushfan): use wwiv::os::wait_for
  while (!bout.localIO()->KeyPressed() && (steady_clock::now() - d < minutes(1))) {
    sleep_for(10ms);
  }

  if (!bout.localIO()->KeyPressed()) {
    a_->Cls();
    return std::make_tuple(local_logon_t::exit, -1);
  }

  auto unx = -1;
  const auto ch = to_upper_case<char>(bout.localIO()->GetChar());
  switch (ch) {
  case 'Y': {
    bout.localIO()->Puts(YesNoString(true));
    bout << wwiv::endl;
    return std::make_tuple(local_logon_t::prompt, -1);
  }
  case 0:
  case 224: {
    // The ch == 224 is a Win32'ism
    bout.localIO()->GetChar();
    return std::make_tuple(local_logon_t::exit, -1);
  }
  case 'F': // 'F' for Fast
  case '1':
    unx = 1;
    break;
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    unx = ch - '0';
    break;
  default:
    return std::make_tuple(local_logon_t::exit, -1);
  }

  // We have a user number to try in unx to use for a
  // fast login.

  if (unx > a_->status_manager()->user_count()) {
    return std::make_tuple(local_logon_t::exit, -1);
  }

  if (const auto tu = a_->users()->readuser(unx); tu->sl() != 255 || tu->deleted()) {
    return std::make_tuple(local_logon_t::exit, -1);
  }

  a_->sess().user_num(unx);
  const auto saved_at_wfc = a_->at_wfc();
  a_->set_at_wfc(false);
  a_->ReadCurrentUser();
  read_qscn(a_->sess().user_num(), a()->sess().qsc, false);
  a_->set_at_wfc(saved_at_wfc);
  bout.bputch(ch);
  bout.localIO()->Puts("\r\n\r\n\r\n\r\n\r\n\r\n");
  a_->reset_effective_sl();
  changedsl();
  return std::make_tuple(local_logon_t::fast, unx);
}

} // namespace wwiv::bbs
