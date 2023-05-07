/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/confutil.h"
#include "bbs/connect1.h"
#include "bbs/defaults.h"
#include "bbs/dropfile.h"
#include "bbs/email.h"
#include "bbs/execexternal.h"
#include "bbs/finduser.h"
#include "bbs/inetmsg.h"
#include "bbs/instmsg.h"
#include "bbs/msgbase1.h"
#include "bbs/newuser.h"
#include "bbs/readmail.h"
#include "bbs/shortmsg.h"
#include "bbs/stuffin.h"
#include "bbs/sysoplog.h"
#include "bbs/trashcan.h"
#include "bbs/utility.h"
#include "bbs/wqscn.h"
#include "bbs/basic/basic.h"
#include "bbs/menus/menusupp.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/output.h"
#include "common/remote_io.h"
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
#include "sdk/bbslist.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/net/networks.h"

#include <chrono>
#include <limits>
#include <memory>
#include <string>

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using namespace wwiv::bbs::menus;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;


static char g_szLastLoginDate[9];


static void CleanUserInfo() {
  if (okconf(a()->user()) && !a()->uconfsub.empty()) {
    setuconf(ConferenceType::CONF_SUBS, a()->user()->subconf(), 0);
    a()->sess().set_current_user_sub_conf_num(a()->user()->subconf());
  } else {
    a()->sess().set_current_user_sub_conf_num(0);    
  }
  if (okconf(a()->user()) && !a()->uconfdir.empty()) {
    setuconf(ConferenceType::CONF_DIRS, a()->user()->dirconf(), 0);
    a()->sess().set_current_user_dir_conf_num(a()->user()->dirconf());
  } else {
    a()->sess().set_current_user_dir_conf_num(0);    
  }
  if (a()->user()->subnum() > a()->config()->max_subs()) {
    a()->user()->subnum(0);
  }
  if (a()->user()->dirnum() > a()->config()->max_dirs()) {
    a()->user()->dirnum(0);
  }
  const auto last_subnum = a()->user()->subnum();
  if (last_subnum < size_int(a()->usub)) {
    a()->set_current_user_sub_num(last_subnum);
  }
  const auto last_dirnum = a()->user()->dirnum();
  if (last_subnum < size_int(a()->udir)) {
    a()->set_current_user_dir_num(last_dirnum);
  }
}
  
bool IsPhoneNumberUSAFormat(User *pUser) {
  const auto country = pUser->country();
  return country == "USA" || country == "CAN" || country == "MEX";
}

static int GetAnsiStatusAndShowWelcomeScreen() {
  bout.print("\r\nWWIV {}\r\n", full_version());
  bout.pl("Copyright (c) 1998-2023, WWIV Software Services.");
  bout.pl("All Rights Reserved.");

  const auto ans = check_ansi();
  if (ans > 0) {
    a()->user()->set_flag(User::flag_ansi);
    a()->user()->set_flag(User::status_color);
  }
  bout.nl();
  if (!bout.printfile_random(WELCOME_NOEXT)) {
    bout.printfile(WELCOME_NOEXT);
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

  bout.outstr("Searching...");
  auto abort = false;
  auto current_count = 0;
  for (const auto& n : a()->names()->names_vector()) {
    if (a()->sess().hangup() || abort) { break; }
    if (++current_count % 25 == 0) {
      // changed from 15 since computers are faster now-a-days
      bout.outstr(".");
    }
    const auto current_user = n.number;
    a()->ReadCurrentUser(current_user);
    
    if (const auto temp_user_name = ToStringUpperCase(a()->user()->real_name());
        temp_user_name == user_name && !a()->user()->deleted()) {
      bout.print("|#5Do you mean {}? ", a()->names()->UserName(n.number));
      if (bin.yesno()) {
        return current_user;
      }
    }
    bin.checka(&abort);
  }
  return 0;
}

static int ShowLoginAndGetUserNumber(const std::string& remote_username) {
  bout.nl();
  // bout.outstr("Enter number or name or 'NEW'\r\nNN: ");
  bout.str("NN_PROMPT");

  std::string user_name;
  if (remote_username.empty()) {
    user_name = bin.input_upper(30);
  } else {
    bout.pl(remote_username);
    user_name = remote_username;
  }
  StringTrim(&user_name);

  Trashcan trashcan(*a()->config());
  if (trashcan.IsTrashName(user_name)) {
    LOG(INFO) << "Trashcan name entered from IP: " << bout.remoteIO()->remote_info().address
              << "; name: " << user_name;
    hang_it_up();
    a()->Hangup();
    return 0;
  }

  if (const auto user_number = finduser(user_name); user_number != 0) {
    return user_number;
  }
  return FindUserByRealName(user_name);
}

static bool IsPhoneRequired() {
  const IniFile ini(FilePath(a()->bbspath(), WWIV_INI),
                    {StrCat("WWIV-", a()->sess().instance_number()), INI_TAG});
  if (ini.IsOpen() && ini.value<bool>("LOGON_PHONE", false)) {
    return true;
  }
  return false;
}

bool VerifyPhoneNumber() {
  if (a()->user()->voice_phone().empty() || !IsPhoneNumberUSAFormat(a()->user()) || a()->user()->country().empty()) {
    return true;
  }

  auto phone_number = bin.input_password("PH: ###-###-", 4);
  if (phone_number != a()->user()->voice_phone().substr(8)) {
    if (phone_number.length() == 4 && phone_number[3] == '-') {
      bout.outstr("\r\n!! Enter the LAST 4 DIGITS of your phone number ONLY !!\r\n\n");
    }
    return false;
  }
  return true;
}

static bool VerifyPassword(const std::string& remote_password) {
  a()->UpdateTopScreen();

  if (!remote_password.empty() && remote_password == a()->user()->password()) {
    return true;
  }

  const auto password = bin.input_password(bout.lang().value("PW_PROMPT"), 8);
  return password == a()->user()->password();
}

static bool VerifySysopPassword() {
  const auto password = bin.input_password("SY: ", 20);
  return password == a()->config()->system_password();
}

static void DoFailedLoginAttempt() {
  a()->user()->increment_illegal_logons();
  a()->WriteCurrentUser();
  bout.outstr("\r\n\aILLEGAL LOGON\a\r\n\n");

  sysoplog(false, "");
  sysoplog(false, fmt::format("### ILLEGAL LOGON for {}", a()->user()->name_and_number()));
  sysoplog(false, "");
  a()->sess().user_num(0);
}

static void LeaveBadPasswordFeedback(int ans) {
  auto at_exit = finally([] {
    a()->sess().user_num(0);
  });
  
  if (ans > 0) {
    a()->user()->set_flag(User::flag_ansi);
  } else {
    a()->user()->clear_flag(User::flag_ansi);
  }
  bout.outstr("|#6Too many logon attempts!!\r\n\n");
  bout.print("|#9Would you like to leave Feedback to {}? ", a()->config()->sysop_name());
  if (!bin.yesno()) {
    return;
  }
  bout.nl();
  bout.outstr("What is your NAME or HANDLE? ");
  const auto temp_name = bin.input_proper("", 31);
  if (temp_name.empty()) {
    return;
  }
  bout.nl();
  a()->sess().user_num(1);
  a()->user()->set_name(temp_name);
  a()->user()->macro(0, "");
  a()->user()->macro(1, "");
  a()->user()->macro(2, "");
  a()->user()->sl(a()->config()->newuser_sl());
  a()->user()->screen_width(80);
  if (ans > 0) {
    select_editor();
  } else {
    a()->user()->default_editor(0);
  }
  a()->user()->email_sent(0);
  const auto save_allow_cc = a()->IsCarbonCopyEnabled();
  a()->SetCarbonCopyEnabled(false);
  const auto title = StrCat("** Illegal logon feedback from ", temp_name);
  email(title, 1, 0, true, 0, true);
  a()->SetCarbonCopyEnabled(save_allow_cc);
  if (a()->user()->email_sent() > 0) {
    ssm(1) << "Check your mailbox.  Someone forgot their password again!";
  }
}

static void CheckCallRestrictions() {
  if (a()->sess().user_num() > 0 && a()->user()->restrict_logon() &&
      date() == a()->user()->laston() &&
      a()->user()->ontoday() > 0) {
    bout.nl();
    bout.outstr("|#6Sorry, you can only logon once per day.\r\n");
    a()->Hangup();
  }
}

static void logon_guest() {
  a()->sess().SetUserOnline(true);
  bout.nl(2);
  input_ansistat();

  bout.printfile(GUEST_NOEXT);
  bout.pausescr();

  std::string userName, reason;
  auto count = 0;
  do {
    bout.outstr("\r\n|#5Enter your real name : ");
    userName = bin.input_upper(25);
    bout.outstr("\r\n|#5Purpose of your call?\r\n");
    reason = bin.input_text(79);
    if (!userName.empty() && !reason.empty()) {
      break;
    }
    count++;
  } while (count < 3);

  if (count >= 3) {
    bout.printfile(REJECT_NOEXT);
    ssm(1) << "Guest Account failed to enter name and purpose";
    a()->Hangup();
  } else {
    ssm(1) << "Guest Account accessed by " << userName << " on " << times() << " for" << reason;
  }
}

void getuser() {
  // Reset the key timeout to 30 seconds while trying to log in a user.
  auto at_exit = finally([] { a()->sess().okmacro(true); bin.reset_key_timeout(); });

  a()->sess().okmacro(false);
  // TODO(rushfan): uncomment this
  //bout.set_logon_key_timeout();

  // Let's set this to 0 here since we don't have a user yet.
  a()->sess().user_num(0);
  a()->sess().set_current_user_sub_conf_num(0);
  a()->sess().set_current_user_dir_conf_num(0);
  a()->sess().effective_sl(a()->config()->newuser_sl());
  a()->user()->SetStatus(0);

  const auto& ip = bout.remoteIO()->remote_info().address;
  const auto& hostname = bout.remoteIO()->remote_info().address_name;
  if (!ip.empty()) {
    bout.print("Client Address: {} ({})\r\n", hostname, ip);
    bout.nl();
  }
  write_inst(INST_LOC_GETUSER, 0, INST_FLAGS_NONE);

  int count = 0;
  bool ok = false;

  const auto ans = GetAnsiStatusAndShowWelcomeScreen();
  auto first_time = true;
  do {
    std::string remote_username;
    std::string remote_password;
    if (first_time) {
      remote_username = ToStringUpperCase(bout.remoteIO()->remote_info().username);
      remote_password = ToStringUpperCase(bout.remoteIO()->remote_info().password);
      first_time = false;
    }
    const auto usernum = ShowLoginAndGetUserNumber(remote_username);
    if (usernum > 0) {
      a()->sess().user_num(static_cast<uint16_t>(usernum));
      a()->ReadCurrentUser();
      read_qscn(a()->sess().user_num(), a()->sess().qsc, false);
      if (const auto instance_number = user_online(a()->sess().user_num());
          instance_number && a()->user()->sl() < 255) {
        bout.print("\r\n|#6You are already online on instance {}!\r\n\n", instance_number.value());
        continue;
      }
      ok = true;
      if (a()->user()->guest_user()) {
        logon_guest();
      } else {
        a()->sess().effective_sl(a()->config()->newuser_sl());
        if (!VerifyPassword(remote_password)) {
          ok = false;
        }

        if ((a()->config()->sysconfig_flags() & sysconfig_free_phone) == 0 && IsPhoneRequired()) {
          if (!VerifyPhoneNumber()) {
            ok = false;
          }
        }
        if (a()->user()->sl() == 255 && a()->sess().incom() && ok) {
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
      bout.outstr("|#6Unknown user.\r\n");
      a()->sess().user_num(static_cast<uint16_t>(usernum));
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
    a()->Hangup();
  }
}

static void FixUserLinesAndColors() {
  if (a()->user()->GetNumExtended() > a()->max_extend_lines) {
    a()->user()->SetNumExtended(a()->max_extend_lines);
  }
  if (a()->user()->raw_color(8) == 0 || a()->user()->raw_color(9) == 0) {
    a()->user()->color(8, a()->newuser_colors[8]);
    a()->user()->color(9, a()->newuser_colors[9]);
  }

  // Query screen size if mismatch.
  if (detect_screensize()) {
    a()->WriteCurrentUser();
  }
}

static void UpdateUserStatsForLogin() {
  to_char_array(g_szLastLoginDate, date());
  if (a()->user()->laston() != g_szLastLoginDate) {
    ResetTodayUserStats(a()->user());
  }
  AddCallToday(a()->user());
  a()->set_current_user_sub_num(0);
  a()->SetNumMessagesReadThisLogon(0);
  const auto& udir = a()->udir;
  if (udir.size() > 1 && udir[0].subnum == 0 && udir[1].subnum > 0) {
    a()->set_current_user_dir_num(1);
  } else {
    a()->set_current_user_dir_num(0);
  }
  if (a()->sess().effective_sl() != 255 && !a()->user()->guest_user()) {
    a()->status_manager()->Run([](Status& s) {
      s.increment_caller_num();
      s.increment_calls_today();
    });
  }
}

static void PrintLogonFile() {
  if (!a()->sess().incom()) {
    return;
  }
  play_sdf(LOGON_NOEXT, false);
  if (!bout.printfile(LOGON_NOEXT)) {
    bout.pausescr();
  }
}

static void PrintUserSpecificFiles() {
  const auto* user = a()->user();  // not-owned
  bout.printfile(fmt::format("sl{}", user->sl()));
  bout.printfile(fmt::format("dsl{}", user->dsl()));

  const auto short_size = std::numeric_limits<uint16_t>::digits - 1;
  for (auto i=0; i < short_size; i++) {
    if (user->has_ar(1 << i)) {
      bout.printfile(fmt::format("ar{}", static_cast<char>('A' + i)));
    }
  }

  for (auto i=0; i < short_size; i++) {
    if (user->has_dar(1 << i)) {
      bout.printfile(fmt::format("dar{}", static_cast<char>('A' + i)));
    }
  }
}

static std::string CreateLastOnLogLine(const Status& status) {
  std::string log_line;
  if (a()->HasConfigFlag(OP_FLAGS_SHOW_CITY_ST) &&
    a()->config()->newuser_config().use_address_city_state != newuser_item_type_t::unused) {
    const std::string username_num = a()->user()->name_and_number();
    const std::string t = times();
    const std::string f = fulldate();
    log_line = fmt::sprintf(
      "|#1%-6ld %-25.25s %-5.5s %-5.5s %-15.15s %-2.2s %-3.3s %-8.8s %2d\r\n",
      status.caller_num(),
      username_num,
      t,
      f,
      a()->user()->city(),
      a()->user()->state(),
      a()->user()->country(),
      a()->GetCurrentSpeed(),
      a()->user()->ontoday());
  } else {
    const auto username_num = a()->user()->name_and_number();
    const auto t = times();
    const auto f = fulldate();
    log_line = fmt::sprintf("|#1%-6ld %-25.25s %-10.10s %-5.5s %-5.5s %-20.20s %2d\r\n",
                            status.caller_num(), username_num, a()->sess().current_language(),
                            t, f, a()->GetCurrentSpeed(), a()->user()->ontoday());
  }
  return log_line;
}


static void UpdateLastOnFile() {
  const auto laston_txt_filename = FilePath(a()->config()->gfilesdir(), LASTON_TXT);
  std::vector<std::string> lines;
  {
    TextFile laston_file(laston_txt_filename, "r");
    lines = laston_file.ReadFileIntoVector();
  }


  auto status = a()->status_manager()->get_status();
  {
    const auto username_num = a()->user()->name_and_number();
    auto t = times();
    auto f = fulldate();
    const auto sysop_log_line = fmt::sprintf("%ld: %s %s %s   %s - %d (%u)",
                                             status->caller_num(),
                                             username_num,
                                             t,
                                             f,
                                             a()->GetCurrentSpeed(),
                                             a()->user()->ontoday(),
                                             a()->sess().instance_number());
    sysoplog(false, "");
    sysoplog(false, stripcolors(sysop_log_line));
    sysoplog(false, "");
  }
  if (a()->sess().incom()) {
    const auto& remote_address = bout.remoteIO()->remote_info().address;
    if (!remote_address.empty()) {
      sysoplog(fmt::format("Remote IP: {}", remote_address));
    }
  }
  if (a()->sess().effective_sl() == 255 && !a()->sess().incom()) {
    return;
  }

  // add line to laston.txt. We keep 8 lines
  if (a()->sess().effective_sl() != 255) {
    TextFile lastonFile(laston_txt_filename, "w");
    if (lastonFile.IsOpen()) {
      auto it = lines.begin();
      // skip over any lines over 7
      while (lines.size() - std::distance(lines.begin(), it) >= 8) {
        ++it;
      }
      while (it != lines.end()) {
        lastonFile.WriteLine(*it++);
      }
      lastonFile.Write(CreateLastOnLogLine(*status));
      lastonFile.Close();
    }
  }
}

static std::string process_net_log_line(const std::string& line) {
  const auto date = StringTrim(line.substr(0, 8));
  const auto time = StringTrim(line.substr(9, 8));
  const auto node = StringTrim(line.substr(21, 5));
  const auto sent = StringTrim(line.substr(30, 5));
  const auto recd = StringTrim(line.substr(39, 5));
  const auto net_name = StringTrim(line.substr(64));

  const auto& net = a()->nets().at(net_name);
  auto b = BbsListNet::ReadBbsDataNet(net.dir);
  std::string bbs_node_name = "Unknown";
  if (auto o = b.node_config_for(to_number<int>(node))) {
    bbs_node_name = o.value().name;
  }

  const auto bbs_node_network = StrCat(node, ".", StringTrim(net.name));
  return fmt::format("|#1{:<11} {:<11} {:<7} {:<7} @{:<15} {}", date, time, sent, recd, bbs_node_network,
                     bbs_node_name);
}


static void DisplayLastNetInfo() {
  const auto net_log_filename = FilePath(a()->config()->gfilesdir(), NET_LOG);
  TextFile nlf(net_log_filename, "rt");
  if (!nlf) {
    return;
  }
  std::vector<std::string> lines;
  for (const auto& line : nlf.ReadLastLinesIntoVector(6)) {
    lines.emplace_back(process_net_log_line(line));
  }
  if (lines.empty()) {
    return;
  }
  bout.nl(2);
  bout.outstr("|#5                      Most Recent Network Activity|#0");
  bout.nl(2);
  bout.pl("|#2  Date        Time      Sent    Recd    Connection       BBS");
  const auto bar = okansi() ? static_cast<char>('\xCD') : '=';
  bout.print("|#7{}\r\n", std::string(79, bar));
  for (const auto& line : lines) {
    bout.pl(line);
    if (bin.checka()) {
      break;
    }
  }
  bout.nl(2);
  bout.pausescr();
}

static void CheckAndUpdateUserInfo() {
  const auto& nc = a()->config()->newuser_config();

  if (nc.use_birthday == newuser_item_type_t::required && a()->user()->birthday_year() == 0) {
    bout.outstr("\r\nPlease enter the following information:\r\n");
    do {
      bout.nl();
      input_age(a()->user());
      bout.nl();
      bout.print("{} -- Correct? ", a()->user()->birthday_mmddyy());
      if (!bin.yesno()) {
        a()->user()->birthday_mdy(0, 0, 0);
      }
    } while (a()->user()->birthday_year() == 0);
  }

  if (nc.use_real_name == newuser_item_type_t::required && a()->user()->real_name().empty()) {
    input_realname();
  }
  if (nc.use_address_country == newuser_item_type_t::required && a()->user()->country().empty()) {
    input_country();
  }
  if (nc.use_address_zipcode == newuser_item_type_t::required && a()->user()->zip_code().empty()) {
    input_zipcode();
  }
  if (nc.use_address_street == newuser_item_type_t::required && a()->user()->street().empty()) {
    input_street();
  }
  if (nc.use_address_city_state == newuser_item_type_t::required && a()->user()->city().empty()) {
    input_city();
  }
  if (nc.use_address_city_state == newuser_item_type_t::required && a()->user()->state().empty()) {
    input_state();
  }
  if (nc.use_data_phone == newuser_item_type_t::required && a()->user()->data_phone().empty()) {
    input_dataphone();
  }
  if (nc.use_computer_type == newuser_item_type_t::required && a()->user()->computer_type() == -1) {
    input_comptype();
  }
}

static void DisplayUserLoginInformation() {
  bout.nl();

  const auto username_num = a()->user()->name_and_number();
  bout.print("|#9Name/Handle|#0....... |#2{}\r\n", username_num);
  bout.outstr("|#9Internet Address|#0.. |#2");
  if (check_inet_addr(a()->user()->email_address())) {
    bout.pl(a()->user()->email_address());
  } else {
    bout.outstr("None.\r\n");
  }
  if (const auto la = a()->user()->last_address(); !la.empty()) {
    bout.print("|#9Last IP Address|#0... |#2{}\r\n", la.to_string());
  }

  bout.print("|#9Time allowed on|#0... |#2{}\r\n", (nsl() + 30) / SECONDS_PER_MINUTE);
  if (a()->user()->illegal_logons() > 0) {
    bout.print("|#9Illegal logons|#0.... |#2{}\r\n", a()->user()->illegal_logons());
  }
  if (a()->user()->email_waiting() > 0) {
    bout.print("|#9Mail waiting|#0...... |#2{}\r\n", a()->user()->email_waiting());
  }
  if (a()->user()->ontoday() == 1) {
    bout.print("|#9Date last on|#0...... |#2{}\r\n", a()->user()->laston());
  } else {
    bout.print("|#9Times on today|#0.... |#2{}\r\n", a()->user()->ontoday());
  }
  bout.print("|#9Sysop currently|#0... |#2{}\r\n", (sysop2() ? "Available" : "NOT Available"));
  bout.print("|#9System is|#0......... |#2WWIV {}\r\n", full_version());

  /////////////////////////////////////////////////////////////////////////
  a()->status_manager()->reload_status();
  const auto screen_width = a()->user()->screen_width() - 19;
  int width = 0;
  bout.outstr("|#9Networks|#0.......... ");
  for (const auto& n : a()->nets().networks()) {
    const auto s = to_string(n);
    const auto len = size_without_colors(s);
    if (width + len >= screen_width) {
      bout.nl();
      bout.outstr("|#0.................. ");
      width = 0;
    }
    bout.outstr(s);
    width += len;
  }
  bout.nl();

  bout.print("|#9Host OS|#0........... |#2{}\r\n", os_version_string());
  bout.print("|#9Instance|#0.......... |#2{}\r\n\r\n", a()->sess().instance_number());
  if (a()->user()->forward_usernum()) {
    if (a()->user()->forward_systemnum() != 0) {
      set_net_num(a()->user()->forward_netnum());
      if (!valid_system(a()->user()->forward_systemnum())) {
        a()->user()->clear_mailbox_forward();
        bout.outstr("Forwarded to unknown system; forwarding reset.\r\n");
      } else {
        bout.outstr("Mail set to be forwarded to ");
        if (wwiv::stl::ssize(a()->nets()) > 1) {
          bout.print("#{} @{}.{}.\r\n", a()->user()->forward_usernum(),
                     a()->user()->forward_systemnum(), a()->network_name());
        } else {
          bout.print("#{} @{}.\r\n", a()->user()->forward_usernum(),
                     a()->user()->forward_systemnum());
        }
      }
    } else {
      if (a()->user()->mailbox_closed()) {
        bout.outstr("Your mailbox is closed.\r\n\n");
      } else {
        bout.print("Mail set to be forwarded to #{}\r\n", a()->user()->forward_usernum());
      }
    }
  } else if (a()->user()->forward_systemnum() != 0) {
    const auto internet_addr = read_inet_addr(a()->sess().user_num());
    bout.print("Mail forwarded to Internet {}.\r\n", internet_addr);
  }
  if (a()->sess().IsTimeOnlineLimited()) {
    bout.outstr("\r\n|#3Your on-line time is limited by an external event.\r\n\n");
  }
}

static void LoginCheckForNewMail() {
  bout.outstr("|#9Scanning for new mail... ");
  if (a()->user()->email_waiting() > 0) {
    const auto messages = check_new_mail(a()->sess().user_num());
    if (messages) {
      bout.print("|#9You have |#2{}|#9 new message(s).\r\n\r\n|#9Read your mail now? ", messages);
      if (bin.noyes()) {
        readmail(true);
      }
    } else {
      bout.print("|#9You have |#2{}|#9 old message(s) in your mailbox.\r\n",
                 a()->user()->email_waiting());
    }
  } else {
    bout.outstr(" |#9No mail found.\r\n");
  }
}

static std::vector<bool> read_voting() {
  DataFile<votingrec> file(FilePath(a()->config()->datadir(), VOTING_DAT));
  std::vector<bool> questused(20);
  if (!file) {
    return questused;
  }
  std::vector<votingrec> votes;
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
  auto questused = read_voting();
  if (!a()->user()->restrict_vote() && a()->sess().effective_sl() >= a()->config()->validated_sl()) {
    for (int i = 0; i < 20; i++) {
      if (questused[i] && a()->user()->votes(i) == 0) {
        bout.nl();
        bout.outstr("|#9You haven't voted yet.\r\n");
        return;
      }
    }
  }
}

// Updates the User::last_address field since we've already displayed
// it to the user from the last session.
static void UpdateLastAddress() {
  if (!a()->sess().incom()) {
    return;
  }
  const auto remote_address = bout.remoteIO()->remote_info().address;
  if (remote_address.empty()) {
    return;
  }
  if (auto ipaddr = ip_address::from_string(remote_address)) {
    a()->user()->last_address(ipaddr.value());
  }
}

void logon() {
  if (a()->sess().user_num() < 1) {
    LOG(ERROR) << "Tried to logon user number < 1";
    a()->Hangup();
    return;
  }
  a()->sess().SetUserOnline(true);
  write_inst(INST_LOC_LOGON, 0, INST_FLAGS_NONE);
  bout.ResetColors();

  FixUserLinesAndColors();
  UpdateUserStatsForLogin();

  PrintLogonFile();
  LastCallers();
  UpdateLastOnFile();

  if (a()->config()->toggles().lastnet_at_logon) {
    DisplayLastNetInfo();  // Morgul Added
  }
  PrintUserSpecificFiles();

  read_automessage();
  a()->sess().SetLogonTime();
  a()->UpdateTopScreen();
  bout.nl(2);
  bout.pausescr();
  if (!a()->logon_cmd.empty()) {
    bout.nl();
    wwiv::bbs::CommandLine cl(a()->logon_cmd);
    cl.args(create_chain_file());
    ExecuteExternalProgram(cl, a()->spawn_option(SPAWNOPT_LOGON));
    bout.nl(2);
  }

  DisplayUserLoginInformation();
  UpdateLastAddress();

  CheckAndUpdateUserInfo();

  a()->UpdateTopScreen();
  a()->read_subs();
  rsm(a()->sess().user_num(), a()->user(), true);

  LoginCheckForNewMail();

  if (a()->user()->nscan_daten()) {
    a()->sess().nscandate(a()->user()->nscan_daten());
  } else {
    a()->sess().nscandate(a()->user()->last_daten());
  }
  a()->batch().clear();

  CheckUserForVotingBooth();

  if ((a()->sess().incom() || sysop1()) && a()->user()->sl() < 255) {
    broadcast(fmt::format("{} Just logged on!", a()->user()->name()));
  }
  setiia(std::chrono::seconds(5));

  // New Message Scan
  auto done_newscan_all = false;
  if (a()->IsNewScanAtLogin()) {
    bout.outstr("\r\n|#5Scan All Message Areas For New Messages? ");
    if (bin.yesno()) {
      wwiv::bbs::menus::NewMsgsAllConfs();
      done_newscan_all = true;
    }
  }

  // Handle case of first conf with no subs avail
  if (a()->usub.empty() && okconf(a()->user())) {
    for (auto confnum = 0; confnum < size_int(a()->uconfsub) && a()->usub.empty(); confnum++) {
      setuconf(ConferenceType::CONF_SUBS, confnum, -1);
      a()->sess().set_current_user_sub_conf_num(confnum);
    }
  }

  if (a()->HasConfigFlag(OP_FLAGS_USE_FORCESCAN) && !done_newscan_all) {
    auto nextsub = false;
    if (a()->user()->sl() < 255) {
      a()->sess().forcescansub(true);
      qscan(a()->GetForcedReadSubNumber(), nextsub);
      a()->sess().forcescansub(false);
    } else {
      qscan(a()->GetForcedReadSubNumber(), nextsub);
    }
  }
  CleanUserInfo();
}

void logoff() {
  VLOG(1) << "logoff()";
  mailrec m{};

  if (a()->sess().incom()) {
    play_sdf(LOGOFF_NOEXT, false);
  }

  if (a()->sess().user_num() > 0) {
    if ((a()->sess().incom() || sysop1()) && a()->user()->sl() < 255) {
      broadcast(fmt::format("{} Just logged off!", a()->user()->name()));
    }
  }
  setiia(std::chrono::seconds(5));
  if (!bout.remoteIO()->disconnect()) {
    LOG(WARNING) << "remoteIO->disconnect() returned false.";
  }
  // Don't need hangup here, but *do* want to ensure that a()->sess().hangup() is true.
  a()->sess().hangup(true);
  VLOG(1) << "Setting a()->sess().hangup()=true in logoff";
  if (a()->sess().user_num() < 1) {
    return;
  }

  std::string text = "  Logged Off At ";
  text += times();
  if (a()->sess().effective_sl() != 255 || a()->sess().incom()) {
    sysoplog(false, "");
    sysoplog(false, stripcolors(text));
  }
  a()->user()->last_bps(a()->modem_speed_);

  // put this back here where it belongs... (not sure why it te
  a()->user()->laston(g_szLastLoginDate);

  a()->user()->illegal_logons(0);
  const auto seconds_used_duration = duration_cast<seconds>(
      std::chrono::system_clock::now() - a()->sess().system_logon_time() - a()->extratimecall());
  a()->user()->add_timeon(seconds_used_duration);
  a()->user()->add_timeon_today(seconds_used_duration);

  a()->status_manager()->Run([=](Status& s) {
    const int active_today = s.active_today_minutes();
    const auto minutes_used_now = std::chrono::duration_cast<std::chrono::minutes>(seconds_used_duration).count();
    s.active_today_minutes(active_today + minutes_used_now);
  });

  if (a()->sess().scanned_files()) {
    a()->user()->nscan_daten(a()->user()->last_daten());
  }
  a()->user()->last_daten(daten_t_now());
  const auto used_this_session = (std::chrono::system_clock::now() - a()->sess().system_logon_time());
  const auto min_used = std::chrono::duration_cast<std::chrono::minutes>(used_this_session);
  sysoplog(false, fmt::format("Read: {}   Time on: {} minutes.", a()->GetNumMessagesReadThisLogon(),
                              min_used.count()));
  {
    if (auto file_email(OpenEmailFile(true)); file_email->IsOpen()) {
      a()->user()->email_waiting(0);
      const auto num_records = static_cast<int>(file_email->length() / sizeof(mailrec));
      auto r = 0;
      auto w = 0;
      while (r < num_records) {
        file_email->Seek(static_cast<long>(sizeof(mailrec)) * static_cast<long>(r), File::Whence::begin);
        file_email->Read(&m, sizeof(mailrec));
        if (m.tosys != 0 || m.touser != 0) {
          if (m.tosys == 0 && m.touser == a()->sess().user_num()) {
            if (a()->user()->email_waiting() != 255) {
              a()->user()->email_waiting(a()->user()->email_waiting() + 1);
            }
          }
          if (r != w) {
            file_email->Seek(static_cast<long>(sizeof(mailrec)) * static_cast<long>(w), File::Whence::begin);
            file_email->Write(&m, sizeof(mailrec));
          }
          ++w;
        }
        ++r;
      }
      if (r != w) {
        m.tosys = 0;
        m.touser = 0;
        for (auto w1 = w; w1 < r; w1++) {
          file_email->Seek(static_cast<long>(sizeof(mailrec)) * static_cast<long>(w1), File::Whence::begin);
          file_email->Write(&m, sizeof(mailrec));
        }
      }
      file_email->Close();
      file_email->set_length(static_cast<long>(sizeof(mailrec)) * static_cast<long>(w));
      a()->status_manager()->Run([](Status& s) {
        s.increment_filechanged(Status::file_change_email);
      });
    }
  }
  if (received_short_message()) {
    File smwFile(FilePath(a()->config()->datadir(), SMW_DAT));
    if (smwFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      const auto num_records = static_cast<int>(smwFile.length() / sizeof(shortmsgrec));
      auto r = 0;
      auto w = 0;
      while (r < num_records) {
        shortmsgrec sm{};
        smwFile.Seek(r * sizeof(shortmsgrec), File::Whence::begin);
        smwFile.Read(&sm, sizeof(shortmsgrec));
        if (sm.tosys != 0 || sm.touser != 0) {
          if (sm.tosys == 0 && sm.touser == a()->sess().user_num()) {
            a()->user()->set_flag(User::SMW);
          }
          if (r != w) {
            smwFile.Seek(w * sizeof(shortmsgrec), File::Whence::begin);
            smwFile.Write(&sm, sizeof(shortmsgrec));
          }
          ++w;
        }
        ++r;
      }
      smwFile.Close();
      smwFile.set_length(w * sizeof(shortmsgrec));
    }
  }
  a()->WriteCurrentUser();
  write_qscn(a()->sess().user_num(), a()->sess().qsc, false);
}

