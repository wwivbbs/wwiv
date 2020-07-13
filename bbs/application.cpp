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
#include "bbs/application.h"
#include "bbs/asv.h"
#include "bbs/bbs.h"
#include "bbs/bbsovl2.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/bbsutl2.h"
#include "bbs/com.h"
#include "bbs/confutil.h"
#include "bbs/exceptions.h"
#include "bbs/instmsg.h"
#include "bbs/lilo.h"
#include "bbs/menu.h"
#include "bbs/netsup.h"
#include "bbs/null_remote_io.h"
#include "bbs/remote_io.h"
#include "bbs/ssh.h"
#include "bbs/syschat.h"
#include "bbs/sysopf.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/wfc.h"
#include "bbs/wqscn.h"
#include "core/command_line.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/strings-ng.h"
#include "core/version.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "local_io/local_io_curses.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "chnedit.h"
#include "diredit.h"
#include "subedit.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "local_io/null_local_io.h" // Used for Linux build.
#include "local_io/wconstants.h"
#include "sdk/chains.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/files/files.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <random>

#if defined(_WIN32)
#include <crtdbg.h>
// Needed for isatty
// ReSharper disable once CppUnusedIncludeDirective
#include <io.h>
#include "bbs/remote_socket_io.h"
#include "local_io/local_io_win32.h"
#else
// ReSharper disable once CppUnusedIncludeDirective
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
    : local_io_(localIO), oklevel_(exitLevelOK), errorlevel_(exitLevelNotOK),
      session_context_(this) {
  ::bout.SetLocalIO(localIO);
  SetCurrentReadMessageArea(-1);
  thisuser_ = std::make_unique<wwiv::sdk::User>();

  tzset();

  memset(&asv, 0, sizeof(asv_rec));
  newuser_colors = {7, 11, 14, 5, 31, 2, 12, 9, 6, 3};
  newuser_bwcolors = {7, 15, 15, 15, 112, 15, 15, 7, 7, 7};
  User::CreateNewUserRecord(user(), 50, 20, 0, 0.1234f, newuser_colors, newuser_bwcolors);

  // Set the home directory
  bbs_dir_ = File::current_directory();
  bbs_dir_string_ = bbs_dir_.string();
  chains = std::make_unique<Chains>();
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
  delete curses_out;
  curses_out = nullptr;
}

wwiv::bbs::SessionContext& Application::context() { return session_context_; }

LocalIO* Application::localIO() const { return local_io_.get(); }

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
    const auto key_fn = FilePath(config_->datadir(), "wwiv.key");
    const File key_file(key_fn);
    const auto system_password = config()->system_password();
    wwiv::bbs::Key key(key_fn.string(), system_password);
    if (!File::Exists(key_fn)) {
      LOG(ERROR) << "Key file doesn't exist. Will try to create it.";
      if (!key.Create()) {
        LOG(ERROR) << "Unable to create or open key file!.  SSH will be disabled!" << endl;
        type = CommunicationType::TELNET;
      }
    }
    if (!key.Open()) {
      LOG(ERROR) << "Unable to open key file!. Did you change your sytem pw?" << endl;
      LOG(ERROR) << "If so, delete " << key_file;
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
  if (!users()->readuser(user(), user_number)) {
    return false;
  }
  last_read_user_number_ = user_number;
  // Update all other session variables that are dependent.
  screenlinest = (using_modem) ? user()->GetScreenLines() : defscreenbottom + 1;
  return true;
}

void Application::reset_effective_sl() {
  effective_sl_ = user()->GetSl(); 
}
void Application::effective_sl(int nSl) { effective_sl_ = nSl; }
int Application::effective_sl() const { return effective_sl_; }
const slrec& Application::effective_slrec() const { return config()->sl(effective_sl_); }


bool Application::WriteCurrentUser(int user_number) {

  if (user_number == 0) {
    LOG(ERROR) << "Trying to call WriteCurrentUser with user_number 0";
  } else if (user_number != last_read_user_number_) {
    LOG(ERROR) << "Trying to call WriteCurrentUser with user_number: " << user_number
               << "; last_read_user_number_: " << last_read_user_number_;
  }
  return users()->writeuser(user(), user_number);
}

void Application::tleft(bool check_for_timeout) {
  const auto nsln = nsl();

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

  const auto temp_sysop = user()->GetSl() != 255 && effective_sl() == 255;
  const auto sysop_available = sysop1();

  const auto cx = localIO()->WhereX();
  const auto cy = localIO()->WhereY();
  const auto ctl = localIO()->GetTopLine();
  const auto cc = bout.curatr();
  bout.curatr(localIO()->GetTopScreenColor());
  localIO()->SetTopLine(0);
  const auto line_number = (chatcall() && (topdata == LocalIO::topdataUser)) ? 5 : 4;

  localIO()->PutsXY(1, line_number, GetCurrentSpeed());
  for (auto i = localIO()->WhereX(); i < 23; i++) {
    localIO()->Putch(static_cast<unsigned char>('\xCD'));
  }

  if (temp_sysop) {
    localIO()->PutsXY(23, line_number, "Temp Sysop");
  }

  if (sysop_available) {
    localIO()->PutsXY(64, line_number, "Available");
  }

  const auto min_left = nsln / SECONDS_PER_MINUTE;
  const auto secs_left = nsln % SECONDS_PER_MINUTE;
  const auto tleft_display = fmt::sprintf("T-%4ldm %2lds", min_left, secs_left);
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
        clear_chatcall();
        UpdateTopScreen();
        break;
      case F5: /* F5 */
        remoteIO()->disconnect();
        Hangup();
        break;
      case SF5: { /* Shift-F5 */
        std::random_device rd;
        std::mt19937 e{rd()};
        std::uniform_int_distribution dist_len{10, 30};
        std::uniform_int_distribution dist{1, 256};
        for (auto i = 0; i < dist_len(e); i++) {
          bout.bputch(static_cast<unsigned char>(dist(e)));
        }
        remoteIO()->disconnect();
        Hangup();
        } break;
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
          toggle_chat_file();
        }
        break;
      }
    }
  }
}

void Application::DisplaySysopWorkingIndicator(bool displayWait) {
  const string waitString = "-=[WAIT]=-";
  auto nNumPrintableChars = waitString.length();
  for (auto iter = waitString.begin(); iter != waitString.end(); ++iter) {
    if (*iter == 3 && nNumPrintableChars > 1) {
      nNumPrintableChars -= 2;
    }
  }

  if (displayWait) {
    if (okansi()) {
      const auto nSavedAttribute = bout.curatr();
      bout.SystemColor(user()->color(3));
      bout << waitString << "\x1b[" << nNumPrintableChars << "D";
      bout.SystemColor(nSavedAttribute);
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
  int i;
  char sl[82], ar[17], dar[17], restrict[17], rst[17];

  auto lll = bout.lines_listed();

  if (so() && !a()->context().incom()) {
    topdata = LocalIO::topdataNone;
  }

#ifdef _WIN32
  if (config()->sysconfig_flags() & sysconfig_titlebar) {
    // Only set the titlebar if the user wanted it that way.
    const string username_num = names()->UserName(usernum);
    string title = fmt::sprintf("WWIV Node %d (User: %s)", instance_number(), username_num);
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
    if (chatcall()) {
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

  auto cx = localIO()->WhereX();
  auto cy = localIO()->WhereY();
  auto nOldTopLine = localIO()->GetTopLine();
  auto cc = bout.curatr();
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
    localIO()->PutsXY(0, 0,
                      fmt::format("{:>50}  Activity for {:>8}:      ", config()->system_name(),
                                  status->GetLastDate()));

    localIO()->PutsXY(
        0, 1,
        fmt::sprintf(
            "Users: %4u       Total Calls: %5lu      Calls Today: %4u    Posted      :%3u ",
            status->GetNumUsers(), status->GetCallerNumber(), status->GetNumCallsToday(),
            status->GetNumLocalPosts()));

    const auto username_num = names()->UserName(usernum);
    localIO()->PutsXY(0, 2,
                      fmt::sprintf("%-36s      %-4u min   /  %2u%%    E-mail sent :%3u ",
                                   username_num, status->GetMinutesActiveToday(),
                                   static_cast<int>(10 * status->GetMinutesActiveToday() / 144),
                                   status->GetNumEmailSentToday()));

    User sysop{};
    auto feedback_waiting = 0;
    if (users()->readuser_nocache(&sysop, 1)) {
      feedback_waiting = sysop.GetNumMailWaiting();
    }
    localIO()->PutsXY(
        0, 3,
        fmt::sprintf(
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
      lo = fmt::sprintf("Today:%2d", user()->GetTimesOnToday());
    }

    const auto username_num = names()->UserName(usernum);
    auto line =
        fmt::sprintf("%-35s W=%3u UL=%4u/%6lu SL=%3u LO=%5u PO=%4u", username_num,
                     user()->GetNumMailWaiting(), user()->GetFilesUploaded(), user()->uk(),
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
                      fmt::sprintf("%-20s %12s  %-6s DL=%4u/%6lu DL=%3u TO=%5.0d ES=%4u",
                                   user()->GetRealName(), user()->GetVoicePhoneNumber(),
                                   callsign_or_regnum, user()->GetFilesDownloaded(),
                                   user()->dk(), user()->GetDsl(), minutes_used.count(),
                                   user()->GetNumEmailSent() + user()->GetNumNetEmailSent()));

    localIO()->PutsXY(0, 2,
                      fmt::sprintf("ARs=%-16s/%-16s R=%-16s EX=%3u %-8s FS=%4u", ar, dar, restrict,
                                   user()->GetExempt(), lo, user()->GetNumFeedbackSent()));

    User sysop{};
    int feedback_waiting = 0;
    if (users()->readuser_nocache(&sysop, 1)) {
      feedback_waiting = sysop.GetNumMailWaiting();
    }
    localIO()->PutsXY(0, 3,
                      fmt::sprintf("%-40.40s %c %2u %-16.16s           FW= %3u",
                                   user()->GetNote(), user()->GetGender(), user()->GetAge(),
                                   ctypes(user()->GetComputerType()), feedback_waiting));

    if (chatcall()) {
      localIO()->PutsXY(0, 4, fmt::format("{:<80}", chat_reason_));
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

// ReSharper disable once CppMemberFunctionMayBeConst
void Application::Cls() {
  localIO()->Cls();
  bout.clear_lines_listed();
}

std::string Application::network_name() const {
  if (nets_->empty()) {
    return {};
  }
  return nets_->at(network_num_).name;
}

std::string Application::network_directory() const {
  if (nets_->empty()) {
    return "";
  }
  return std::string(nets_->at(network_num_).dir);
}

int Application::language_number() const { return m_nCurrentLanguageNumber; }

void Application::set_language_number(int n) {
  m_nCurrentLanguageNumber = n;
  if (n >= 0 && n <= static_cast<int>(languages.size())) {
    cur_lang_name = languages[n].name;
    language_dir = languages[n].dir;
  }
}

bool Application::GetCaller() {
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

  const auto lokb = wfc.doWFCEvents();

  if (lokb) {
    modem_speed_ = 38400;
  }

  using_modem = a()->context().incom() ? 1 : 0;
  if (lokb == 2) {
    using_modem = -1;
  }
  if (lokb == 999) {
    // Magic exit from WWIV
    return false;
  }

  bout.okskey(true);
  Cls();
  localIO()->Puts(StrCat("Logging on at ", GetCurrentSpeed(), " ...\r\n"));
  set_at_wfc(false);

  return true;
}

void Application::GotCaller(int ms) {
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

std::filesystem::path Application::bbspath() const noexcept { return bbs_dir_; }
std::string Application::bbsdir() const noexcept { return bbs_dir_string_; }
std::string Application::bindir() const noexcept { return bindir_; }
std::string Application::configdir() const noexcept { return configdir_; }
std::string Application::logdir() const noexcept { return logdir_; }
int Application::verbose() const noexcept { return verbose_; }

int Application::ExitBBSImpl(int exit_level, bool perform_shutdown) {
  if (perform_shutdown) {
    write_inst(INST_LOC_DOWN, 0, INST_FLAGS_NONE);
    if (exit_level != Application::exitLevelOK && exit_level != Application::exitLevelQuit) {
      // Only log the exiting at abnormal error levels, since we see lots of exiting statements
      // in the logs that don't correspond to sessions every being created (network probers, etc).
      sysoplog(false);
      sysoplog(false) << "WWIV " << short_version() << ", inst " << instance_number()
                      << ", taken down at " << times() << " on " << fulldate() << " with exit code "
                      << exit_level << ".";
      sysoplog(false);
    }
    catsl();
    clog << "\r\n";
    clog << "WWIV Bulletin Board System " << full_version()
         << " exiting at error level " << exit_level << endl
         << endl;
    clog.flush();
  }

  return exit_level;
}

int Application::Run(int argc, char* argv[]) {
  int ui = 0;
  auto ooneuser = false;
  auto type = CommunicationType::NONE;

#ifdef _WIN32
  // Disable padding with 0xFE for all safe string functions.
  // We'd rather leave them padded with 0x0 as default.
  _CrtSetDebugFillThreshold(0);
#endif
  CommandLine cmdline(argc, argv, "");
  cmdline.AddStandardArgs();
  cmdline.set_no_args_allowed(true);
  cmdline.add_argument({"error_exit", 'a', "Specify the Error Exit Level", "1"});
  cmdline.add_argument({"bps", 'b', "Modem speed of logged on user", "115200"});
  cmdline.add_argument({"sysop_cmd", 'c', "Executes a sysop command (b/c/d)", ""});
  cmdline.add_argument(
      BooleanCommandLineArgument{"beginday", 'e', "Load for beginday event only", false});
  cmdline.add_argument({"handle", 'h', "Socket handle", "0"});
  cmdline.add_argument(BooleanCommandLineArgument{"no_modem", 'm',
                                                  "Don't access the modem at all", false});
  cmdline.add_argument({"instance", 'n', "Designate instance number <inst>", "1"});
  cmdline.add_argument({"menu_commands", 'o', "Displays the menu commands available then exits.", ""});
  cmdline.add_argument({"ok_exit", 'q', "Normal exit level", "0"});
  cmdline.add_argument({"remaining_min", 'r', "Specify max # minutes until event", "0"});
  cmdline.add_argument({"user_num", 'u', "Pass usernumber <user#> online", "0"});
  cmdline.add_argument(BooleanCommandLineArgument{"version", 'V', "Display version.", false});
  cmdline.add_argument({"x", 'x', "Someone is logged in with t for telnet or s for ssh.", ""});
  cmdline.add_argument(BooleanCommandLineArgument{"no_hangup", 'z',
                                                  "Do not hang up on user when at log off", false});

  if (!cmdline.Parse()) {
    cout << "WWIV Bulletin Board System [" << full_version() << "]\r\n\n";
    cout << cmdline.GetHelp() << endl;
    return EXIT_FAILURE;
  }
  if (cmdline.help_requested()) {
    cout << "WWIV Bulletin Board System [" << full_version() << "]\r\n\n";
    cout << cmdline.GetHelp() << endl;
    return 0;
  }
  if (cmdline.barg("version")) {
    cout << "WWIV Bulletin Board System [" << full_version() << "]\r\n\n";
    return 0;
  }
  const auto menu_commands = cmdline.arg("menu_commands");
  if (!menu_commands.is_default()) {
    wwiv::menus::PrintMenuCommands(menu_commands.as_string());
    return 0;
  }

  const auto bbs_env = environment_variable("BBS");
  if (!bbs_env.empty()) {
    if (bbs_env.find("WWIV") != string::npos) {
      LOG(ERROR) << "You are already in the BBS, type 'EXIT' instead.\n\n";
      return 255;
    }
  }
  
  bout.curatr(0x07);
  // Set the directories.
  bbs_dir_ = cmdline.bbsdir();
  bbs_dir_string_ = bbs_dir_.string();
  bindir_ = cmdline.bindir();
  logdir_ = cmdline.logdir();
  configdir_ = cmdline.configdir();
  verbose_ = cmdline.verbose();
  File::set_current_directory(bbs_dir_);
  
  oklevel_ = cmdline.iarg("ok_exit");
  errorlevel_ = cmdline.iarg("error_exit");
  const unsigned int hSockOrComm = cmdline.iarg("handle");
  no_hangup_ = cmdline.barg("no_hangup");
  //auto num_min = cmdline.iarg("remaining_min");
  context().ok_modem_stuff(!cmdline.barg("no_modem"));
  instance_number_ = cmdline.iarg("instance");

  auto this_usernum_from_commandline = static_cast<uint16_t>(cmdline.iarg("user_num"));
  const auto x = cmdline.sarg("x");
  if (!x.empty()) {
    const auto xarg = to_upper_case_char(x.at(0));
    if (cmdline.arg("handle").is_default()) {
      clog << "-h must be specified when using '"
           << "-x" << x << "'" << std::endl;
      return errorlevel_;
    }
    if (xarg == 'T' || xarg == 'S') {
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
      return errorlevel_;
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
      CursesIO::Init(fmt::sprintf("WWIV BBS %s", full_version()));
      reset_local_io(new CursesLocalIO(curses_out->GetMaxY()));
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
    return Application::exitLevelNotOK;
  }

  const auto sysop_cmd = cmdline.sarg("sysop_cmd");
  if (!sysop_cmd.empty()) {
    // HACK for now, pass arg into InitializeBBS
    user_already_on_ = true;
  }
  CreateComm(hSockOrComm, type);
  if (!InitializeBBS(!user_already_on_ && sysop_cmd.empty())) {
    return Application::exitLevelNotOK;
  }
  localIO()->UpdateNativeTitleBar(config()->system_name(), instance_number());

  auto remote_opened = true;
  // If we are telnet...
  if (type == CommunicationType::TELNET || type == CommunicationType::SSH) {
    context().ok_modem_stuff(true);
    remote_opened = remoteIO()->open();
  }

  if (!remote_opened) {
    // Remote side disconnected.
    clog << "Remote side disconnected." << std::endl;
    return oklevel_;
  }

  if (cmdline.barg("beginday")) {
    const auto status = status_manager()->GetStatus();
    if (date() != status->GetLastDate()) {
      // This may be another node, but the user explicitly wanted to run the beginday
      // event from the commandline, so we'll just check the date.
      beginday(true);
    } else {
      sysoplog(false) << "!!! Wanted to run the beginday event when it's not required!!!";
      clog << "! WARNING: Tried to run beginday event again\r\n\n";
      sleep_for(seconds(2));
    }
    return oklevel_;
  }
  if (!sysop_cmd.empty()) {
    LOG(INFO) << "Executing Sysop Command: " << sysop_cmd;
    remoteIO()->remote_info().clear();
    frequent_init();
    ReadCurrentUser(1);
    reset_effective_sl();

    usernum = 0;
    // Since hang_it_up sets hangup_ = true, let's ensure we're always
    // not in this state when we enter the WFC.
    hangup_ = false;
    set_at_wfc(true);
    const auto cmd = static_cast<char>(std::toupper(sysop_cmd.front()));
    switch (cmd) {
    case 'B':
      boardedit();
      break;
    case 'C':
      chainedit();
      break;
    case 'D':
      dlboardedit();
      break;
    default:
      LOG(ERROR) << "Unknown Sysop Command: '" << sysop_cmd;
      return errorlevel_;
    }
    return oklevel_;
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
          if (!GetCaller()) {
            // GetCaller returning false means time to exit.
            return Application::exitLevelOK;
          }
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
      if (!batch().entry.empty() && batch().ssize() != batch().numbatchdl()) {
        for (const auto& b : batch().entry) {
          if (!b.sending()) {
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

/** Returns the WWIV SDK Config Object. */
Config* Application::config() const { return config_.get(); }
void Application::set_config_for_test(std::unique_ptr<wwiv::sdk::Config> config) {
  config_ = std::move(config);
}

/** Returns the WWIV Names.LST Config Object. */
Names* Application::names() const { return names_.get(); }

msgapi::MessageApi* Application::msgapi(int type) const {
  return msgapis_.at(type).get();
}

msgapi::MessageApi* Application::msgapi() const {
  return msgapis_.at(current_sub().storage_type).get();
}

msgapi::WWIVMessageApi* Application::msgapi_email() const {
  return dynamic_cast<wwiv::sdk::msgapi::WWIVMessageApi*>(msgapi(2));
}

files::FileApi* Application::fileapi() const {
  return fileapi_.get();
}
files::FileArea* Application::current_file_area() const {
  return file_area_.get();
}

void Application::set_current_file_area(std::unique_ptr<wwiv::sdk::files::FileArea> a) {
  file_area_ = std::move(a);
}



Batch& Application::batch() { return batch_; }

const subboard_t& Application::current_sub() const {
  return subs().sub(GetCurrentReadMessageArea());
}

const files::directory_t& Application::current_dir() const {
 return dirs()[current_user_dir().subnum];
}

Subs& Application::subs() { return *subs_; }

const Subs& Application::subs() const { return *subs_; }

files::Dirs& Application::dirs() {
  return *dirs_;
}

const files::Dirs& Application::dirs() const {
  return *dirs_;
}

Networks& Application::nets() {
  return *nets_;
}

const Networks& Application::nets() const {
  return *nets_;
}

const net_networks_rec& Application::current_net() const {
  const static net_networks_rec empty_rec{};
  if (nets_->empty()) {
    return empty_rec;
  }
  return nets_->at(net_num());
}
