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
#ifdef _WIN32
// include this here so it won't get includes by local_io_win32.h
#include "core/wwiv_windows.h"
#include <io.h>
#endif // WIN32
#include "bbs/application.h"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "bbs/asv.h"
#include "bbs/bbs.h"
#include "bbs/bbsovl2.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/bbsutl2.h"
#include "bbs/bgetch.h"
#include "bbs/com.h"
#include "bbs/confutil.h"
#include "bbs/datetime.h"
#include "bbs/events.h"
#include "bbs/exceptions.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/lilo.h"
#include "bbs/menu.h"
#include "bbs/netsup.h"
#include "bbs/null_remote_io.h"
#include "bbs/pause.h"
#include "bbs/remote_io.h"
#include "bbs/ssh.h"
#include "bbs/subacc.h"
#include "bbs/syschat.h"
#include "bbs/sysopf.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/wfc.h"
#include "bbs/wqscn.h"
#include "bbs/xfer.h"
#include "core/command_line.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/version.h"
#include "local_io/local_io.h"
#include "local_io/local_io_curses.h"
#include "local_io/null_local_io.h" // Used for Linux build.
#include "local_io/wconstants.h"
#include "sdk/status.h"

#if defined(_WIN32)
#include "bbs/remote_socket_io.h"
#include "local_io/local_io_win32.h"
#else
#include <unistd.h>
#endif // _WIN32

using std::clog;
using std::cout;
using std::endl;
using std::max;
using std::min;
using std::string;
using std::unique_ptr;
using namespace wwiv::core;
using namespace std::chrono;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::strings;

Output bout;

Application::Application(LocalIO* localIO)
    : local_io_(localIO), oklevel_(exitLevelOK), errorlevel_(exitLevelNotOK), batch_(),
      session_context_(this) {
  ::bout.SetLocalIO(localIO);
  SetCurrentReadMessageArea(-1);

  tzset();
  srand(static_cast<unsigned int>(time_t_now()));

  memset(&asv, 0, sizeof(asv_rec));
  newuser_colors = {7, 11, 14, 5, 31, 2, 12, 9, 6, 3};
  newuser_bwcolors = {7, 15, 15, 15, 112, 15, 15, 7, 7, 7};
  User::CreateNewUserRecord(&thisuser_, 50, 20, 0, 0.1234f, newuser_colors, newuser_bwcolors);

  // Set the home directory
  current_dir_ = File::current_directory();
}

Application::~Application() {
  if (comm_ && context().ok_modem_stuff()) {
    comm_->close(false);
    comm_.reset(nullptr);
  }
  if (local_io_) {
    local_io_->SetCursor(LocalIO::cursorNormal);
  }
  // CursesIO.
  delete out;
  out = nullptr;
}

wwiv::bbs::SessionContext& Application::context() { return session_context_; }

bool Application::reset_local_io(LocalIO* wlocal_io) {
  local_io_.reset(wlocal_io);
  ::bout.SetLocalIO(wlocal_io);

  const auto screen_bottom = localIO()->GetDefaultScreenBottom();
  localIO()->SetScreenBottom(screen_bottom);
  defscreenbottom = screen_bottom;
  screenlinest = screen_bottom + 1;

  ClearTopScreenProtection();
  return true;
}

void Application::CreateComm(unsigned int nHandle, CommunicationType type) {
  switch (type) {
  case CommunicationType::SSH: {
    const File key_file(FilePath(config_->datadir(), "wwiv.key"));
    const auto system_password = config()->system_password();
    wwiv::bbs::Key key(key_file.full_pathname(), system_password);
    if (!key_file.Exists()) {
      LOG(ERROR) << "Key file doesn't exist. Will try to create it.";
      if (!key.Create()) {
        LOG(ERROR) << "Unable to create or open key file!.  SSH will be disabled!" << endl;
        type = CommunicationType::TELNET;
      }
    }
    if (!key.Open()) {
      LOG(ERROR) << "Unable to open key file!. Did you change your sytem pw?" << endl;
      LOG(ERROR) << "If so, delete " << key_file.full_pathname();
      LOG(ERROR) << "SSH will be disabled!";
      type = CommunicationType::TELNET;
    }
    comm_.reset(new wwiv::bbs::IOSSH(nHandle, key));
  } break;
  case CommunicationType::TELNET: {
    comm_.reset(new RemoteSocketIO(nHandle, true));
  } break;
  case CommunicationType::NONE: {
    comm_.reset(new NullRemoteIO());
  } break;
  }
  bout.SetComm(comm_.get());
}

void Application::SetCommForTest(RemoteIO* remote_io) {
  comm_.reset(remote_io);
  bout.SetComm(remote_io);
}

bool Application::ReadCurrentUser(int user_number) {
  if (!users()->readuser(&thisuser_, user_number)) {
    return false;
  }
  last_read_user_number_ = user_number;
  // Update all other session variables that are dependent.
  screenlinest = (using_modem) ? user()->GetScreenLines() : defscreenbottom + 1;
  return true;
}

bool Application::WriteCurrentUser(int user_number) {

  if (user_number == 0) {
    LOG(ERROR) << "Trying to call WriteCurrentUser with user_number 0";
  } else if (user_number != last_read_user_number_) {
    LOG(ERROR) << "Trying to call WriteCurrentUser with user_number: " << user_number
               << "; last_read_user_number_: " << last_read_user_number_;
  }
  return users()->writeuser(&thisuser_, user_number);
}

void Application::tleft(bool check_for_timeout) {
  auto nsln = nsl();

  // Check for tineout 1st.
  if (check_for_timeout && IsUserOnline()) {
    if (nsln == 0.0) {
      bout << "\r\nTime expired.\r\n\n";
      Hangup();
    }
    return;
  }

  // If we're not displaying the time left or password on the
  // topdata display, leave now.
  if (topdata == LocalIO::topdataNone) {
    return;
  }

  bool temp_sysop = user()->GetSl() != 255 && effective_sl() == 255;
  auto sysop_available = sysop1();

  int cx = localIO()->WhereX();
  int cy = localIO()->WhereY();
  int ctl = localIO()->GetTopLine();
  int cc = bout.curatr();
  bout.curatr(localIO()->GetTopScreenColor());
  localIO()->SetTopLine(0);
  int line_number = (chatcall_ && (topdata == LocalIO::topdataUser)) ? 5 : 4;

  localIO()->PutsXY(1, line_number, GetCurrentSpeed());
  for (int i = localIO()->WhereX(); i < 23; i++) {
    localIO()->Putch(static_cast<unsigned char>('\xCD'));
  }

  if (temp_sysop) {
    localIO()->PutsXY(23, line_number, "Temp Sysop");
  }

  if (sysop_available) {
    localIO()->PutsXY(64, line_number, "Available");
  }

  auto min_left = nsln / SECONDS_PER_MINUTE;
  auto secs_left = nsln % SECONDS_PER_MINUTE;
  auto tleft_display = wwiv::strings::StringPrintf("T-%4ldm %2lds", min_left, secs_left);
  switch (topdata) {
  case LocalIO::topdataSystem: {
    if (IsUserOnline()) {
      localIO()->PutsXY(18, 3, tleft_display);
    }
  } break;
  case LocalIO::topdataUser: {
    if (IsUserOnline()) {
      localIO()->PutsXY(18, 3, tleft_display);
    } else {
      localIO()->PutsXY(18, 3, user()->GetPassword());
    }
  } break;
  }
  localIO()->SetTopLine(ctl);
  bout.curatr(cc);
  localIO()->GotoXY(cx, cy);
}

void Application::handle_sysop_key(uint8_t key) {
  int i, i1;

  if (bout.okskey()) {
    if (key >= AF1 && key <= AF10) {
      set_autoval(key - 104);
    } else {
      switch (key) {
      case F1: /* F1 */
        OnlineUserEditor();
        break;
      case SF1:
        /* Shift-F1 */
        // Nothing.
        UpdateTopScreen();
        break;
      case CF1: /* Ctrl-F1 */
        // Used to be shutdown bbs in 3 minutes.
        break;
      case F2: /* F2 */
        topdata++;
        if (topdata > LocalIO::topdataUser) {
          topdata = LocalIO::topdataNone;
        }
        UpdateTopScreen();
        break;
      case F3: /* F3 */
        if (using_modem) {
          a()->context().incom(!a()->context().incom());
          bout.dump();
          tleft(false);
        }
        break;
      case F4: /* F4 */
        chatcall_ = false;
        UpdateTopScreen();
        break;
      case F5: /* F5 */
        remoteIO()->disconnect();
        Hangup();
        break;
      case SF5: /* Shift-F5 */
        i1 = (rand() % 20) + 10;
        for (i = 0; i < i1; i++) {
          bout.bputch(static_cast<unsigned char>(rand() % 256));
        }
        remoteIO()->disconnect();
        Hangup();
        break;
      case CF5: /* Ctrl-F5 */
        bout << "\r\nCall back later when you are there.\r\n\n";
        remoteIO()->disconnect();
        Hangup();
        break;
      case F6: /* F6 - was Toggle Sysop Alert*/
        tleft(false);
        break;
      case F7: /* F7 */
        user()->subtract_extratime(std::chrono::minutes(5));
        tleft(false);
        break;
      case F8: /* F8 */
        user()->add_extratime(std::chrono::minutes(5));
        tleft(false);
        break;
      case F9: /* F9 */
        if (user()->GetSl() != 255) {
          if (effective_sl() != 255) {
            effective_sl(255);
          } else {
            reset_effective_sl();
          }
          changedsl();
          tleft(false);
        }
        break;
      case F10: /* F10 */
        if (chatting_ == 0) {
          if (config()->sysconfig_flags() & sysconfig_2_way) {
            chat1("", true);
          } else {
            chat1("", false);
          }
        } else {
          chatting_ = 0;
        }
        break;
      case CF10: /* Ctrl-F10 */
        if (chatting_ == 0) {
          chat1("", false);
        } else {
          chatting_ = 0;
        }
        break;
      case HOME: /* HOME */
        if (chatting_ == 1) {
          chat_file_ = !chat_file_;
        }
        break;
      }
    }
  }
}

void Application::DisplaySysopWorkingIndicator(bool displayWait) {
  const string waitString = "-=[WAIT]=-";
  auto nNumPrintableChars = waitString.length();
  for (std::string::const_iterator iter = waitString.begin(); iter != waitString.end(); ++iter) {
    if (*iter == 3 && nNumPrintableChars > 1) {
      nNumPrintableChars -= 2;
    }
  }

  if (displayWait) {
    if (okansi()) {
      int nSavedAttribute = bout.curatr();
      bout.SystemColor(user()->color(3));
      bout << waitString << "\x1b[" << nNumPrintableChars << "D";
      bout.SystemColor(static_cast<unsigned char>(nSavedAttribute));
    } else {
      bout << waitString;
    }
  } else {
    if (okansi()) {
      for (unsigned int j = 0; j < nNumPrintableChars; j++) {
        bout.bputch(' ');
      }
      bout.Left(nNumPrintableChars);
    } else {
      for (unsigned int j = 0; j < nNumPrintableChars; j++) {
        bout.bs();
      }
    }
  }
}

void Application::UpdateTopScreen() {
  if (at_wfc()) {
    return;
  }

  auto status = (status_manager()->GetStatus());
  char i;
  char sl[82], ar[17], dar[17], restrict[17], rst[17];

  int lll = bout.lines_listed();

  if (so() && !a()->context().incom()) {
    topdata = LocalIO::topdataNone;
  }

#ifdef _WIN32
  if (config()->sysconfig_flags() & sysconfig_titlebar) {
    // Only set the titlebar if the user wanted it that way.
    const string username_num = names()->UserName(usernum);
    string title = StringPrintf("WWIV Node %d (User: %s)", instance_number(), username_num.c_str());
    ::SetConsoleTitle(title.c_str());
  }
#endif // _WIN32

  switch (topdata) {
  case LocalIO::topdataNone:
    localIO()->set_protect(0);
    break;
  case LocalIO::topdataSystem:
    localIO()->set_protect(5);
    break;
  case LocalIO::topdataUser:
    if (chatcall_) {
      localIO()->set_protect(6);
    } else {
      if (localIO()->GetTopLine() == 6) {
        localIO()->set_protect(0);
      }
      localIO()->set_protect(5);
    }
    break;
  }
  // This used to be inside of localIO::set_protect but that
  // made absolutely no sense, so pulled it out here
  if (!using_modem) {
    screenlinest = defscreenbottom + 1 - localIO()->GetTopLine();
  }

  int cx = localIO()->WhereX();
  int cy = localIO()->WhereY();
  int nOldTopLine = localIO()->GetTopLine();
  int cc = bout.curatr();
  bout.curatr(localIO()->GetTopScreenColor());
  localIO()->SetTopLine(0);
  for (i = 0; i < 80; i++) {
    sl[i] = '\xCD';
  }
  sl[80] = '\0';

  switch (topdata) {
  case LocalIO::topdataNone:
    break;
  case LocalIO::topdataSystem: {
    const auto sn = lpad_to(config()->system_name(), 50);
    const auto ld = lpad_to(status->GetLastDate(), 8);
    localIO()->PutsXY(0, 0, StrCat(sn, "  Activity for ", ld, ":      "));

    localIO()->PutsXY(
        0, 1,
        StringPrintf(
            "Users: %4u       Total Calls: %5lu      Calls Today: %4u    Posted      :%3u ",
            status->GetNumUsers(), status->GetCallerNumber(), status->GetNumCallsToday(),
            status->GetNumLocalPosts()));

    const string username_num = names()->UserName(usernum);
    localIO()->PutsXY(0, 2,
                      StringPrintf("%-36s      %-4u min   /  %2u%%    E-mail sent :%3u ",
                                   username_num.c_str(), status->GetMinutesActiveToday(),
                                   static_cast<int>(10 * status->GetMinutesActiveToday() / 144),
                                   status->GetNumEmailSentToday()));

    User sysop{};
    int feedback_waiting = 0;
    if (users()->readuser_nocache(&sysop, 1)) {
      feedback_waiting = sysop.GetNumMailWaiting();
    }
    localIO()->PutsXY(
        0, 3,
        StringPrintf(
            "SL=%3u   DL=%3u               FW=%3u      Uploaded:%2u files    Feedback    :%3u ",
            user()->GetSl(), user()->GetDsl(), feedback_waiting, status->GetNumUploadsToday(),
            status->GetNumFeedbackSentToday()));
  } break;
  case LocalIO::topdataUser: {
    to_char_array(rst, restrict_string);
    for (i = 0; i <= 15; i++) {
      if (user()->HasArFlag(1 << i)) {
        ar[i] = static_cast<char>('A' + i);
      } else {
        ar[i] = SPACE;
      }
      if (user()->HasDarFlag(1 << i)) {
        dar[i] = static_cast<char>('A' + i);
      } else {
        dar[i] = SPACE;
      }
      if (user()->HasRestrictionFlag(1 << i)) {
        restrict[i] = rst[i];
      } else {
        restrict[i] = SPACE;
      }
    }
    dar[16] = '\0';
    ar[16] = '\0';
    restrict[16] = '\0';
    string lo;
    if (date() != user()->GetLastOn()) {
      lo = user()->GetLastOn();
    } else {
      lo = StringPrintf("Today:%2d", user()->GetTimesOnToday());
    }

    const auto username_num = names()->UserName(usernum);
    auto line =
        StringPrintf("%-35s W=%3u UL=%4u/%6lu SL=%3u LO=%5u PO=%4u", username_num.c_str(),
                     user()->GetNumMailWaiting(), user()->GetFilesUploaded(), user()->GetUploadK(),
                     user()->GetSl(), user()->GetNumLogons(), user()->GetNumMessagesPosted());
    localIO()->PutsXYA(0, 0, bout.curatr(), line);

    string callsign_or_regnum = user()->GetCallsign();
    if (user()->GetWWIVRegNumber()) {
      callsign_or_regnum = std::to_string(user()->GetWWIVRegNumber());
    }
    auto used_this_session = (system_clock::now() - system_logon_time());
    auto used_total = used_this_session + user()->timeon();
    auto minutes_used = duration_cast<minutes>(used_total);

    localIO()->PutsXY(0, 1,
                      StringPrintf("%-20s %12s  %-6s DL=%4u/%6lu DL=%3u TO=%5.0d ES=%4u",
                                   user()->GetRealName(), user()->GetVoicePhoneNumber(),
                                   callsign_or_regnum.c_str(), user()->GetFilesDownloaded(),
                                   user()->GetDownloadK(), user()->GetDsl(), minutes_used.count(),
                                   user()->GetNumEmailSent() + user()->GetNumNetEmailSent()));

    localIO()->PutsXY(0, 2,
                      StringPrintf("ARs=%-16s/%-16s R=%-16s EX=%3u %-8s FS=%4u", ar, dar, restrict,
                                   user()->GetExempt(), lo.c_str(), user()->GetNumFeedbackSent()));

    User sysop{};
    int feedback_waiting = 0;
    if (users()->readuser_nocache(&sysop, 1)) {
      feedback_waiting = sysop.GetNumMailWaiting();
    }
    localIO()->PutsXY(0, 3,
                      StringPrintf("%-40.40s %c %2u %-16.16s           FW= %3u",
                                   user()->GetNote().c_str(), user()->GetGender(), user()->GetAge(),
                                   ctypes(user()->GetComputerType()).c_str(), feedback_waiting));

    if (chatcall_) {
      localIO()->PutsXY(0, 4, chat_reason_);
    }
  } break;
  }
  if (nOldTopLine != 0) {
    localIO()->PutsXY(0, nOldTopLine - 1, sl);
  }
  localIO()->SetTopLine(nOldTopLine);
  localIO()->GotoXY(cx, cy);
  bout.curatr(cc);
  tleft(false);

  bout.lines_listed_ = lll;
}

void Application::ClearTopScreenProtection() {
  localIO()->set_protect(0);
  if (!using_modem) {
    screenlinest = defscreenbottom + 1 - localIO()->GetTopLine();
  }
}

void Application::Cls() {
  localIO()->Cls();
  bout.clear_lines_listed();
}

const char* Application::network_name() const {
  if (net_networks.empty()) {
    return "";
  }
  return net_networks[network_num_].name;
}

const std::string Application::network_directory() const {
  if (net_networks.empty()) {
    return "";
  }
  return std::string(net_networks[network_num_].dir);
}

void Application::GetCaller() {
  wwiv::bbs::WFC wfc(this);
  remoteIO()->remote_info().clear();
  frequent_init();
  usernum = 0;
  // Since hang_it_up sets hangup_ = true, let's ensure we're always
  // not in this state when we enter the WFC.
  hangup_ = false;
  set_at_wfc(false);
  write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
  // We'll read the sysop record for defaults, but let's set
  // usernum to 0 here since we don't want to botch up the
  // sysop's record if things go wrong.
  // TODO(rushfan): Let's make record 0 the logon defaults
  // and stop using the sysop record.
  ReadCurrentUser(1);
  read_qscn(1, context().qsc, false);
  // N.B. This used to be 1.
  usernum = 0;

  reset_effective_sl();
  if (user()->IsUserDeleted()) {
    user()->SetScreenChars(80);
    user()->SetScreenLines(25);
  }
  screenlinest = defscreenbottom + 1;

  int lokb = wfc.doWFCEvents();

  if (lokb) {
    modem_speed_ = 38400;
  }

  using_modem = a()->context().incom() ? 1 : 0;
  if (lokb == 2) {
    using_modem = -1;
  }

  bout.okskey(true);
  Cls();
  localIO()->Puts(StrCat("Logging on at ", GetCurrentSpeed(), " ...\r\n"));
  set_at_wfc(false);
}

void Application::GotCaller(unsigned int ms) {
  frequent_init();
  wfc_cls(a());
  modem_speed_ = ms;
  ReadCurrentUser(1);
  read_qscn(1, context().qsc, false);
  reset_effective_sl();
  usernum = 1;
  if (user()->IsUserDeleted()) {
    user()->SetScreenChars(80);
    user()->SetScreenLines(25);
  }
  Cls();
  localIO()->Puts(StrCat("Logging on at ", GetCurrentSpeed(), " ...\r\n"));
  if (ms) {
    a()->context().incom(true);
    a()->context().outcom(true);
    using_modem = 1;
  } else {
    using_modem = 0;
    a()->context().incom(false);
    a()->context().outcom(false);
  }
}

void Application::CdHome() { File::set_current_directory(current_dir_); }

const string Application::GetHomeDir() const noexcept { return current_dir_; }
const std::string Application::bbsdir() const noexcept { return current_dir_; }
const std::string Application::bindir() const noexcept { return bindir_; }
const std::string Application::configdir() const noexcept { return configdir_; }
const std::string Application::logdir() const noexcept { return logdir_; }
int Application::verbose() const noexcept { return verbose_; }

void Application::AbortBBS(bool bSkipShutdown) {
  clog.flush();
  ExitBBSImpl(errorlevel_, !bSkipShutdown);
}

void Application::QuitBBS() { ExitBBSImpl(Application::exitLevelQuit, true); }

void Application::ExitBBSImpl(int exit_level, bool perform_shutdown) {
  if (perform_shutdown) {
    write_inst(INST_LOC_DOWN, 0, INST_FLAGS_NONE);
    if (exit_level != Application::exitLevelOK && exit_level != Application::exitLevelQuit) {
      // Only log the exiting at abnormal error levels, since we see lots of exiting statements
      // in the logs that don't correspond to sessions every being created (network probers, etc).
      sysoplog(false);
      sysoplog(false) << "WWIV " << wwiv_version << ", inst " << instance_number()
                      << ", taken down at " << times() << " on " << fulldate() << " with exit code "
                      << exit_level << ".";
      sysoplog(false);
    }
    catsl();
    clog << "\r\n";
    clog << "WWIV Bulletin Board System " << wwiv_version << beta_version
         << " exiting at error level " << exit_level << endl
         << endl;
    clog.flush();
  }

  // We just delete the session class, not the application class
  // since one day it'd be ideal to have 1 application contain
  // N sessions for N>1.
  delete this;
  exit(exit_level);
}

int Application::Run(int argc, char* argv[]) {
  unsigned int ui = 0;
  bool ooneuser = false;
  CommunicationType type = CommunicationType::NONE;

  CommandLine cmdline(argc, argv, "");
  cmdline.AddStandardArgs();
  cmdline.set_no_args_allowed(true);
  cmdline.add_argument({"error_exit", 'a', "Specify the Error Exit Level", "1"});
  cmdline.add_argument({"bps", 'b', "Modem speed of logged on user", "115200"});
  cmdline.add_argument(
      BooleanCommandLineArgument{"beginday", 'e', "Load for beginday event only", false});
  cmdline.add_argument({"handle", 'h', "Socket handle", "0"});
  cmdline.add_argument(BooleanCommandLineArgument{"no_modem", 'm',
                                                  "Don't access the modem at all", false});
  cmdline.add_argument({"instance", 'n', "Designate instance number <inst>", "1"});
  cmdline.add_argument({"ok_exit", 'q', "Normal exit level", "0"});
  cmdline.add_argument({"remaining_min", 'r', "Specify max # minutes until event", "0"});
  cmdline.add_argument({"user_num", 'u', "Pass usernumber <user#> online", "0"});
  cmdline.add_argument(BooleanCommandLineArgument{"version", 'V', "Display version.", false});
  cmdline.add_argument({"x", 'x', "Someone is logged in with t for telnet or s for ssh.", ""});
  cmdline.add_argument(BooleanCommandLineArgument{"no_hangup", 'z',
                                                  "Do not hang up on user when at log off", false});

  if (!cmdline.Parse()) {
    cout << "WWIV Bulletin Board System [" << wwiv_version << beta_version << "]\r\n\n";
    cout << cmdline.GetHelp() << endl;
    return EXIT_FAILURE;
  } else if (cmdline.help_requested()) {
    cout << "WWIV Bulletin Board System [" << wwiv_version << beta_version << "]\r\n\n";
    cout << cmdline.GetHelp() << endl;
    ExitBBSImpl(0, false);
    return EXIT_SUCCESS;
  } else  if (cmdline.barg("version")) {
    cout << "WWIV Bulletin Board System [" << wwiv_version << beta_version << "]\r\n\n";
    return 0;
  }

  const std::string bbs_env = environment_variable("BBS");
  if (!bbs_env.empty()) {
    if (bbs_env.find("WWIV") != string::npos) {
      LOG(ERROR) << "You are already in the BBS, type 'EXIT' instead.\n\n";
      ExitBBSImpl(255, false);
    }
  }
  
  bout.curatr(0x07);
  // Set the directories.
  current_dir_ = cmdline.bbsdir();
  bindir_ = cmdline.bindir();
  logdir_ = cmdline.logdir();
  configdir_ = cmdline.configdir();
  verbose_ = cmdline.verbose();
  File::set_current_directory(current_dir_);
  
  oklevel_ = cmdline.iarg("ok_exit");
  errorlevel_ = cmdline.iarg("error_exit");
  unsigned int hSockOrComm = cmdline.iarg("handle");
  no_hangup_ = cmdline.barg("no_hangup");
  int num_min = cmdline.iarg("remaining_min");
  context().ok_modem_stuff(!cmdline.barg("no_modem"));
  instance_number_ = cmdline.iarg("instance");

  uint16_t this_usernum_from_commandline = cmdline.iarg("user_num");
  const auto x = cmdline.sarg("x");
  if (!x.empty()) {
    char xarg = to_upper_case<char>(x.at(0));
    if (cmdline.arg("handle").is_default()) {
      clog << "-h must be specified when using '"
           << "-x" << x << "'" << std::endl;
      ExitBBSImpl(errorlevel_, false);
    } else if (xarg == 'T' || xarg == 'S') {
      ui = cmdline.iarg("bps");
      SetCurrentSpeed(std::to_string(ui));
      // Set it false until we call LiLo
      user_already_on_ = true;
      ooneuser = true;
      using_modem = 0;
      a()->context().incom(true);
      a()->context().outcom(false);
      type = (xarg == 'S') ? CommunicationType::SSH : CommunicationType::TELNET;
    } else {
      clog << "Invalid Command line argument given '" << "-x" << x << "'" << std::endl;
      ExitBBSImpl(errorlevel_, false);
    }
  }
  if (this_usernum_from_commandline > 0) {
    if (!user_already_on_) {
      SetCurrentSpeed("KB");
    }
    user_already_on_ = true;
    ooneuser = true;
  }

  // Setup the full-featured localIO if we have a TTY (or console)
  if (isatty(fileno(stdin))) {
#if defined(_WIN32) && !defined(WWIV_WIN32_CURSES_IO)
    reset_local_io(new Win32ConsoleIO());
#else
    if (type == CommunicationType::NONE) {
      // We only want the localIO if we ran this locally at a terminal
      // and also not passed in from the telnet handler, etc.  On Windows
      // We always have a local console, so this is *NIX specific.
      CursesIO::Init(StringPrintf("WWIV BBS %s%s", wwiv_version, beta_version));
      reset_local_io(new CursesLocalIO(out->GetMaxY()));
    } else if (type == CommunicationType::TELNET || type == CommunicationType::SSH) {
      reset_local_io(new NullLocalIO());
    }
#endif
  } else {
#ifdef __unix__
    reset_local_io(new NullLocalIO());
#endif // __unix__
  }

  // Add the environment variable or overwrite the existing one
  const auto env_str = std::to_string(instance_number());
  set_environment_variable("WWIV_INSTANCE", env_str);
  if (!ReadConfig()) {
    // Gotta read the config before we can create the socket handles.
    // Since we may need the SSH key.
    AbortBBS(true);
  }

  CreateComm(hSockOrComm, type);
  InitializeBBS();
  localIO()->UpdateNativeTitleBar(config()->system_name(), instance_number());

  bool remote_opened = true;
  // If we are telnet...
  if (type == CommunicationType::TELNET || type == CommunicationType::SSH) {
    context().ok_modem_stuff(true);
    remote_opened = remoteIO()->open();
  }

  if (!remote_opened) {
    // Remote side disconnected.
    clog << "Remote side disconnected." << std::endl;
    ExitBBSImpl(oklevel_, false);
  }

  if (cmdline.barg("beginday")) {
    auto status = status_manager()->GetStatus();
    cleanup_events();
    if (date() != status->GetLastDate()) {
      // This may be another node, but the user explicitly wanted to run the beginday
      // event from the commandline, so we'll just check the date.
      beginday(true);
    } else {
      sysoplog(false) << "!!! Wanted to run the beginday event when it's not required!!!";
      clog << "! WARNING: Tried to run beginday event again\r\n\n";
      sleep_for(seconds(2));
    }
    ExitBBSImpl(oklevel_, true);
  }

  do {
    if (this_usernum_from_commandline) {
      usernum = this_usernum_from_commandline;
      ReadCurrentUser();
      if (!user()->IsUserDeleted()) {
        GotCaller(ui);
        usernum = this_usernum_from_commandline;
        ReadCurrentUser();
        read_qscn(usernum, context().qsc, false);
        reset_effective_sl();
        changedsl();
        a()->context().okmacro(true);
      } else {
        this_usernum_from_commandline = 0;
      }
    }
    try {
      // Try setting this at the top of the try loop. It's currently only
      // set in logon() which could cause problems if we get hung up before then.
      SetLogonTime();

      if (!this_usernum_from_commandline) {
        if (user_already_on_) {
          GotCaller(ui);
        } else {
          GetCaller();
        }
      }

      if (using_modem > -1) {
        if (!this_usernum_from_commandline) {
          getuser();
        }
      } else {
        using_modem = 0;
        a()->context().okmacro(true);
        usernum = unx_;
        reset_effective_sl();
        changedsl();
      }
      this_usernum_from_commandline = 0;
      CheckForHangup();
      // Set new timeout
      logon();
      setiia(seconds(5));
      set_net_num(0);
      while (!hangup_ && usernum > 0) {
        CheckForHangup();
        filelist.clear();
        zap_ed_info();
        write_inst(INST_LOC_MAIN, current_user_sub().subnum, INST_FLAGS_NONE);
        try {
          wwiv::menus::mainmenu();
        } catch (const std::logic_error& le) {
          std::cerr << "Caught std::logic_error: " << le.what() << "\r\n";
          sysoplog() << le.what();
        }
      }
    } catch (const wwiv::bbs::hangup_error& h) {
      if (IsUserOnline()) {
        // Don't need to log this unless the user actually made it online.
        std::cerr << h.what() << "\r\n";
        sysoplog() << h.what();
      }
    }
    logoff();
    {
      // post_logoff_cleanup
      remove_from_temp("*.*", temp_directory(), false);
      remove_from_temp("*.*", batch_directory(), false);
      if (!batch().entry.empty() && (batch().entry.size() != batch().numbatchdl())) {
        for (const auto& b : batch().entry) {
          if (!b.sending) {
            didnt_upload(b);
          }
        }
      }
      batch().clear();
    }
    if (!no_hangup_ && using_modem && context().ok_modem_stuff()) {
      hang_it_up();
    }
    catsl();
    frequent_init();
    wfc_cls(a());
    cleanup_net();

    if (!no_hangup_ && context().ok_modem_stuff()) {
      remoteIO()->disconnect();
    }
    user_already_on_ = false;
  } while (!ooneuser);

  return oklevel_;
}
