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
#include "bbs/wfc.h"

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsutl.h"
#include "bbs/chnedit.h"
#include "bbs/com.h"
#include "bbs/confutil.h"
#include "bbs/connect1.h"
#include "bbs/datetime.h"
#include "bbs/diredit.h"
#include "bbs/exec.h"
#include "bbs/external_edit.h"
#include "bbs/gfileedit.h"
#include "bbs/inetmsg.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/multinst.h"
#include "bbs/netsup.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/readmail.h"
#include "bbs/ssh.h"
#include "bbs/subedit.h"
#include "bbs/sysopf.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/voteedit.h"
#include "bbs/workspace.h"
#include "bbs/wqscn.h"
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
#include <cctype>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::os::random_number;
using namespace std::chrono;
using namespace std::chrono_literals;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

// Local Functions
static std::unique_ptr<char[]> screen_buffer;
static int inst_num;
static constexpr int sysop_usernum = 1;

void wfc_cls(Application* a) {
  if (a->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    bout.ResetColors();
    a->Cls();
    a->localIO()->SetCursor(LocalIO::cursorNormal);
  }
  // Every time we clear the WFC, reset the lines listed.
  bout.clear_lines_listed();
}

namespace wwiv::bbs {

static void wfc_update() {
  // Every time we update the WFC, reset the lines listed.
  bout.clear_lines_listed();

  if (!a()->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    return;
  }

  instancerec ir = {};
  User u = {};

  get_inst_info(inst_num, &ir);
  a()->users()->readuser_nocache(&u, ir.user);
  a()->localIO()->PutsXYA(57, 18, 15, fmt::format("{:<3}", inst_num));
  if (ir.flags & INST_FLAGS_ONLINE) {
    const auto unn = a()->names()->UserName(ir.user);
    a()->localIO()->PutsXYA(42, 19, 14, fmt::format("{:<25}", unn));
  } else {
    a()->localIO()->PutsXYA(42, 19, 14, fmt::format("{:<25}", "Nobody"));
  }

  auto activity_string = make_inst_str(inst_num, INST_FORMAT_WFC);
  a()->localIO()->PutsXYA(42, 20, 14, fmt::format("{:<25}", activity_string));
  if (num_instances() > 1) {
    do {
      ++inst_num;
      if (inst_num > num_instances()) {
        inst_num = 1;
      }
    } while (inst_num == a()->instance_number());
  }
}

void WFC::Clear() {
  wfc_cls(a_);
  status_ = 0;
}

void WFC::DrawScreen() {
  instancerec ir{};
  User u;
  static steady_clock::time_point wfc_time;
  static steady_clock::time_point poll_time;

  if (!a()->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    return;
  }

  auto nNumNewMessages = check_new_mail(sysop_usernum);
  auto status = a()->status_manager()->GetStatus();
  if (status_ == 0) {
    a()->localIO()->SetCursor(LocalIO::cursorNone);
    a()->Cls();
    if (!screen_buffer) {
      screen_buffer = std::make_unique<char[]>(80 * 25 * sizeof(uint16_t));
      File wfcFile(PathFilePath(a()->config()->datadir(), WFC_DAT));
      if (!wfcFile.Open(File::modeBinary | File::modeReadOnly)) {
        Clear();
        LOG(FATAL) << wfcFile << " NOT FOUND.";
        a()->AbortBBS();
      }
      wfcFile.Read(screen_buffer.get(), 80 * 25 * sizeof(uint16_t));
    }
    a()->localIO()->WriteScreenBuffer(screen_buffer.get());
    const auto title = fmt::format("Activity and Statistics of {} Node {}",
                                   a()->config()->system_name(), a()->instance_number());
    a()->localIO()->PutsXYA(1 + (76 - wwiv::strings::ssize(title)) / 2, 4, 15, title);
    a()->localIO()->PutsXYA(8, 1, 14, fulldate());
    a()->localIO()->PutsXYA(40, 1, 3, StrCat("OS: ", wwiv::os::os_version_string()));
    a()->localIO()->PutsXYA(21, 6, 14, std::to_string(status->GetNumCallsToday()));
    User sysop{};
    auto feedback_waiting = 0;
    if (a()->users()->readuser_nocache(&sysop, sysop_usernum)) {
      feedback_waiting = sysop.GetNumMailWaiting();
    }
    a()->localIO()->PutsXYA(21, 7, 14, std::to_string(feedback_waiting));
    if (nNumNewMessages) {
      a()->localIO()->PutsXYA(29, 7, 3, "New:");
      a()->localIO()->PutsXYA(34, 7, 12, std::to_string(nNumNewMessages));
    }
    a()->localIO()->PutsXYA(21, 8, 14, std::to_string(status->GetNumUploadsToday()));
    a()->localIO()->PutsXYA(21, 9, 14, std::to_string(status->GetNumMessagesPostedToday()));
    a()->localIO()->PutsXYA(21, 10, 14, std::to_string(status->GetNumLocalPosts()));
    a()->localIO()->PutsXYA(21, 11, 14, std::to_string(status->GetNumEmailSentToday()));
    a()->localIO()->PutsXYA(21, 12, 14, std::to_string(status->GetNumFeedbackSentToday()));
    a()->localIO()->PutsXYA(
        21, 13, 14,
        fmt::sprintf("%d Mins (%.1f%%)", status->GetMinutesActiveToday(),
                     100.0 * static_cast<float>(status->GetMinutesActiveToday()) / 1440.0));
    a()->localIO()->PutsXYA(58, 6, 14, StrCat(wwiv_version, beta_version));

    a()->localIO()->PutsXYA(58, 7, 14, std::to_string(status->GetNetworkVersion()));
    a()->localIO()->PutsXYA(58, 8, 14, std::to_string(status->GetNumUsers()));
    a()->localIO()->PutsXYA(58, 9, 14, std::to_string(status->GetCallerNumber()));
    if (status->GetDays()) {
      a()->localIO()->PutsXYA(58, 10, 14,
                              fmt::sprintf("%.2f", static_cast<float>(status->GetCallerNumber()) /
                                                       static_cast<float>(status->GetDays())));
    } else {
      a()->localIO()->PutsXYA(58, 10, 14, "N/A");
    }
    a()->localIO()->PutsXYA(58, 11, 14, sysop2() ? "Available    " : "Not Available");

    get_inst_info(a()->instance_number(), &ir);
    if (ir.user < a()->config()->max_users() && ir.user > 0) {
      const auto unn = a()->names()->UserName(ir.user);
      a()->localIO()->PutsXYA(33, 16, 14, fmt::format("{:<20}", unn));
    } else {
      a()->localIO()->PutsXYA(33, 16, 14, fmt::format("{:<20}", "Nobody"));
    }

    status_ = 1;
    wfc_update();
    poll_time = wfc_time = steady_clock::now();
  } else {
    auto screen_saver_time = seconds(a()->screen_saver_time);
    if (a()->screen_saver_time == 0 || steady_clock::now() - wfc_time < screen_saver_time) {
      auto t = times();
      a()->localIO()->PutsXYA(28, 1, 14, t);
      a()->localIO()->PutsXYA(58, 11, 14, sysop2() ? "Available    " : "Not Available");
      if (steady_clock::now() - poll_time > seconds(10)) {
        wfc_update();
        poll_time = steady_clock::now();
      }
    } else {
      if ((steady_clock::now() - poll_time > seconds(10)) || status_ == 1) {
        status_ = 2;
        a_->Cls();
        a()->localIO()->PutsXYA(random_number(38), random_number(24), random_number(14) + 1,
                                "WWIV Screen Saver - Press Any Key For WWIV");
        wfc_time = steady_clock::now() - seconds(a()->screen_saver_time) - seconds(1);
        poll_time = steady_clock::now();
      }
    }
  }
}

WFC::WFC(Application* a) : a_(a) {
  a_->localIO()->SetCursor(LocalIO::cursorNormal);
  if (a_->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    status_ = 0;
    inst_num = 1;
  }
  Clear();
}

WFC::~WFC() = default;

int WFC::doWFCEvents() {
  unsigned char ch = 0;
  int lokb = 0;
  LocalIO* io = a_->localIO();

  const auto last_date_status = a()->status_manager()->GetStatus();
  do {
    write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
    a_->set_net_num(0);
    bool any = false;
    a_->set_at_wfc(true);

    // If the date has changed since we last checked, then then run the beginday event.
    if (date() != last_date_status->GetLastDate()) {
      if (a_->GetBeginDayNodeNumber() == 0 ||
          a_->instance_number() == a_->GetBeginDayNodeNumber()) {
        beginday(true);
        Clear();
      }
    }

    lokb = 0;
    a_->SetCurrentSpeed("KB");
    auto current_time = steady_clock::now();
    const auto node_supports_callout = a_->HasConfigFlag(OP_FLAGS_NET_CALLOUT);
    // try to check for packets to send every minute.
    auto diff_time = current_time - last_network_attempt();
    auto time_to_call = diff_time > minutes(1); // was 1200
    if (!any && time_to_call && a_->current_net().sysnum && node_supports_callout) {
      // also try this.
      Clear();
      attempt_callout();
      any = true;
    }
    DrawScreen();
    bout.okskey(false);
    if (io->KeyPressed()) {
      a_->set_at_wfc(false);
      a_->ReadCurrentUser(sysop_usernum);
      read_qscn(1, a()->context().qsc, false);
      a_->set_at_wfc(true);
      ch = to_upper_case<char>(io->GetChar());
      if (ch == 0) {
        ch = io->GetChar();
        a_->handle_sysop_key(ch);
        ch = 0;
      }
    } else {
      ch = 0;
      giveup_timeslice();
    }
    if (ch) {
      a_->set_at_wfc(true);
      any = true;
      bout.okskey(true);
      resetnsp();
      io->SetCursor(LocalIO::cursorNormal);
      switch (ch) {
        // Local Logon
      case SPACE:
        lokb = LocalLogon();
        break;
        // Show WFC Menu
      case '?': {
        string helpFileName = SWFC_NOEXT;
        char chHelp = ESC;
        do {
          io->Cls();
          bout.nl();
          print_help_file(helpFileName);
          chHelp = bout.getkey();
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
        if (a_->current_net().sysnum > 0 || !a_->net_networks.empty()) {
          io->GotoXY(2, 23);
          bout << "|#7(|#2Q|#7=|#2Quit|#7) Display Which NETDAT Log File (|#10|#7-|#12|#7): ";
          ch = onek("Q012");
          switch (ch) {
          case '0':
          case '1':
          case '2': {
            print_local_file(fmt::format("netdat{}.log", ch));
          } break;
          }
        }
        break;
        // Net List
      case '`':
        if (a_->current_net().sysnum) {
          print_net_listing(true);
        }
        break;
        // [ESC] Quit the BBS
      case ESC:
        io->GotoXY(2, 23);
        bout << "|#7Exit the BBS? ";
        if (yesno()) {
          a_->QuitBBS();
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
        a_->usernum = 1;
        bout.bputs("|#1Send Email:");
        send_email();
        a_->WriteCurrentUser(sysop_usernum);
        cleanup_net();
        break;
      case 'F': {
        Clear();
        bout.bputs("|#1Enter Number: ");
        auto x = input_number_or_key_raw(1, 0, 2112, true, {'Q', '?', '/'});
        bout << "key: " << x.key << "; num: " << x.num;
        pausescr();
      } break;
      // GfileEdit
      case 'G':
        write_inst(INST_LOC_GFILEEDIT, 0, INST_FLAGS_NONE);
        gfileedit();
        break;
        // Send Internet Mail
      case 'I': {
        Clear();
        a_->usernum = 1;
        a_->SetUserOnline(true);
        get_user_ppp_addr();
        send_inet_email();
        a_->SetUserOnline(false);
        a_->WriteCurrentUser(sysop_usernum);
        cleanup_net();
      } break;
      // ConfEdit
      case 'J':
        Clear();
        edit_confs();
        break;
        // SendMailFile
      case 'K': {
        Clear();
        a_->usernum = 1;
        bout << "|#1Send any Text File in Email:\r\n\n|#2Filename: ";
        auto buffer = input_path(50);
        LoadFileIntoWorkspace(buffer, false);
        send_email();
        a_->WriteCurrentUser(sysop_usernum);
        cleanup_net();
      } break;
      // Print Log Daily logs
      case 'L': {
        Clear();
        auto status = a()->status_manager()->GetStatus();
        print_local_file(status->GetLogFileName(0));
      } break;
      // Read User Mail
      case 'M': {
        Clear();
        a_->usernum = sysop_usernum;
        readmail(0);
        a_->WriteCurrentUser(sysop_usernum);
        cleanup_net();
      } break;
      // Print Net Log
      case 'N': {
        Clear();
        print_local_file("net.log");
      } break;
      // EditTextFile
      case 'O': {
        Clear();
        write_inst(INST_LOC_TEDIT, 0, INST_FLAGS_NONE);
        bout << "\r\n|#1Edit any Text File: \r\n\n|#2Filename: ";
        const auto current_dir_slash = File::EnsureTrailingSlashPath(File::current_directory());
        auto net_filename = input_path(current_dir_slash, 50);
        if (!net_filename.empty()) {
          external_text_edit(net_filename, "", 500, MSGED_FLAG_NO_TAGLINE);
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
        a_->QuitBBS();
        break;
        // Read All Mail
      case 'R':
        Clear();
        write_inst(INST_LOC_MAILR, 0, INST_FLAGS_NONE);
        mailr();
        break;
        // Print Current Status
      case 'S':
        prstatus();
        bout.getkey();
        break;
      case 'T':
        if (a()->terminal_command.empty()) {
          bout << "Terminal Command not specified. " << wwiv::endl
               << " Please set TERMINAL_CMD in wwiv.ini" << wwiv::endl;
          bout.getkey();
          break;
        }
        exec_cmdline(a()->terminal_command, INST_FLAGS_NONE);
        break;
      case 'U': {
        // User edit
        const auto exe = PathFilePath(a()->bindir(), "wwivconfig");
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
      // Print Environment
      case 'X':
        break;
        // Print Yesterday's Log
      case 'Y': {
        Clear();
        auto status = a()->status_manager()->GetStatus();
        print_local_file(status->GetLogFileName(1));
      } break;
      // Print Activity (Z) Log
      case 'Z': {
        zlog();
        bout.nl();
        bout.getkey();
      } break;
      }
      Clear(); // moved from after getch
      if (!a()->context().incom() && !lokb) {
        frequent_init();
        a_->ReadCurrentUser(sysop_usernum);
        read_qscn(1, a()->context().qsc, false);
        a_->reset_effective_sl();
        a_->usernum = sysop_usernum;
      }
      catsl();
      write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
    }

    if (!any) {
      if (a_->IsCleanNetNeeded()) {
        // let's try this.
        Clear();
        cleanup_net();
      }
      giveup_timeslice();
    }
  } while (!a()->context().incom() && !lokb);
  return lokb;
}

int WFC::LocalLogon() {
  a_->localIO()->GotoXY(2, 23);
  bout << "|#9Log on to the BBS?";
  auto d = steady_clock::now();
  int lokb = 0;
  // TODO(rushfan): use wwiv::os::wait_for
  while (!a_->localIO()->KeyPressed() && (steady_clock::now() - d < minutes(1))) {
    wwiv::os::sleep_for(10ms);
  }

  if (a_->localIO()->KeyPressed()) {
    auto ch = to_upper_case<char>(a_->localIO()->GetChar());
    if (ch == 'Y') {
      a_->localIO()->Puts(YesNoString(true));
      bout << wwiv::endl;
      lokb = 1;
    } else if (ch == 0 || static_cast<unsigned char>(ch) == 224) {
      // The ch == 224 is a Win32'ism
      a_->localIO()->GetChar();
    } else {
      auto fast = false;

      if (ch == 'F') { // 'F' for Fast
        a_->unx_ = 1;
        fast = true;
      } else {
        switch (ch) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          fast = true;
          a_->unx_ = ch - '0';
          break;
        }
      }
      if (!fast || a_->unx_ > a_->status_manager()->GetUserCount()) {
        return lokb;
      }

      User tu;
      a_->users()->readuser_nocache(&tu, a_->unx_);
      if (tu.GetSl() != 255 || tu.IsUserDeleted()) {
        return lokb;
      }

      a_->usernum = a_->unx_;
      auto saved_at_wfc = a_->at_wfc();
      a_->set_at_wfc(false);
      a_->ReadCurrentUser();
      read_qscn(a_->usernum, a()->context().qsc, false);
      a_->set_at_wfc(saved_at_wfc);
      bout.bputch(ch);
      a_->localIO()->Puts("\r\n\r\n\r\n\r\n\r\n\r\n");
      lokb = 2;
      a_->reset_effective_sl();
      changedsl();
      if (!set_language(a_->user()->GetLanguage())) {
        a_->user()->SetLanguage(0);
        set_language(0);
      }
      return lokb;
    }
    if (ch == 0 || static_cast<unsigned char>(ch) == 224) {
      // The 224 is a Win32'ism
      a_->localIO()->GetChar();
    }
  }
  if (lokb == 0) {
    a_->Cls();
  }
  return lokb;
}

} // namespace wwiv
