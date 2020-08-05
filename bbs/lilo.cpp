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

#include "bbs/automsg.h"
#include "bbs/basic/basic.h"
#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/com.h"
#include "bbs/confutil.h"
#include "bbs/connect1.h"
#include "bbs/datetime.h"
#include "bbs/defaults.h"
#include "bbs/dropfile.h"
#include "bbs/email.h"
#include "bbs/execexternal.h"
#include "bbs/finduser.h"
#include "bbs/inetmsg.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/menusupp.h"
#include "bbs/msgbase1.h"
#include "bbs/newuser.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/readmail.h"
#include "bbs/remote_io.h"
#include "bbs/shortmsg.h"
#include "bbs/stuffin.h"
#include "bbs/sysoplog.h"
#include "bbs/trashcan.h"
#include "bbs/utility.h"
#include "bbs/wqscn.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/inifile.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "fmt/printf.h"
#include "local_io/wconstants.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include <chrono>
#include <limits>
#include <memory>
#include <string>

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

#define SECS_PER_DAY 86400L

static char g_szLastLoginDate[9];


static void CleanUserInfo() {
  if (okconf(a()->user())) {
    setuconf(ConferenceType::CONF_SUBS, a()->user()->GetLastSubConf(), 0);
    setuconf(ConferenceType::CONF_DIRS, a()->user()->GetLastDirConf(), 0);
  }
  if (a()->user()->GetLastSubNum() > a()->config()->max_subs()) {
    a()->user()->SetLastSubNum(0);
  }
  if (a()->user()->GetLastDirNum() > a()->config()->max_dirs()) {
    a()->user()->SetLastDirNum(0);
  }
  if (a()->usub[a()->user()->GetLastSubNum()].subnum != -1) {
    a()->set_current_user_sub_num(a()->user()->GetLastSubNum());
  }
  if (a()->udir[a()->user()->GetLastDirNum()].subnum != -1) {
    a()->set_current_user_dir_num(a()->user()->GetLastDirNum());
  }

}

bool IsPhoneNumberUSAFormat(User *pUser) {
  string country = pUser->GetCountry();
  return (country == "USA" || country == "CAN" || country == "MEX");
}

static int GetAnsiStatusAndShowWelcomeScreen() {
  bout << "\r\nWWIV " << full_version() << wwiv::endl;
  bout << "Copyright (c) 1998-2020, WWIV Software Services." << wwiv::endl;
  bout << "All Rights Reserved." << wwiv::endl;

  int ans = check_ansi();
  if (ans > 0) {
    a()->user()->SetStatusFlag(User::ansi);
    a()->user()->SetStatusFlag(User::status_color);
  }
  bout.nl();
  if (!printfile_random(WELCOME_NOEXT)) {
    printfile(WELCOME_NOEXT);
  }
  if (bout.curatr() != 7) {
    bout.ResetColors();
  }
  return ans;
}

static uint16_t FindUserByRealName(const std::string& user_name) {
  if (user_name.empty()) {
    return 0;
  }

  bout << "Searching...";
  bool abort = false;
  int current_count = 0;
  for (const auto& n : a()->names()->names_vector()) {
    if (a()->hangup_ || abort) { break; }
    if (++current_count % 25 == 0) {
      // changed from 15 since computers are faster now-a-days
      bout << ".";
    }
    auto current_user = n.number;
    a()->ReadCurrentUser(current_user);
    string temp_user_name(a()->user()->GetRealName());
    StringUpperCase(&temp_user_name);
    if (user_name == temp_user_name && !a()->user()->IsUserDeleted()) {
      bout << "|#5Do you mean " << a()->names()->UserName(n.number) << "? ";
      if (yesno()) {
        return current_user;
      }
    }
    checka(&abort);
  }
  return 0;
}

static int ShowLoginAndGetUserNumber(string remote_username) {
  bout.nl();
  bout << "Enter number or name or 'NEW'\r\n";
  bout << "NN: ";

  string user_name;
  if (remote_username.empty()) {
    user_name = input_upper(30);
  } else {
    bout << remote_username << wwiv::endl;
    user_name = remote_username;
  }
  StringTrim(&user_name);

  Trashcan trashcan(*a()->config());
  if (trashcan.IsTrashName(user_name)) {
    LOG(INFO) << "Trashcan name entered from IP: " << a()->remoteIO()->remote_info().address
              << "; name: " << user_name;
    hang_it_up();
    Hangup();
    return 0;
  }


  auto user_number = finduser(user_name);
  if (user_number != 0) {
    return user_number;
  }
  return FindUserByRealName(user_name);
}

bool IsPhoneRequired() {
  IniFile ini(FilePath(a()->bbspath(), WWIV_INI),
              {StrCat("WWIV-", a()->instance_number()), INI_TAG});
  if (ini.IsOpen()) {
    if (ini.value<bool>("NEWUSER_MIN")) {
      return false;
    }
    if (!ini.value<bool>("LOGON_PHONE")) {
      return false;
    }
  }
  return true;
}

bool VerifyPhoneNumber() {
  if (IsPhoneNumberUSAFormat(a()->user()) || !a()->user()->GetCountry()[0]) {
    string phoneNumber = input_password("PH: ###-###-", 4);

    if (phoneNumber != &a()->user()->GetVoicePhoneNumber()[8]) {
      if (phoneNumber.length() == 4 && phoneNumber[3] == '-') {
        bout << "\r\n!! Enter the LAST 4 DIGITS of your phone number ONLY !!\r\n\n";
      }
      return false;
    }
  }
  return true;
}

static bool VerifyPassword(string remote_password) {
  a()->UpdateTopScreen();

  if (!remote_password.empty() && remote_password == a()->user()->GetPassword()) {
    return true;
  }

  string password = input_password("PW: ", 8);
  return (password == a()->user()->GetPassword());
}

static bool VerifySysopPassword() {
  string password = input_password("SY: ", 20);
  return (password == a()->config()->system_password()) ? true : false;
}

static void DoFailedLoginAttempt() {
  a()->user()->SetNumIllegalLogons(a()->user()->GetNumIllegalLogons() + 1);
  a()->WriteCurrentUser();
  bout << "\r\n\aILLEGAL LOGON\a\r\n\n";

  const string logline = StrCat("### ILLEGAL LOGON for ",
    a()->names()->UserName(a()->usernum));
  sysoplog(false) << "";
  sysoplog(false) << logline;
  sysoplog(false) << "";
  a()->usernum = 0;
}

static void LeaveBadPasswordFeedback(int ans) {
  ScopeExit at_exit([] {
    a()->usernum = 0;
  });
  
  if (ans > 0) {
    a()->user()->SetStatusFlag(User::ansi);
  } else {
    a()->user()->ClearStatusFlag(User::ansi);
  }
  bout << "|#6Too many logon attempts!!\r\n\n";
  bout << "|#9Would you like to leave Feedback to " << a()->config()->sysop_name() << "? ";
  if (!yesno()) {
    return;
  }
  bout.nl();
  bout << "What is your NAME or HANDLE? ";
  auto tempName = input_proper("", 31);
  if (tempName.empty()) {
    return;
  }
  bout.nl();
  a()->usernum = 1;
  a()->user()->set_name(tempName.c_str());
  a()->user()->SetMacro(0, "");
  a()->user()->SetMacro(1, "");
  a()->user()->SetMacro(2, "");
  a()->user()->SetSl(a()->config()->newuser_sl());
  a()->user()->SetScreenChars(80);
  if (ans > 0) {
    select_editor();
  } else {
    a()->user()->SetDefaultEditor(0);
  }
  a()->user()->SetNumEmailSent(0);
  bool bSaveAllowCC = a()->IsCarbonCopyEnabled();
  a()->SetCarbonCopyEnabled(false);
  auto title = StrCat("** Illegal logon feedback from ", tempName);
  email(title, 1, 0, true, 0, true);
  a()->SetCarbonCopyEnabled(bSaveAllowCC);
  if (a()->user()->GetNumEmailSent() > 0) {
    ssm(1) << "Check your mailbox.  Someone forgot their password again!";
  }
}

static void CheckCallRestrictions() {
  if (a()->usernum > 0 && a()->user()->IsRestrictionLogon() &&
      date() == a()->user()->GetLastOn() &&
      a()->user()->GetTimesOnToday() > 0) {
    bout.nl();
    bout << "|#6Sorry, you can only logon once per day.\r\n";
    Hangup();
  }
}

static void logon_guest() {
  a()->SetUserOnline(true);
  bout.nl(2);
  input_ansistat();

  printfile(GUEST_NOEXT);
  pausescr();

  string userName, reason;
  int count = 0;
  do {
    bout << "\r\n|#5Enter your real name : ";
    userName = input_upper(25);
    bout << "\r\n|#5Purpose of your call?\r\n";
    reason = input_text(79);
    if (!userName.empty() && !reason.empty()) {
      break;
    }
    count++;
  } while (count < 3);

  if (count >= 3) {
    printfile(REJECT_NOEXT);
    ssm(1) << "Guest Account failed to enter name and purpose";
    Hangup();
  } else {
    ssm(1) << "Guest Account accessed by " << userName << " on " << times() << " for" << reason;
  }
}

void getuser() {
  // Reset the key timeout to 30 seconds while trying to log in a user.
  ScopeExit at_exit([] { a()->context().okmacro(true); bout.reset_key_timeout(); });

  a()->context().okmacro(false);
  // TODO(rushfan): uncomment this
  //bout.set_logon_key_timeout();

  // Let's set this to 0 here since we don't have a user yet.
  a()->usernum = 0;
  a()->SetCurrentConferenceMessageArea(0);
  a()->SetCurrentConferenceFileArea(0);
  a()->effective_sl(a()->config()->newuser_sl());
  a()->user()->SetStatus(0);

  const auto& ip = a()->remoteIO()->remote_info().address;
  const auto& hostname = a()->remoteIO()->remote_info().address_name;
  if (!ip.empty()) {
    bout << "Client Address: " << hostname << " (" << ip << ")" << wwiv::endl;
    bout.nl();
  }
  write_inst(INST_LOC_GETUSER, 0, INST_FLAGS_NONE);

  int count = 0;
  bool ok = false;

  int ans = GetAnsiStatusAndShowWelcomeScreen();
  bool first_time = true;
  do {
    string remote_username;
    string remote_password;
    if (first_time) {
      remote_username = ToStringUpperCase(a()->remoteIO()->remote_info().username);
      remote_password = ToStringUpperCase(a()->remoteIO()->remote_info().password);
      first_time = false;
    }
    auto usernum = ShowLoginAndGetUserNumber(remote_username);
    if (usernum > 0) {
      a()->usernum = static_cast<uint16_t>(usernum);
      a()->ReadCurrentUser();
      read_qscn(a()->usernum, a()->context().qsc, false);
      if (!set_language(a()->user()->GetLanguage())) {
        a()->user()->SetLanguage(0);
        set_language(0);
      }
      int nInstanceNumber;
      if (a()->user()->GetSl() < 255 && user_online(a()->usernum, &nInstanceNumber)) {
        bout << "\r\n|#6You are already online on instance " << nInstanceNumber << "!\r\n\n";
        continue;
      }
      ok = true;
      if (a()->context().guest_user()) {
        logon_guest();
      } else {
        a()->effective_sl(a()->config()->newuser_sl());
        if (!VerifyPassword(remote_password)) {
          ok = false;
        }

        if ((a()->config()->sysconfig_flags() & sysconfig_free_phone) == 0 && IsPhoneRequired()) {
          if (!VerifyPhoneNumber()) {
            ok = false;
          }
        }
        if (a()->user()->GetSl() == 255 && a()->context().incom() && ok) {
          if (!VerifySysopPassword()) {
            ok = false;
          }
        }
      }
      if (ok) {
        a()->reset_effective_sl();
        changedsl();
      } else {
        DoFailedLoginAttempt();
      }
    } else if (usernum == 0) {
      bout.nl();
      bout << "|#6Unknown user.\r\n";
      a()->usernum = static_cast<uint16_t>(usernum);
    } else if (usernum == -1) {
      write_inst(INST_LOC_NEWUSER, 0, INST_FLAGS_NONE);
      play_sdf(NEWUSER_NOEXT, false);
      newuser();
      ok = true;
    }
  } while (!ok && ++count < 3);

  if (count >= 3) {
    LeaveBadPasswordFeedback(ans);
  }

  CheckCallRestrictions();
  if (!ok) {
    Hangup();
  }
}

static void FixUserLinesAndColors() {
  if (a()->user()->GetNumExtended() > a()->max_extend_lines) {
    a()->user()->SetNumExtended(a()->max_extend_lines);
  }
  if (a()->user()->GetColor(8) == 0 || a()->user()->GetColor(9) == 0) {
    a()->user()->SetColor(8, a()->newuser_colors[8]);
    a()->user()->SetColor(9, a()->newuser_colors[9]);
  }
}

static void UpdateUserStatsForLogin() {
  to_char_array(g_szLastLoginDate, date());
  if (a()->user()->GetLastOn() != g_szLastLoginDate) {
    wwiv::sdk::ResetTodayUserStats(a()->user());
  }
  wwiv::sdk::AddCallToday(a()->user());
  a()->set_current_user_sub_num(0);
  a()->SetNumMessagesReadThisLogon(0);
  if (a()->udir[0].subnum == 0 && a()->udir[1].subnum > 0) {
    a()->set_current_user_dir_num(1);
  } else {
    a()->set_current_user_dir_num(0);
  }
  if (a()->effective_sl() != 255 && !a()->context().guest_user()) {
    a()->status_manager()->Run([](WStatus& s) {
      s.IncrementCallerNumber();
      s.IncrementNumCallsToday();
    });
  }
}

static void PrintLogonFile() {
  if (!a()->context().incom()) {
    return;
  }
  play_sdf(LOGON_NOEXT, false);
  if (!printfile(LOGON_NOEXT)) {
    pausescr();
  }
}

static void PrintUserSpecificFiles() {
  const auto* user = a()->user();  // not-owned
  printfile(fmt::format("sl{}", user->GetSl()));
  printfile(fmt::format("dsl{}", user->GetDsl()));

  const int short_size = std::numeric_limits<uint16_t>::digits - 1;
  for (int i=0; i < short_size; i++) {
    if (user->HasArFlag(1 << i)) {
      printfile(fmt::format("ar{}", static_cast<char>('A' + i)));
    }
  }

  for (int i=0; i < short_size; i++) {
    if (user->HasDarFlag(1 << i)) {
      printfile(fmt::format("dar{}", static_cast<char>('A' + i)));
    }
  }
}

static std::string CreateLastOnLogLine(const WStatus& status) {
  string log_line;
  if (a()->HasConfigFlag(OP_FLAGS_SHOW_CITY_ST) &&
    (a()->config()->sysconfig_flags() & sysconfig_extended_info)) {
    const string username_num = a()->names()->UserName(a()->usernum);
    const string t = times();
    const string f = fulldate();
    log_line = fmt::sprintf(
      "|#1%-6ld %-25.25s %-5.5s %-5.5s %-15.15s %-2.2s %-3.3s %-8.8s %2d\r\n",
      status.GetCallerNumber(),
      username_num,
      t,
      f,
      a()->user()->GetCity(),
      a()->user()->GetState(),
      a()->user()->GetCountry(),
      a()->GetCurrentSpeed(),
      a()->user()->GetTimesOnToday());
  } else {
    const string username_num = a()->names()->UserName(a()->usernum);
    const string t = times();
    const string f = fulldate();
    log_line = fmt::sprintf(
      "|#1%-6ld %-25.25s %-10.10s %-5.5s %-5.5s %-20.20s %2d\r\n",
      status.GetCallerNumber(),
      username_num,
      a()->cur_lang_name,
      t,
      f,
      a()->GetCurrentSpeed(),
      a()->user()->GetTimesOnToday());
  }
  return log_line;
}


static void UpdateLastOnFile() {
  const auto laston_txt_filename = FilePath(a()->config()->gfilesdir(), LASTON_TXT);
  vector<string> lines;
  {
    TextFile laston_file(laston_txt_filename, "r");
    lines = laston_file.ReadFileIntoVector();
  }

  if (!lines.empty()) {
    bool needs_header = true;
    for (const auto& line : lines) {
      if (line.empty()) {
        continue;
      }
      if (needs_header) {
        bout.nl(2);
        bout << "|#1Last few callers|#7: |#0";
        bout.nl(2);
        if (a()->HasConfigFlag(OP_FLAGS_SHOW_CITY_ST) &&
            (a()->config()->sysconfig_flags() & sysconfig_extended_info)) {
          bout << "|#2Number Name/Handle               Time  Date  City            ST Cty Speed    ##" << wwiv::endl;
        } else {
          bout << "|#2Number Name/Handle               Language   Time  Date  Speed                ##" << wwiv::endl;
        }
        char chLine = (okansi()) ? static_cast<char>('\xCD') : '=';
        bout << "|#7" << std::string(79, chLine) << wwiv::endl;
        needs_header = false;
      }
      bout << line << wwiv::endl;
      if (checka()) {
        break;
      }
    }
    bout.nl(2);
    pausescr();
  }

  auto status = a()->status_manager()->GetStatus();
  {
    const string username_num = a()->names()->UserName(a()->usernum);
    string t = times();
    string f = fulldate();
    const string sysop_log_line = fmt::sprintf("%ld: %s %s %s   %s - %d (%u)",
      status->GetCallerNumber(),
      username_num,
      t,
      f,
      a()->GetCurrentSpeed(),
      a()->user()->GetTimesOnToday(),
      a()->instance_number());
    sysoplog(false) << "";
    sysoplog(false) << stripcolors(sysop_log_line);
    sysoplog(false) << "";
  }
  if (a()->context().incom()) {
    const auto remote_address = a()->remoteIO()->remote_info().address;
    if (!remote_address.empty()) {
      sysoplog() << "Remote IP: " << remote_address;
    }
  }
  if (a()->effective_sl() == 255 && !a()->context().incom()) {
    return;
  }

  // add line to laston.txt. We keep 8 lines
  if (a()->effective_sl() != 255) {
    TextFile lastonFile(laston_txt_filename, "w");
    if (lastonFile.IsOpen()) {
      auto it = lines.begin();
      // skip over any lines over 7
      while (lines.size() - std::distance(lines.begin(), it) >= 8) {
        it++;
      }
      while (it != lines.end()) {
        lastonFile.WriteLine(*it++);
      }
      lastonFile.Write(CreateLastOnLogLine(*status.get()));
      lastonFile.Close();
    }
  }
}

static void CheckAndUpdateUserInfo() {
  if (a()->user()->birthday_year() == 0) {
    bout << "\r\nPlease enter the following information:\r\n";
    do {
      bout.nl();
      input_age(a()->user());
      bout.nl();
      bout.format("{} -- Correct? ", a()->user()->birthday_mmddyy());
      if (!yesno()) {
        a()->user()->birthday_mdy(0, 0, 0);
      }
    } while (a()->user()->birthday_year() == 0);
  }

  if (!a()->user()->GetRealName()[0]) {
    input_realname();
  }
  if (!(a()->config()->sysconfig_flags() & sysconfig_extended_info)) {
    return;
  }
  if (!a()->user()->GetStreet()[0]) {
    input_street();
  }
  if (!a()->user()->GetCity()[0]) {
    input_city();
  }
  if (!a()->user()->GetState()[0]) {
    input_state();
  }
  if (!a()->user()->GetCountry()[0]) {
    input_country();
  }
  if (!a()->user()->GetZipcode()[0]) {
    input_zipcode();
  }
  if (!a()->user()->GetDataPhoneNumber()[0]) {
    input_dataphone();
  }
  if (a()->user()->GetComputerType() == -1) {
    input_comptype();
  }
}

static std::string to_string(const net_networks_rec& n) {
  switch (n.type) { case network_type_t::ftn:
    return fmt::format("|#9(|#1{}|#9@|#2{}|#9) ", n.fido.fido_address, n.name);
  case network_type_t::internet:
    return fmt::format("|#9(|#2{}|#9@|#1Internet|#9) ", n.name);
  case network_type_t::news:
    return fmt::format("|#9(|#2{}|#9@|#1News|#9) ", n.name);
  case network_type_t::wwivnet:
  default:
    return fmt::format("|#9(|#2@{}|#9.|#1{}|#9) ", n.sysnum, n.name);
  }
}

static void DisplayUserLoginInformation() {
  bout.nl();

  const string username_num = a()->names()->UserName(a()->usernum);
  bout << "|#9Name/Handle|#0....... |#2" << username_num << wwiv::endl;
  bout << "|#9Internet Address|#0.. |#2";
  if (check_inet_addr(a()->user()->GetEmailAddress())) {
    bout << a()->user()->GetEmailAddress() << wwiv::endl;
  } else if (!a()->internetEmailName.empty()) {
    bout << (a()->IsInternetUseRealNames() ? a()->user()->GetRealName() :
                           a()->user()->GetName())
                       << "<" << a()->internetFullEmailAddress << ">\r\n";
  } else {
    bout << "None.\r\n";
  }
  auto la = a()->user()->last_address();
  if (!la.empty()) {
    bout << "|#9Last IP Address|#0... |#2" << a()->user()->last_address() << wwiv::endl;
  }

  bout << "|#9Time allowed on|#0... |#2" << (nsl() + 30) / SECONDS_PER_MINUTE
       << wwiv::endl;
  if (a()->user()->GetNumIllegalLogons() > 0) {
    bout << "|#9Illegal logons|#0.... |#2" << a()->user()->GetNumIllegalLogons()
         << wwiv::endl;
  }
  if (a()->user()->GetNumMailWaiting() > 0) {
    bout << "|#9Mail waiting|#0...... |#2" << a()->user()->GetNumMailWaiting()
         << wwiv::endl;
  }
  if (a()->user()->GetTimesOnToday() == 1) {
    bout << "|#9Date last on|#0...... |#2" << a()->user()->GetLastOn() << wwiv::endl;
  } else {
    bout << "|#9Times on today|#0.... |#2" << a()->user()->GetTimesOnToday() << wwiv::endl;
  }
  bout << "|#9Sysop currently|#0... |#2";
  if (sysop2()) {
    bout << "Available\r\n";
  } else {
    bout << "NOT Available\r\n";
  }

  bout << "|#9System is|#0......... |#2WWIV " << full_version() << "  " << wwiv::endl;

  /////////////////////////////////////////////////////////////////////////
  a()->status_manager()->RefreshStatusCache();
  const auto screen_width = a()->user()->GetScreenChars() - 19;
  int width = 0;
  bout << "|#9Networks|#0.......... ";
  for (const auto& n : a()->nets().networks()) {
    const auto s = to_string(n);
    const auto len = size_without_colors(s);
    if (width + len >= screen_width) {
      bout.nl();
      bout << "|#9Networks|#0.......... ";
      width = 0;
    }
    bout.bputs(s);
    width += len;
  }
  bout.nl();

  bout << "|#9Host OS|#0........... |#2" << wwiv::os::os_version_string() << wwiv::endl;
  bout << "|#9Instance|#0.......... |#2" << a()->instance_number() << "\r\n\n";
  if (a()->user()->GetForwardUserNumber()) {
    if (a()->user()->GetForwardSystemNumber() != 0) {
      set_net_num(a()->user()->GetForwardNetNumber());
      if (!valid_system(a()->user()->GetForwardSystemNumber())) {
        a()->user()->ClearMailboxForward();
        bout << "Forwarded to unknown system; forwarding reset.\r\n";
      } else {
        bout << "Mail set to be forwarded to ";
        if (wwiv::stl::ssize(a()->nets()) > 1) {
          bout << "#" << a()->user()->GetForwardUserNumber()
               << " @"
               << a()->user()->GetForwardSystemNumber()
               << "." << a()->network_name() << "."
               << wwiv::endl;
        } else {
          bout << "#" << a()->user()->GetForwardUserNumber() << " @"
               << a()->user()->GetForwardSystemNumber() << "." << wwiv::endl;
        }
      }
    } else {
      if (a()->user()->IsMailboxClosed()) {
        bout << "Your mailbox is closed.\r\n\n";
      } else {
        bout << "Mail set to be forwarded to #" 
             << a()->user()->GetForwardUserNumber() << wwiv::endl;
      }
    }
  } else if (a()->user()->GetForwardSystemNumber() != 0) {
    string internet_addr;
    read_inet_addr(internet_addr, a()->usernum);
    bout << "Mail forwarded to Internet " << internet_addr << ".\r\n";
  }
  if (a()->IsTimeOnlineLimited()) {
    bout << "\r\n|#3Your on-line time is limited by an external event.\r\n\n";
  }
}

static void LoginCheckForNewMail() {
  bout << "|#9Scanning for new mail... ";
  if (a()->user()->GetNumMailWaiting() > 0) {
    int nNumNewMessages = check_new_mail(a()->usernum);
    if (nNumNewMessages) {
      bout << "|#9You have |#2" << nNumNewMessages 
           << "|#9 new message(s).\r\n\r\n"
           << "|#9Read your mail now? ";
      if (noyes()) {
        readmail(1);
      }
    } else {
      bout << "|#9You have |#2" << a()->user()->GetNumMailWaiting() 
           << "|#9 old message(s) in your mailbox.\r\n";
    }
  } else {
    bout << " |#9No mail found.\r\n";
  }
}

static vector<bool> read_voting() {
  vector<votingrec> votes;
  DataFile<votingrec> file(FilePath(a()->config()->datadir(), VOTING_DAT));
  vector<bool> questused(20);
  if (!file) {
    return questused;
  }
  file.ReadVector(votes);
  int cur = 0;
  for (const auto& v : votes) {
    if (v.numanswers != 0) {
      questused[cur] = true;
    }
    ++cur;
  }
  return questused;
}

static void CheckUserForVotingBooth() {
  vector<bool> questused = read_voting();
  if (!a()->user()->IsRestrictionVote() && a()->effective_sl() > a()->config()->newuser_sl()) {
    for (int i = 0; i < 20; i++) {
      if (questused[i] && a()->user()->GetVote(i) == 0) {
        bout.nl();
        bout << "|#9You haven't voted yet.\r\n";
        return;
      }
    }
  }
}

// Updates the User::last_address field since we've already displayed
// it to the user from the last session.
static void UpdateLastAddress() {
  if (!a()->context().incom()) {
    return;
  }
  const auto remote_address = a()->remoteIO()->remote_info().address;
  if (remote_address.empty()) {
    return;
  }
  auto ipaddr = ip_address::from_string(remote_address);
  if (!ipaddr) {
    return;
  }
  a()->user()->last_address(ipaddr.value());
}

void logon() {
  if (a()->usernum < 1) {
    Hangup();
    return;
  }
  a()->SetUserOnline(true);
  write_inst(INST_LOC_LOGON, 0, INST_FLAGS_NONE);
  get_user_ppp_addr();
  bout.ResetColors();
  bout.Color(0);
  bout.cls();

  FixUserLinesAndColors();
  UpdateUserStatsForLogin();
  PrintLogonFile();
  UpdateLastOnFile();
  PrintUserSpecificFiles();

  read_automessage();
  a()->SetLogonTime();
  a()->UpdateTopScreen();
  bout.nl(2);
  pausescr();
  if (!a()->logon_cmd.empty()) {
    bout.nl();
    const auto cmd = stuff_in(a()->logon_cmd, create_chain_file(), "", "", "", "");
    ExecuteExternalProgram(cmd, a()->spawn_option(SPAWNOPT_LOGON));
    bout.nl(2);
  }

  DisplayUserLoginInformation();
  UpdateLastAddress();

  CheckAndUpdateUserInfo();

  a()->UpdateTopScreen();
  a()->read_subs();
  rsm(a()->usernum, a()->user(), true);

  LoginCheckForNewMail();

  if (a()->user()->GetNewScanDateNumber()) {
    a()->context().nscandate(a()->user()->GetNewScanDateNumber());
  } else {
    a()->context().nscandate(a()->user()->GetLastOnDateNumber());
  }
  a()->batch().clear();

  CheckUserForVotingBooth();

  if ((a()->context().incom() || sysop1()) && a()->user()->GetSl() < 255) {
    broadcast(fmt::format("{} Just logged on!", a()->user()->GetName()));
  }
  setiia(std::chrono::seconds(5));

  // New Message Scan
  if (a()->IsNewScanAtLogin()) {
    bout << "\r\n|#5Scan All Message Areas For New Messages? ";
    if (yesno()) {
      NewMsgsAllConfs();
    }
  }

  // Handle case of first conf with no subs avail
  if (a()->usub[0].subnum == -1 && okconf(a()->user())) {
    for (a()->SetCurrentConferenceMessageArea(0); 
         (a()->GetCurrentConferenceMessageArea() < ssize(a()->subconfs))
         && (a()->uconfsub[a()->GetCurrentConferenceMessageArea()].confnum != -1);
         a()->SetCurrentConferenceMessageArea(a()->GetCurrentConferenceMessageArea() + 1)) {
      setuconf(ConferenceType::CONF_SUBS, a()->GetCurrentConferenceMessageArea(), -1);
      if (a()->usub[0].subnum != -1) {
        break;
      }
    }
    if (a()->usub[0].subnum == -1) {
      a()->SetCurrentConferenceMessageArea(0);
      setuconf(ConferenceType::CONF_SUBS, a()->GetCurrentConferenceMessageArea(), -1);
    }
  }

  if (a()->HasConfigFlag(OP_FLAGS_USE_FORCESCAN)) {
    bool nextsub = false;
    if (a()->user()->GetSl() < 255) {
      a()->context().forcescansub(true);
      qscan(a()->GetForcedReadSubNumber(), nextsub);
      a()->context().forcescansub(false);
    } else {
      qscan(a()->GetForcedReadSubNumber(), nextsub);
    }
  }
  CleanUserInfo();
}

void logoff() {
  mailrec m;

  if (a()->context().incom()) {
    play_sdf(LOGOFF_NOEXT, false);
  }

  if (a()->usernum > 0) {
    if ((a()->context().incom() || sysop1()) && a()->user()->GetSl() < 255) {
      broadcast(fmt::format("{} Just logged off!", a()->user()->GetName()));
    }
  }
  setiia(std::chrono::seconds(5));
  a()->remoteIO()->disconnect();
  // Don't need hangup here, but *do* want to ensure that a()->hangup_ is true.
  a()->hangup_ = true;
  VLOG(1) << "Setting a()->hangup_=true in logoff";
  if (a()->usernum < 1) {
    return;
  }

  string text = "  Logged Off At ";
  text += times();
  if (a()->effective_sl() != 255 || a()->context().incom()) {
    sysoplog(false) << "";
    sysoplog(false) << stripcolors(text);
  }
  a()->user()->SetLastBaudRate(a()->modem_speed_);

  // put this back here where it belongs... (not sure why it te
  a()->user()->SetLastOn(g_szLastLoginDate);

  a()->user()->SetNumIllegalLogons(0);
  auto seconds_used_duration = duration_cast<seconds>(
      std::chrono::system_clock::now() - a()->system_logon_time() - a()->extratimecall());
  a()->user()->add_timeon(seconds_used_duration);
  a()->user()->add_timeon_today(seconds_used_duration);

  a()->status_manager()->Run([=](WStatus& s) {
    int active_today = s.GetMinutesActiveToday();
    auto minutes_used_now = std::chrono::duration_cast<std::chrono::minutes>(seconds_used_duration).count();
    s.SetMinutesActiveToday(active_today + minutes_used_now);
  });

  if (a()->context().scanned_files()) {
    a()->user()->SetNewScanDateNumber(a()->user()->GetLastOnDateNumber());
  }
  a()->user()->SetLastOnDateNumber(daten_t_now());
  auto used_this_session = (std::chrono::system_clock::now() - a()->system_logon_time());
  auto min_used = std::chrono::duration_cast<std::chrono::minutes>(used_this_session);
  sysoplog(false) << "Read: " << a()->GetNumMessagesReadThisLogon() 
      << "   Time on: "  << min_used.count() << " minutes.";
  {
    unique_ptr<File> pFileEmail(OpenEmailFile(true));
    if (pFileEmail->IsOpen()) {
      a()->user()->SetNumMailWaiting(0);
      auto num_records = static_cast<int>(pFileEmail->length() / sizeof(mailrec));
      int r = 0;
      int w = 0;
      while (r < num_records) {
        pFileEmail->Seek(static_cast<long>(sizeof(mailrec)) * static_cast<long>(r), File::Whence::begin);
        pFileEmail->Read(&m, sizeof(mailrec));
        if (m.tosys != 0 || m.touser != 0) {
          if (m.tosys == 0 && m.touser == a()->usernum) {
            if (a()->user()->GetNumMailWaiting() != 255) {
              a()->user()->SetNumMailWaiting(a()->user()->GetNumMailWaiting() + 1);
            }
          }
          if (r != w) {
            pFileEmail->Seek(static_cast<long>(sizeof(mailrec)) * static_cast<long>(w), File::Whence::begin);
            pFileEmail->Write(&m, sizeof(mailrec));
          }
          ++w;
        }
        ++r;
      }
      if (r != w) {
        m.tosys = 0;
        m.touser = 0;
        for (int w1 = w; w1 < r; w1++) {
          pFileEmail->Seek(static_cast<long>(sizeof(mailrec)) * static_cast<long>(w1), File::Whence::begin);
          pFileEmail->Write(&m, sizeof(mailrec));
        }
      }
      pFileEmail->set_length(static_cast<long>(sizeof(mailrec)) * static_cast<long>(w));
      a()->status_manager()->Run([](WStatus& s) {
        s.IncrementFileChangedFlag(WStatus::fileChangeEmail);
      });
      pFileEmail->Close();
    }
  }
  if (a()->received_short_message_) {
    File smwFile(FilePath(a()->config()->datadir(), SMW_DAT));
    if (smwFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      auto num_records = static_cast<int>(smwFile.length() / sizeof(shortmsgrec));
      int r = 0;
      int w = 0;
      while (r < num_records) {
        shortmsgrec sm;
        smwFile.Seek(r * sizeof(shortmsgrec), File::Whence::begin);
        smwFile.Read(&sm, sizeof(shortmsgrec));
        if (sm.tosys != 0 || sm.touser != 0) {
          if (sm.tosys == 0 && sm.touser == a()->usernum) {
            a()->user()->SetStatusFlag(User::SMW);
          }
          if (r != w) {
            smwFile.Seek(w * sizeof(shortmsgrec), File::Whence::begin);
            smwFile.Write(&sm, sizeof(shortmsgrec));
          }
          ++w;
        }
        ++r;
      }
      smwFile.set_length(w * sizeof(shortmsgrec));
      smwFile.Close();
    }
  }
  a()->WriteCurrentUser();
  write_qscn(a()->usernum, a()->context().qsc, false);
}

