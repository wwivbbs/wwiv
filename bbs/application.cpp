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
#include "local_io/wconstants.h"
#include "bbs/wfc.h"
#include "bbs/wqscn.h"
#include "bbs/xfer.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/version.h"
#include "local_io/local_io.h"
#include "local_io/local_io_curses.h"
#include "local_io/null_local_io.h" // Used for Linux build.
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
using std::exception;
using std::max;
using std::min;
using std::stoi;
using std::string;
using std::unique_ptr;
using wwiv::bbs::InputMode;
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
  if (out != nullptr) {
    delete out;
    out = nullptr;
  }
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
    const string system_password = config()->config()->systempw;
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
  long nsln = nsl();

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

  bool temp_sysop = user()->GetSl() != 255 && GetEffectiveSl() == 255;
  bool sysop_available = sysop1();

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
          if (GetEffectiveSl() != 255) {
            SetEffectiveSl(255);
          } else {
            ResetEffectiveSl();
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
      bout.SystemColor(user()->HasColor() ? user()->GetColor(3) : user()->GetBWColor(3));
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
      bout << "\x1b[" << nNumPrintableChars << "D";
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

  unique_ptr<WStatus> pStatus(status_manager()->GetStatus());
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
    localIO()->PrintfXY(0, 0, "%-50s  Activity for %8s:      ", config()->system_name().c_str(),
                        pStatus->GetLastDate());

    localIO()->PrintfXY(
        0, 1, "Users: %4u       Total Calls: %5lu      Calls Today: %4u    Posted      :%3u ",
        pStatus->GetNumUsers(), pStatus->GetCallerNumber(), pStatus->GetNumCallsToday(),
        pStatus->GetNumLocalPosts());

    const string username_num = names()->UserName(usernum);
    localIO()->PrintfXY(0, 2, "%-36s      %-4u min   /  %2u%%    E-mail sent :%3u ",
                        username_num.c_str(), pStatus->GetMinutesActiveToday(),
                        static_cast<int>(10 * pStatus->GetMinutesActiveToday() / 144),
                        pStatus->GetNumEmailSentToday());

    User sysop{};
    int feedback_waiting = 0;
    if (users()->readuser_nocache(&sysop, 1)) {
      feedback_waiting = sysop.GetNumMailWaiting();
    }
    localIO()->PrintfXY(
        0, 3, "SL=%3u   DL=%3u               FW=%3u      Uploaded:%2u files    Feedback    :%3u ",
        user()->GetSl(), user()->GetDsl(), feedback_waiting, pStatus->GetNumUploadsToday(),
        pStatus->GetNumFeedbackSentToday());
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

    const string username_num = names()->UserName(usernum);
    localIO()->PrintfXYA(0, 0, bout.curatr(), "%-35s W=%3u UL=%4u/%6lu SL=%3u LO=%5u PO=%4u",
                         username_num.c_str(), user()->GetNumMailWaiting(),
                         user()->GetFilesUploaded(), user()->GetUploadK(), user()->GetSl(),
                         user()->GetNumLogons(), user()->GetNumMessagesPosted());

    string callsign_or_regnum = user()->GetCallsign();
    if (user()->GetWWIVRegNumber()) {
      callsign_or_regnum = std::to_string(user()->GetWWIVRegNumber());
    }
    auto used_this_session = (system_clock::now() - system_logon_time());
    auto used_total = used_this_session + user()->timeon();
    auto minutes_used = duration_cast<minutes>(used_total);

    localIO()->PrintfXY(0, 1, "%-20s %12s  %-6s DL=%4u/%6lu DL=%3u TO=%5.0d ES=%4u",
                        user()->GetRealName(), user()->GetVoicePhoneNumber(),
                        callsign_or_regnum.c_str(), user()->GetFilesDownloaded(),
                        user()->GetDownloadK(), user()->GetDsl(), minutes_used.count(),
                        user()->GetNumEmailSent() + user()->GetNumNetEmailSent());

    localIO()->PrintfXY(0, 2, "ARs=%-16s/%-16s R=%-16s EX=%3u %-8s FS=%4u", ar, dar, restrict,
                        user()->GetExempt(), lo.c_str(), user()->GetNumFeedbackSent());

    User sysop{};
    int feedback_waiting = 0;
    if (users()->readuser_nocache(&sysop, 1)) {
      feedback_waiting = sysop.GetNumMailWaiting();
    }
    localIO()->PrintfXY(0, 3, "%-40.40s %c %2u %-16.16s           FW= %3u",
                        user()->GetNote().c_str(), user()->GetGender(), user()->GetAge(),
                        ctypes(user()->GetComputerType()).c_str(), feedback_waiting);

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

  ResetEffectiveSl();
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
  ResetEffectiveSl();
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

const string Application::GetHomeDir() {
  return current_dir_;
}

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

void Application::ShowUsage() {
  cout << "WWIV Bulletin Board System [" << wwiv_version << beta_version << "]\r\n\n"
       << "Usage:\r\n\n"
       << "bbs -N<inst> [options] \r\n\n"
       << "Options:\r\n\n"
       << "  -?         - Display command line options (This screen)\r\n\n"
       << "  -A<level>  - Specify the Error Exit Level\r\n"
       << "  -B<rate>   - Someone already logged on at rate (modem speed)\r\n"
       << "  -E         - Load for beginday event only\r\n"
       << "  -H<handle> - Socket handle\r\n"
       << "  -M         - Don't access modem at all\r\n"
       << "  -N<inst>   - Designate instance number <inst>\r\n"
       << "  -Q<level>  - Normal exit level\r\n"
       << "  -R<min>    - Specify max # minutes until event\r\n"
       << "  -U<user#>  - Pass usernumber <user#> online\r\n"
       << "  -V         - Display WWIV Version\r\n"
       << "  -XT        - Someone already logged on via telnet (socket handle)\r\n"
#if defined(_WIN32)
       << "  -XS        - Someone already logged on via SSH (socket handle)\r\n"
#endif // _WIN32
       << "  -Z         - Do not hang up on user when at log off\r\n"
       << endl;
}

int Application::Run(int argc, char* argv[]) {
  int num_min = 0;
  unsigned int ui = 0;
  unsigned short this_usernum_from_commandline = 0;
  bool ooneuser = false;
  bool event_only = false;
  CommunicationType type = CommunicationType::NONE;
  unsigned int hSockOrComm = 0;

  bout.curatr(0x07);
  // Set the instance, this may be changed by a command line argument
  instance_number_ = 1;
  context().ok_modem_stuff(true);

  const std::string bbs_env = environment_variable("BBS");
  if (!bbs_env.empty()) {
    if (bbs_env.find("WWIV") != string::npos) {
      LOG(ERROR) << "You are already in the BBS, type 'EXIT' instead.\n\n";
      ExitBBSImpl(255, false);
    }
  }
  const string wwiv_dir = environment_variable("WWIV_DIR");
  if (!wwiv_dir.empty()) {
    File::set_current_directory(wwiv_dir);
    // Reset current_dir_ since it was only set in the constructor.
    current_dir_ = File::current_directory();
  }

  for (int i = 1; i < argc; i++) {
    string argumentRaw = argv[i];
    if (argumentRaw.length() > 1 && (argumentRaw[0] == '-' || argumentRaw[0] == '/')) {
      string argument = argumentRaw.substr(2);
      char ch = to_upper_case<char>(argumentRaw[1]);
      switch (ch) {
      case 'B': {
        // I think this roundtrip here is just to ensure argument is really a number.
        ui = to_number<unsigned int>(argument);
        SetCurrentSpeed(std::to_string(ui));
        user_already_on_ = true;
      } break;
      case 'E':
        event_only = true;
        break;
      case 'Q':
        oklevel_ = stoi(argument);
        break;
      case 'A':
        errorlevel_ = stoi(argument);
        break;
      case 'H':
        hSockOrComm = stoi(argument);
        break;
      case 'M':
        context().ok_modem_stuff(false);
        break;
      case 'N': {
        instance_number_ = stoi(argument);
        if (instance_number_ <= 0 || instance_number_ > 999) {
          clog << "Your Instance can only be 1..999, you tried instance #" << instance_number_
               << endl;
          ExitBBSImpl(errorlevel_, false);
        }
      } break;
      case 'P':
        Cls();
        localIO()->Puts("Waiting for keypress...");
        (void)getchar();
        break;
      case 'R':
        num_min = stoi(argument);
        break;
      case 'U':
        this_usernum_from_commandline = to_number<uint16_t>(argument);
        if (!user_already_on_) {
          SetCurrentSpeed("KB");
        }
        user_already_on_ = true;
        ooneuser = true;
        break;
      case 'V':
        cout << "WWIV Bulletin Board System [" << wwiv_version << beta_version << "]" << endl;
        ExitBBSImpl(0, false);
        break;
      case 'X': {
        char argument2Char = to_upper_case<char>(argument.at(0));
        if (argument2Char == 'T' || argument2Char == 'S' || argument2Char == 'U') {
          // This more of a hack to make sure the WWIV
          // Server's -Bxxx parameter doesn't hose us.
          SetCurrentSpeed("115200");

          // These are needed for both Telnet or SSH
          SetUserOnline(false);
          ui = 115200;
          user_already_on_ = true;
          ooneuser = true;
          using_modem = 0;
          hangup_ = false;
          a()->context().incom(true);
          a()->context().outcom(false);
          if (argument2Char == 'T') {
            type = CommunicationType::TELNET;
          } else if (argument2Char == 'S') {
            type = CommunicationType::SSH;
          }
        } else {
          clog << "Invalid Command line argument given '" << argumentRaw << "'" << std::endl;
          ExitBBSImpl(errorlevel_, false);
        }
      } break;
      case 'Z':
        no_hangup_ = true;
        break;
        //
        // TODO Add handling for Socket and Comm handle here
        //
        //
      case '?':
        ShowUsage();
        ExitBBSImpl(0, false);
        break;
      case '-': {
        if (argumentRaw == "--help") {
          ShowUsage();
          ExitBBSImpl(0, false);
        }
      } break;
      default: {
        LOG(ERROR) << "Invalid Command line argument given '" << argument << "'";
        ExitBBSImpl(errorlevel_, false);
      } break;
      }
    } else {
      // Command line argument did not start with a '-' or a '/'
      LOG(ERROR) << "Invalid Command line argument given '" << argumentRaw << "'";
      ExitBBSImpl(errorlevel_, false);
    }
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
  const string env_str = std::to_string(instance_number());
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

  if (event_only) {
    unique_ptr<WStatus> pStatus(status_manager()->GetStatus());
    cleanup_events();
    if (date() != pStatus->GetLastDate()) {
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
        ResetEffectiveSl();
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
        ResetEffectiveSl();
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
