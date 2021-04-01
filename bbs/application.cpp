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
#include "bbs/application.h"

#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/bbsovl2.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/bbsutl2.h"
#include "bbs/confutil.h"
#include "bbs/diredit.h"
#include "bbs/execexternal.h"
#include "bbs/instmsg.h"
#include "bbs/interpret.h"
#include "bbs/lilo.h"
#include "bbs/netsup.h"
#include "bbs/shortmsg.h"
#include "bbs/ssh.h"
#include "bbs/stuffin.h"
#include "bbs/subedit.h"
#include "bbs/syschat.h"
#include "bbs/sysopf.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/tag.h"
#include "bbs/wfc.h"
#include "bbs/wqscn.h"
#include "bbs/basic/basic.h"
#include "bbs/menus/mainmenu.h"
#include "bbs/menus/printcommands.h"
#include "common/datetime.h"
#include "common/exceptions.h"
#include "common/input.h"
#include "common/null_remote_io.h"
#include "common/output.h"
#include "common/pipe_expr.h"
#include "common/remote_io.h"
#include "common/workspace.h"
#include "core/command_line.h"
#include "core/eventbus.h"
#include "core/os.h"
#include "core/strings-ng.h"
#include "core/strings.h"
#include "core/version.h"
#include "fmt/printf.h"
#include "fsed/fsed.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"
// ReSharper disable once CppUnusedIncludeDirective
// ReSharper disable once CppUnusedIncludeDirective
#include "bbs/chnedit.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "bbs/uedit.h"
#include "local_io/wconstants.h"
#include "sdk/chains.h"
#include "sdk/gfiles.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/names.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "localui/curses_io.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/files/files.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/net/networks.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#include <crtdbg.h>
// Needed for isatty
// ReSharper disable once CppUnusedIncludeDirective
#include "common/remote_socket_io.h"
#include "local_io/local_io_win32.h"
#include <io.h>
#else
// ReSharper disable once CppUnusedIncludeDirective
#include "local_io/local_io_curses.h"
#include "local_io/null_local_io.h" // Used for Linux build.
#include <unistd.h>
#endif // _WIN32

using namespace std::chrono;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

// Implementation of Context for the Application
class ApplicationContext final : public Context {
public:
  explicit ApplicationContext(Application* app) : app_(app) {}
  ApplicationContext() = delete;
  ApplicationContext(const ApplicationContext&) = delete;
  ApplicationContext(ApplicationContext&&) = delete;
  ApplicationContext& operator=(const ApplicationContext&) = delete;
  ApplicationContext& operator = (ApplicationContext &&) = delete;
  ~ApplicationContext() override = default;
  [[nodiscard]] Config& config() override { return *app_->config(); }
  [[nodiscard]] User& u() override { return *app_->user(); }
  [[nodiscard]] SessionContext& session_context() override { return app_->sess(); }
  [[nodiscard]] bool mci_enabled() const override { return bout.mci_enabled(); }
  [[nodiscard]] const std::vector<editorrec>& editors() const override {
    return app_->editors;
  }
  [[nodiscard]] const Chains& chains() const override {
    return *app_->chains;
  }

private:
  Application* app_;
};


Application::Application(LocalIO* localIO)
    : local_io_(localIO), oklevel_(exitLevelOK), errorlevel_(exitLevelNotOK),
      session_context_(std::make_unique<SessionContext>(localIO)),
      context_(std::make_unique<ApplicationContext>(this)),
      pipe_eval_(std::make_unique<PipeEval>(*context_)),
      bbs_macro_context_(std::make_unique<BbsMacroContext>(context_.get(), *pipe_eval_)),
      batch_(std::make_unique<Batch>()) {
  ::bout.SetLocalIO(localIO);
  bout.set_context_provider([this]() -> Context& { return *this->context_; });
  bout.set_macro_context_provider(
      [this]() -> MacroContext& { return *bbs_macro_context_; });

  ::bin.SetLocalIO(localIO);
  bin.set_context_provider([this]() -> Context& { return *this->context_; });

  sess().SetCurrentReadMessageArea(-1);
  thisuser_ = std::make_unique<User>();

  tzset();

  memset(&asv, 0, sizeof(asv_rec));
  newuser_colors = {7, 11, 14, 5, 31, 2, 12, 9, 6, 3};
  newuser_bwcolors = {7, 15, 15, 15, 112, 15, 15, 7, 7, 7};
  User::CreateNewUserRecord(user(), 50, 20, 0, 0.1234f, newuser_colors, newuser_bwcolors);

  // Set the home directory
  bbs_dir_ = File::current_directory();
  chains = std::make_unique<Chains>();
}

Application::~Application() {
  if (comm_ && sess().ok_modem_stuff()) {
    comm_->close(false);
    comm_.reset(nullptr);
  }
  if (local_io_) {
    local_io_->SetCursor(wwiv::local::io::LocalIO::cursorNormal);
  }
  // CursesIO.
  delete wwiv::local::ui::curses_out;
  wwiv::local::ui::curses_out = nullptr;
}

SessionContext& Application::sess() { return *session_context_; }
const SessionContext& Application::sess() const { return *session_context_; }

void Application::set_user_for_test(User& u) { *thisuser_ = u; }

Context& Application::context() {
  CHECK(context_);
  return *context_;
}

const Context& Application::context() const {
  CHECK(context_);
  return *context_;
}

//LocalIO* Application::localIO() const { return local_io_.get(); }

bool Application::reset_local_io(LocalIO* wlocal_io) {
  local_io_.reset(wlocal_io);
  ::bout.SetLocalIO(wlocal_io);
  ::bin.SetLocalIO(wlocal_io);
  sess().reset_local_io(wlocal_io);

  const auto screen_bottom = bout.localIO()->GetDefaultScreenBottom();
  bout.localIO()->SetScreenBottom(screen_bottom);
  sess().num_screen_lines(screen_bottom + 1);

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
        LOG(ERROR) << "Unable to create or open key file!.  SSH will be disabled!" << std::endl;
        comm_ = std::make_unique<RemoteSocketIO>(nHandle, true);
        break;
      }
    }
    if (!key.Open()) {
      LOG(ERROR) << "Unable to open key file!. Did you change your sytem pw?";
      LOG(ERROR) << "If so, delete " << key_file;
      LOG(ERROR) << "SSH will be disabled!";
      comm_ = std::make_unique<RemoteSocketIO>(nHandle, true);
      break;
    }
    comm_ = std::make_unique<wwiv::bbs::IOSSH>(nHandle, key);
  } break;
  case CommunicationType::TELNET: {
    comm_ = std::make_unique<RemoteSocketIO>(nHandle, true);
  } break;
  case CommunicationType::NONE: {
    comm_ = std::make_unique<NullRemoteIO>();
  } break;
  case CommunicationType::STDIO:
    comm_ = std::make_unique<NullRemoteIO>();
    break;
  }
  bout.SetComm(comm_.get());
  bin.SetComm(comm_.get());
}

void Application::SetCommForTest(RemoteIO* remote_io) {
  comm_.reset(remote_io);
  bout.SetComm(remote_io);
  bin.SetComm(remote_io);
}

bool Application::ReadCurrentUser() { 
  DCHECK_GE(sess().user_num(), 1);
  return ReadCurrentUser(sess().user_num());
}

bool Application::ReadCurrentUser(int user_number) {
  DCHECK_GE(user_number, 1);
  if (!users()->readuser(user(), user_number)) {
    return false;
  }
  last_read_user_number_ = user_number;
  // Update all other session variables that are dependent.
  sess().current_menu_set(user()->menu_set());
  sess().num_screen_lines(sess().using_modem() ? user()->screen_lines()
                                          : bout.localIO()->GetDefaultScreenBottom() + 1);
  sess().dirs().current_menu_directory(FilePath(config_->menudir(), user()->menu_set()));
  return true;
}

void Application::reset_effective_sl() {
  sess().effective_sl(user()->sl());
}

bool Application::WriteCurrentUser() {
  DCHECK_GE(sess().user_num(), 1);
  return WriteCurrentUser(sess().user_num()); 
}

bool Application::WriteCurrentUser(int user_number) {
  DCHECK_GE(sess().user_num(), 1) << "Trying to call WriteCurrentUser with user_number 0";

  if (user_number != last_read_user_number_) {
    LOG(ERROR) << "Trying to call WriteCurrentUser with user_number: " << user_number
               << "; last_read_user_number_: " << last_read_user_number_;
  }
  // Update any possibly changed session variables.
  sess().current_menu_set(user()->menu_set());
  sess().dirs().current_menu_directory(FilePath(config_->menudir(), user()->menu_set()));
  return users()->writeuser(user(), user_number);
}

void Application::tleft(bool check_for_timeout) {
  const auto nsln = nsl();

  // Check for timeout 1st.
  if (check_for_timeout && sess().IsUserOnline()) {
    if (nsln == 0) {
      bout << "\r\nTime expired.\r\n\n";
      a()->Hangup();
    }
    return;
  }

  // If we're not displaying the time left or password on the
  // topdata display, leave now.
  if (bout.localIO()->topdata() == LocalIO::topdata_t::none) {
    return;
  }

  const auto temp_sysop = user()->sl() != 255 && sess().effective_sl() == 255;
  const auto sysop_available = wwiv::common::sysop1();

  const auto cx = bout.localIO()->WhereX();
  const auto cy = bout.localIO()->WhereY();
  const auto ctl = bout.localIO()->GetTopLine();
  const auto cc = bout.curatr();
  bout.curatr(bout.localIO()->GetTopScreenColor());
  bout.localIO()->SetTopLine(0);
  const auto line_number =
      (sess().chatcall() && (bout.localIO()->topdata() == LocalIO::topdata_t::user)) ? 5 : 4;

  bout.localIO()->PutsXY(1, line_number, GetCurrentSpeed());
  for (auto i = bout.localIO()->WhereX(); i < 23; i++) {
    bout.localIO()->Putch(static_cast<unsigned char>('\xCD'));
  }

  if (temp_sysop) {
    bout.localIO()->PutsXY(23, line_number, "Temp Sysop");
  }

  if (sysop_available) {
    bout.localIO()->PutsXY(64, line_number, "Available");
  }

  const auto min_left = nsln / SECONDS_PER_MINUTE;
  const auto secs_left = nsln % SECONDS_PER_MINUTE;
  const auto tleft_display = fmt::sprintf("T-%4ldm %2lds", min_left, secs_left);
  switch (bout.localIO()->topdata()) {
  case LocalIO::topdata_t::system: {
    if (sess().IsUserOnline()) {
      bout.localIO()->PutsXY(18, 3, tleft_display);
    }
  } break;
  case LocalIO::topdata_t::user: {
    if (sess().IsUserOnline()) {
      bout.localIO()->PutsXY(18, 3, tleft_display);
    } else {
      bout.localIO()->PutsXY(18, 3, user()->password());
    }
  } break;
  case LocalIO::topdata_t::none:
    break;
  }
  bout.localIO()->SetTopLine(ctl);
  bout.curatr(cc);
  bout.localIO()->GotoXY(cx, cy);
}

void Application::handle_sysop_key(uint8_t key) {
  if (bin.okskey()) {
    if (key >= AF1 && key <= AF10) {
      // N is the auto-val number (1-10). Add one to make it 1 based not 0 based.
      const auto n = key - AF1 + 1;
      auto_val(n, user());
      reset_effective_sl();
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
        bout.localIO()->increment_topdata();
        UpdateTopScreen();
        break;
      case F3: /* F3 */
        if (sess().using_modem()) {
          sess().incom(!sess().incom());
          bout.dump();
          tleft(false);
        }
        break;
      case F4: /* F4 */
        sess().clear_chatcall();
        UpdateTopScreen();
        break;
      case F5: /* F5 */
        bout.remoteIO()->disconnect();
        a()->Hangup(hangup_type_t::sysop_forced);
        break;
      case SF5: { /* Shift-F5 */
        std::random_device rd;
        std::mt19937 e{rd()};
        std::uniform_int_distribution dist_len{10, 30};
        std::uniform_int_distribution dist{1, 256};
        for (auto i = 0; i < dist_len(e); i++) {
          bout.bputch(static_cast<char>(dist(e) & 0xff));
        }
        bout.remoteIO()->disconnect();
        a()->Hangup(hangup_type_t::sysop_forced);
        } break;
      case CF5: /* Ctrl-F5 */
        bout << "\r\nCall back later when you are there.\r\n\n";
        bout.remoteIO()->disconnect();
        a()->Hangup(hangup_type_t::sysop_forced);
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
        if (user()->sl() != 255) {
          if (sess().effective_sl() != 255) {
            sess().effective_sl(255);
          } else {
            reset_effective_sl();
          }
          changedsl();
          tleft(false);
        }
        break;
      case F10: /* F10 */
        if (sess().chatting() == wwiv::common::chatting_t::none) {
          if (config()->sysconfig_flags() & sysconfig_2_way) {
            chat1("", true);
          } else {
            chat1("", false);
          }
        } else {
          sess().chatting(wwiv::common::chatting_t::none);
        }
        break;
      case CF10: /* Ctrl-F10 */
        if (sess().chatting() == wwiv::common::chatting_t::none) {
          chat1("", false);
        } else {
          sess().chatting(chatting_t::none);
        }
        break;
      case HOME: /* HOME */
        if (sess().chatting() == chatting_t::one_way) {
          toggle_chat_file();
        }
        break;
      default: ;
      }
    }
  }
}

void Application::DisplaySysopWorkingIndicator(bool displayWait) {
  const std::string waitString = "-=[WAIT]=-";
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

  auto status = (status_manager()->get_status());
  int i;
  char sl[82], ar[17], dar[17], restrict[17], rst[17];

  auto lll = bout.lines_listed();

  if (so() && !sess().incom()) {
    bout.localIO()->topdata(LocalIO::topdata_t::none);
  }

#ifdef _WIN32
  if (config()->sysconfig_flags() & sysconfig_titlebar) {
    // Only set the titlebar if the user wanted it that way.
    const auto username_num = user()->name_and_number();
    const auto title = fmt::sprintf("WWIV Node %d (User: %s)", sess().instance_number(), username_num);
    ::SetConsoleTitle(title.c_str());
  }
#endif // _WIN32

  switch (bout.localIO()->topdata()) {
  case LocalIO::topdata_t::none:
    bout.localIO()->set_protect(0);
    break;
  case LocalIO::topdata_t::system:
    bout.localIO()->set_protect(5);
    break;
  case LocalIO::topdata_t::user:
    if (sess().chatcall()) {
      bout.localIO()->set_protect(6);
    } else {
      if (bout.localIO()->GetTopLine() == 6) {
        bout.localIO()->set_protect(0);
      }
      bout.localIO()->set_protect(5);
    }
    break;
  }
  // This used to be inside of localIO::set_protect but that
  // made absolutely no sense, so pulled it out here
  if (!sess().using_modem()) {
    sess().num_screen_lines(bout.localIO()->GetDefaultScreenBottom() + 1 - bout.localIO()->GetTopLine());
  }

  auto cx = bout.localIO()->WhereX();
  auto cy = bout.localIO()->WhereY();
  auto nOldTopLine = bout.localIO()->GetTopLine();
  auto cc = bout.curatr();
  bout.curatr(bout.localIO()->GetTopScreenColor());
  bout.localIO()->SetTopLine(0);
  for (i = 0; i < 80; i++) {
    sl[i] = '\xCD';
  }
  sl[80] = '\0';

  switch (bout.localIO()->topdata()) {
  case LocalIO::topdata_t::none:
    break;
  case LocalIO::topdata_t::system: {
    bout.localIO()->PutsXY(0, 0,
                      fmt::format("{:>50}  Activity for {:>8}:      ", config()->system_name(),
                                  status->last_date()));

    bout.localIO()->PutsXY(
        0, 1,
        fmt::sprintf(
            "Users: %4u       Total Calls: %5lu      Calls Today: %4u    Posted      :%3u ",
            status->num_users(), status->caller_num(), status->calls_today(),
            status->localposts()));

    const auto username_num = user()->name_and_number();
    bout.localIO()->PutsXY(0, 2,
                      fmt::sprintf("%-36s      %-4u min   /  %2u%%    E-mail sent :%3u ",
                                   username_num, status->active_today_minutes(),
                                   static_cast<int>(10 * status->active_today_minutes() / 144),
                                   status->email_today()));

    auto feedback_waiting = 0;
    if (const auto sysop = users()->readuser(1)) {
      feedback_waiting = sysop->email_waiting();
    }
    bout.localIO()->PutsXY(
        0, 3,
        fmt::sprintf(
            "SL=%3u   DL=%3u               FW=%3u      Uploaded:%2u files    Feedback    :%3u ",
            user()->sl(), user()->dsl(), feedback_waiting, status->uploads_today(),
            status->feedback_today()));
  } break;
  case LocalIO::topdata_t::user: {
    to_char_array(rst, restrict_string);
    for (i = 0; i <= 15; i++) {
      if (user()->has_ar(1 << i)) {
        ar[i] = static_cast<char>('A' + i);
      } else {
        ar[i] = SPACE;
      }
      if (user()->has_dar(1 << i)) {
        dar[i] = static_cast<char>('A' + i);
      } else {
        dar[i] = SPACE;
      }
      if (user()->has_restrict(1 << i)) {
        restrict[i] = rst[i];
      } else {
        restrict[i] = SPACE;
      }
    }
    dar[16] = '\0';
    ar[16] = '\0';
    restrict[16] = '\0';
    std::string lo;
    if (date() != user()->laston()) {
      lo = user()->laston();
    } else {
      lo = fmt::sprintf("Today:%2d", user()->ontoday());
    }

    const auto username_num = user()->name_and_number();
    auto line =
        fmt::sprintf("%-35s W=%3u UL=%4u/%6lu SL=%3u LO=%5u PO=%4u", username_num,
                     user()->email_waiting(), user()->uploaded(), user()->uk(),
                     user()->sl(), user()->logons(), user()->messages_posted());
    bout.localIO()->PutsXYA(0, 0, bout.curatr(), line);

    std::string callsign_or_regnum = user()->callsign();
    if (user()->wwiv_regnum()) {
      callsign_or_regnum = std::to_string(user()->wwiv_regnum());
    }
    auto used_this_session = (system_clock::now() - sess().system_logon_time());
    auto used_total = used_this_session + user()->timeon();
    auto minutes_used = duration_cast<minutes>(used_total);

    bout.localIO()->PutsXY(0, 1,
                      fmt::sprintf("%-20s %12s  %-6s DL=%4u/%6lu DL=%3u TO=%5.0d ES=%4u",
                                   user()->real_name(), user()->voice_phone(),
                                   callsign_or_regnum, user()->downloaded(),
                                   user()->dk(), user()->dsl(), minutes_used.count(),
                                   user()->email_sent() + user()->email_net()));

    bout.localIO()->PutsXY(0, 2,
                      fmt::sprintf("ARs=%-16s/%-16s R=%-16s EX=%3u %-8s FS=%4u", ar, dar, restrict,
                                   user()->exempt(), lo, user()->feedback_sent()));

    int feedback_waiting = 0;
    if (const auto sysop = users()->readuser(1)) {
      feedback_waiting = sysop->email_waiting();
    }
    bout.localIO()->PutsXY(0, 3,
                      fmt::sprintf("%-40.40s %c %2u %-16.16s           FW= %3u",
                                   user()->note(), user()->gender(), user()->age(),
                                   ctypes(user()->computer_type()), feedback_waiting));

    if (sess() .chatcall()) {
      bout.localIO()->PutsXY(0, 4, fmt::format("{:<80}", sess().chat_reason()));
    }
  } break;
  }
  if (nOldTopLine != 0) {
    bout.localIO()->PutsXY(0, nOldTopLine - 1, sl);
  }
  bout.localIO()->SetTopLine(nOldTopLine);
  bout.localIO()->GotoXY(cx, cy);
  bout.curatr(cc);
  tleft(false);

  bout.lines_listed(lll);
}

void Application::ClearTopScreenProtection() {
  bout.localIO()->set_protect(0);
  if (!sess().using_modem()) {
    sess().num_screen_lines(bout.localIO()->GetDefaultScreenBottom() + 1 - bout.localIO()->GetTopLine());
  }
}

// ReSharper disable once CppMemberFunctionMayBeConst
void Application::Cls() {
  bout.localIO()->Cls();
  bout.clear_lines_listed();
}

std::string Application::GetCurrentSpeed() const { return current_speed_; }

std::string Application::network_name() const {
  if (nets_->empty() || network_num_ >= wwiv::stl::ssize(*nets_)) {
    return {};
  }
  return nets_->at(network_num_).name;
}

std::string Application::network_directory() const {
  if (nets_->empty() || network_num_ >= wwiv::stl::ssize(*nets_)) {
    return {};
  }
  return nets_->at(network_num_).dir.string();
}

get_caller_t Application::GetCaller() {
  wwiv::bbs::WFC wfc(this);
  const auto [lokb, unx] = wfc.doWFCEvents();

  if (lokb == wwiv::bbs::wfc_events_t::exit) {
    // Magic exit from WWIV
    return get_caller_t::exit;
  }

  sess().using_modem(sess().incom());
  modem_speed_ = 38400;
  bin.okskey(true);
  Cls();
  bout.localIO()->Puts(StrCat("Logging on at ", GetCurrentSpeed(), " ...\r\n"));

  return lokb == wwiv::bbs::wfc_events_t::login_fast ? get_caller_t::fast_login
                                                     : get_caller_t::normal_login;
}

void Application::GotCaller(int ms) {
  bout.localIO()->SetTopLine(0);
  frequent_init();
  wfc_cls(a());
  modem_speed_ = ms;
  ReadCurrentUser(1);
  read_qscn(1, sess().qsc, false);
  reset_effective_sl();
  sess().user_num(1);
  if (user()->deleted()) {
    user()->screen_width(80);
    user()->screen_lines(25);
  }
  Cls();
  bout.localIO()->Puts(StrCat("Logging on at ", GetCurrentSpeed(), " ...\r\n"));
  const auto remote_conneted = modem_speed_ != 0;
  sess().incom(remote_conneted);
  sess().outcom(remote_conneted);
  sess().using_modem(remote_conneted);
}

std::filesystem::path Application::bbspath() const noexcept { return bbs_dir_; }
std::string Application::bindir() const noexcept { return bindir_; }
std::string Application::configdir() const noexcept { return configdir_; }
std::string Application::logdir() const noexcept { return logdir_; }
int Application::verbose() const noexcept { return verbose_; }

int Application::ExitBBSImpl(int exit_level, bool perform_shutdown) {
  // Only perform shutdown when asked, and we've loaded config.json
  if (perform_shutdown && a()->config()) {
    write_inst(INST_LOC_DOWN, 0, INST_FLAGS_NONE);
    if (exit_level != Application::exitLevelOK && exit_level != Application::exitLevelQuit) {
      // Only log the exiting at abnormal error levels, since we see lots of exiting statements
      // in the logs that don't correspond to sessions every being created (network probes, etc).
      sysoplog(false) << "";
      sysoplog(false) << "WWIV " << short_version() << ", inst " << sess().instance_number()
                      << ", taken down at " << times() << " on " << fulldate() << " with exit code "
                      << exit_level << ".";
      sysoplog(false) << "";
    }
    catsl();
    std::clog << "\r\n";
    std::clog << "WWIV Bulletin Board System " << full_version()
         << " exiting at error level " << exit_level << std::endl
         << std::endl;
    std::clog.flush();
  }

  return exit_level;
}

int Application::Run(int argc, char* argv[]) {
  auto bps = 0;
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
  cmdline.add_argument({"fsed", 'f', "Opens file in the FSED", ""});
  cmdline.add_argument({"bps", 'b', "Modem speed of logged on user", "38400"});
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
  cmdline.add_argument({"run_basic", 's', "Executes a WWIVbasic script", ""});
  cmdline.add_argument({"user_num", 'u', "Pass usernumber <user#> online", "0"});
  cmdline.add_argument(BooleanCommandLineArgument{"version", 'V', "Display version.", false});
  cmdline.add_argument({"x", 'x', "Someone is logged in with t for telnet or s for ssh.", ""});
  cmdline.add_argument(BooleanCommandLineArgument{"no_hangup", 'z',
                                                  "Do not hang up on user when at log off", false});

  if (!cmdline.Parse()) {
    std::cout << "WWIV Bulletin Board System [" << full_version() << "]\r\n\n";
    std::cout << cmdline.GetHelp() << std::endl;
    return EXIT_FAILURE;
  }
  if (cmdline.help_requested()) {
    std::cout << "WWIV Bulletin Board System [" << full_version() << "]\r\n\n";
    std::cout << cmdline.GetHelp() << std::endl;
    return 0;
  }
  if (cmdline.barg("version")) {
    std::cout << "WWIV Bulletin Board System [" << full_version() << "]\r\n\n";
    return 0;
  }

  if (const auto bbs_env = environment_variable("BBS"); !bbs_env.empty()) {
    if (bbs_env.find("WWIV") != std::string::npos) {
      LOG(ERROR) << "You are already in the BBS, type 'EXIT' instead.\n\n";
      return 255;
    }
  }
  
  bout.curatr(0x07);
  // Set the directories.
  bbs_dir_ = cmdline.bbsdir();
  bindir_ = cmdline.bindir();
  logdir_ = cmdline.logdir();
  configdir_ = cmdline.configdir();
  verbose_ = cmdline.verbose();
  File::set_current_directory(bbs_dir_);
  
  oklevel_ = cmdline.iarg("ok_exit");
  errorlevel_ = cmdline.iarg("error_exit");
  const unsigned int hSockOrComm = cmdline.iarg("handle");
  no_hangup_ = cmdline.barg("no_hangup");
  sess().ok_modem_stuff(!cmdline.barg("no_modem"));
  sess().instance_number(cmdline.iarg("instance"));

  auto this_usernum_from_commandline = static_cast<uint16_t>(cmdline.iarg("user_num"));
  if (const auto x = cmdline.sarg("x"); !x.empty()) {
    const auto xarg = to_upper_case_char(x.at(0));
    if (cmdline.arg("handle").is_default()) {
      std::clog << "-h must be specified when using '"
           << "-x" << x << "'" << std::endl;
      return errorlevel_;
    }
    if (xarg == 'T' || xarg == 'S') {
      // Setting a max of 57600 for the BPS value, by default we use 38400
      // as the default value.
      bps = std::min<int>(cmdline.iarg("bps"), 57600);
      if (xarg == 'S') {
        SetCurrentSpeed("SSH");
      } else {
        SetCurrentSpeed("TELNET");
      }
      // Set it false until we call LiLo
      user_already_on_ = true;
      ooneuser = true;
      sess().using_modem(false);
      sess().incom(true);
      sess().outcom(false);
      type = (xarg == 'S') ? CommunicationType::SSH : CommunicationType::TELNET;
    } else {
      std::clog << "Invalid Command line argument given '" << "-x" << x << "'" << std::endl;
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
      wwiv::local::ui::CursesIO::Init(fmt::sprintf("WWIV BBS %s", full_version()));
      reset_local_io(new CursesLocalIO(wwiv::local::ui::curses_out->GetMaxY()));
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
  const auto env_str = std::to_string(sess().instance_number());
  set_environment_variable("WWIV_INSTANCE", env_str);
  if (!ReadConfig()) {
    // Gotta read the config before we can create the socket handles.
    // Since we may need the SSH key.
    return Application::exitLevelNotOK;
  }

  const auto sysop_cmd = cmdline.sarg("sysop_cmd");
  const auto fsed = cmdline.sarg("fsed");
  const auto run_basic = cmdline.sarg("run_basic");
  const auto mini_cmd = !sysop_cmd.empty() || !fsed.empty() || !run_basic.empty();
  if (mini_cmd) {
    // HACK for now, pass arg into InitializeBBS
    user_already_on_ = true;
  }
  CreateComm(hSockOrComm, type);
  if (!InitializeBBS(!user_already_on_ && sysop_cmd.empty() && fsed.empty() && run_basic.empty())) {
    return exitLevelNotOK;
  }
  bout.localIO()->UpdateNativeTitleBar(config()->system_name(), sess().instance_number());

  auto remote_opened = true;
  // If we are telnet...
  if (type == CommunicationType::TELNET || type == CommunicationType::SSH) {
    sess().ok_modem_stuff(true);
    remote_opened = bout.remoteIO()->open();
  }

  if (!remote_opened) {
    // Remote side disconnected.
    std::clog << "Remote side disconnected." << std::endl;
    return oklevel_;
  }

  if (cmdline.barg("beginday")) {
    if (const auto status = status_manager()->get_status(); date() != status->last_date()) {
      // This may be another node, but the user explicitly wanted to run the begin-day
      // event from the commandline, so we'll just check the date.
      beginday(true);
    } else {
      sysoplog(false) << "!!! Wanted to run the beginday event when it's not required!!!";
      std::clog << "! WARNING: Tried to run beginday event again\r\n\n";
      sleep_for(seconds(2));
    }
    return oklevel_;
  }
  if (mini_cmd) {
    bout.remoteIO()->remote_info().clear();
    frequent_init();
    ReadCurrentUser(1);
    reset_effective_sl();
    // not sure if this is right
    sess().user_num(0);
    // Since hang_it_up sets hangup_ = true, let's ensure we're always
    // not in this state when we enter the WFC.
    sess().hangup(false);
  }

  if (const auto menu_commands = cmdline.arg("menu_commands"); !menu_commands.is_default()) {
    wwiv::bbs::menus::PrintMenuCommands(menu_commands.as_string());
    return 0;
  }

  if (!sysop_cmd.empty()) {
    LOG(INFO) << "Executing Sysop Command: " << sysop_cmd;

    set_at_wfc(true);
    switch (static_cast<char>(std::toupper(sysop_cmd.front()))) {
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
  // Runs the FSED on a file
  if (!fsed.empty()) {
    LOG(INFO) << "Executing FSED on : " << fsed;

    wwiv::fsed::fsed(a()->context(), fsed);
    return oklevel_;
  }
  if (!run_basic.empty()) {
    const auto eff_user = std::max<int>(this_usernum_from_commandline, 1);
    ReadCurrentUser(eff_user);
    reset_effective_sl();
    sess().user_num(eff_user);
    if (!wwiv::bbs::basic::RunBasicScript(run_basic)) {
      LOG(ERROR) << "Error running WWIVbasic script: '" << run_basic;
      return errorlevel_;
    }
    return oklevel_;
  }

#if 0
  localIO()->Puts("Debug: Press any key...");
  localIO()->GetChar();
#endif // 0
  do {
    if (this_usernum_from_commandline) {
      sess().user_num(this_usernum_from_commandline);
      ReadCurrentUser();
      if (!user()->deleted()) {
        GotCaller(bps);
        sess().user_num(this_usernum_from_commandline);
        ReadCurrentUser();
        read_qscn(sess().user_num(), sess().qsc, false);
        reset_effective_sl();
        changedsl();
        sess().okmacro(true);
      } else {
        this_usernum_from_commandline = 0;
      }
    }
    try {
      // Try setting this at the top of the try loop. It's currently only
      // set in logon() which could cause problems if we get hung up before then.
      sess().SetLogonTime();
      
      auto gt = get_caller_t::normal_login;
      if (!this_usernum_from_commandline) {
        if (user_already_on_) {
          GotCaller(bps);
        } else {
          gt = GetCaller();
          if (gt == get_caller_t::exit) {
            // GetCaller returning false means time to exit.
            return Application::exitLevelOK;
          }
        }
      }

      if (gt == get_caller_t::normal_login) {
        if (!this_usernum_from_commandline) {
          getuser();
        }
      } else {
        sess().using_modem(false);
        sess().okmacro(true);
        reset_effective_sl();
        changedsl();
      }
      this_usernum_from_commandline = 0;
      CheckForHangup();
      // Set new timeout
      logon();
      setiia(seconds(5));
      set_net_num(0);
      while (!sess().hangup() && sess().user_num() > 0) {
        CheckForHangup();
        filelist.clear();
        write_inst(INST_LOC_MAIN, current_user_sub().subnum, INST_FLAGS_NONE);
        try {
          wwiv::bbs::menus::mainmenu();
        } catch (const std::logic_error& le) {
          std::cerr << "Caught std::logic_error: " << le.what() << "\r\n";
          sysoplog() << le.what();
        }
      }
    } catch (const hangup_error& h) {
      if (sess().IsUserOnline() && h.hangup_type() == hangup_type_t::user_disconnected) {
        // Don't need to log this unless the user actually made it online.
        std::cerr << h.what() << "\r\n";
        sysoplog() << h.what();
      }
    }
    logoff();
    {
      // post_logoff_cleanup
      remove_from_temp("*.*", sess().dirs().temp_directory(), false);
      remove_from_temp("*.*", sess().dirs().batch_directory(), false);
      remove_from_temp("*.*", sess().dirs().qwk_directory(), false);
      remove_from_temp("*.*", sess().dirs().scratch_directory(), false);
      if (!batch().entry.empty() && batch().ssize() != batch().numbatchdl()) {
        for (const auto& b : batch().entry) {
          if (!b.sending()) {
            didnt_upload(b);
          }
        }
      }
      batch().clear();
    }
    if (!no_hangup_ && sess().using_modem() && sess().ok_modem_stuff()) {
      hang_it_up();
    }
    catsl();
    frequent_init();
    wfc_cls(a());
    cleanup_net();

    if (!no_hangup_ && sess().ok_modem_stuff()) {
      bout.remoteIO()->disconnect();
    }
    user_already_on_ = false;
  } while (!ooneuser);

  return oklevel_;
}

/** Returns the WWIV SDK Config Object. */
Config* Application::config() const { return config_.get(); }
void Application::set_config_for_test(const wwiv::sdk::Config& config) {
  if (!config_) {
    config_ = std::make_unique<Config>(config);
  } else {
    *config_ = config;
  }
}

/** Returns the WWIV Names.LST Config Object. */
Names* Application::names() const { return names_.get(); }

msgapi::MessageApi* Application::msgapi(int type) const {
  if (type == 0) {
    return msgapis_.at(2).get();
  }
  return msgapis_.at(type).get();
}

msgapi::MessageApi* Application::msgapi() const {
  if (current_sub().storage_type == 0) {
    LOG(ERROR) << "Invalid storage type on sub: " << current_sub().filename;
  }
  return msgapi(current_sub().storage_type);
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

Batch& Application::batch() { return *batch_; }

static usersubrec empty_user_sub{"", -1};

const usersubrec& Application::current_user_sub() const {
  const auto n = current_user_sub_num();
  if (n >= usub.size()) {
    return empty_user_sub;
  }
  return usub[n];
}

const usersubrec& Application::current_user_dir() const {
  const auto n = current_user_dir_num();
  if (n >= udir.size()) {
    return empty_user_sub;
  }
  return udir[n];
}

const subboard_t& Application::current_sub() const {
  return subs().sub(sess().GetCurrentReadMessageArea());
}

void Application::set_current_user_dir_num(int n) { 
  user_dir_num_ = static_cast<uint16_t>(n);
  if (user_dir_num_ < 0 || user_dir_num_ > wwiv::stl::size_int(udir)) {
    user_dir_num_ = 0;
  }

  auto subnum = current_user_dir().subnum;
  if (subnum < 0 || subnum  > dirs().size()) {
    subnum = 0;
  }
  // Update the value in the context.
  sess().current_dir(dirs()[subnum]);
}

const files::directory_t& Application::current_dir() const {
 return dirs()[current_user_dir().subnum];
}

Subs& Application::subs() {
  CHECK(subs_);
  return *subs_;
}

const Subs& Application::subs() const {
  CHECK(subs_);
  return *subs_;
}

Conferences& Application::all_confs() {
  CHECK(all_confs_);
  return *all_confs_;
}

const Conferences& Application::all_confs() const {
  CHECK(all_confs_);
  return *all_confs_;
}

files::Dirs& Application::dirs() {
  CHECK(dirs_);
  return *dirs_;
}

const files::Dirs& Application::dirs() const {
  CHECK(dirs_);
  return *dirs_;
}

Networks& Application::nets() {
  CHECK(nets_);
  return *nets_;
}

const Networks& Application::nets() const {
  CHECK(nets_);
  return *nets_;
}

GFiles& Application::gfiles() {
  CHECK(gfiles_);
  return *gfiles_;
}

const GFiles& Application::gfiles() const {
  CHECK(gfiles_);
  return *gfiles_;
}

Instances& Application::instances() {
  CHECK(instances_);
  return *instances_;
}

const Instances& Application::instances() const {
  CHECK(instances_);
  return *instances_;
}

const Network& Application::current_net() const {
  const static Network empty_rec(network_type_t::unknown, "(Unknown)");
  if (nets_->empty()) {
    return empty_rec;
  }
  return nets_->at(net_num());
}

wwiv::sdk::net::Network& Application::mutable_current_net() {
  static wwiv::sdk::net::Network empty_rec{};
  if (nets_->empty()) {
    return empty_rec;
  }
  return nets_->at(net_num());
}

bool Application::IsUseInternalFsed() const { return internal_fsed_; }

/**
 * Should be called after a user is logged off, and will initialize
 * screen-access variables.
 */
void Application::frequent_init() {
  setiia(seconds(5));

  // Context Globals to move to Application
  // Context Globals in Application
  set_extratimecall(seconds(0));
  ReadCurrentUser(1);
  sess().reset();
  bin.charbufferpointer_ = 0;
  received_short_message(false);
  use_workspace = false;

  set_net_num(0);
  read_qscn(1, sess().qsc, false);
  reset_disable_conf();

  // Output context
  bout.reset();
  bin.okskey(true);

  // DSZ Log
  File::Remove(dsz_logfile_name_, true);
}

// This function checks to see if the user logged on to the com port has
// hung up.  Obviously, if no user is logged on remotely, this does nothing.
// returns the value of sess().hangup()
bool Application::CheckForHangup() {
  if (!sess().hangup() && sess().using_modem() && !bout.remoteIO()->connected()) {
    if (sess().IsUserOnline()) {
      sysoplog() << "Hung Up.";
    }
    a()->Hangup(hangup_type_t::user_disconnected);
  }
  return sess().hangup();
}

void Application::Hangup(hangup_type_t t) {
  if (!cleanup_cmd.empty()) {
    bout.nl();
    const auto cmd = stuff_in(cleanup_cmd, "", "", "", "", "");
    ExecuteExternalProgram(cmd, spawn_option(SPAWNOPT_CLEANUP));
    bout.nl(2);
  }
  if (sess().hangup()) {
    return;
  }
  sess().hangup(true);
  VLOG(1) << "Invoked Hangup()";
  throw hangup_error(t, user()->name_and_number());
}
