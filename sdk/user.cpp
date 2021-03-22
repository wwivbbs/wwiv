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
#include "sdk/user.h"

#include "core/clock.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/names.h"
#include "sdk/wwivcolors.h"
#include <chrono>
#include <cstring>
#include <random>

using namespace std::chrono;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::sdk {

constexpr int HOTKEYS_ON = 0;
constexpr int HOTKEYS_OFF = 1;

User::User() { ZeroUserData(); }

User::User(const User& w) {
  memcpy(&data, &w.data, sizeof(userrec));
  user_number_ = w.user_number_;
}

User::User(User&& u) noexcept {
  memmove(&data, &u.data, sizeof(userrec));
  user_number_ = u.user_number_;
  u.user_number_ = -1;
}

User::User(const userrec& rhs, int user_number) {
  memcpy(&data, &rhs, sizeof(userrec));
  user_number_ = user_number;
}

User& User::operator=(const User& rhs) {
  if (this == &rhs) {
    return *this;
  }
  memcpy(&data, &rhs.data, sizeof(userrec));
  user_number_ = rhs.user_number_;
  return *this;
}

User& User::operator=(User&& rhs) noexcept {
  if (this == &rhs) {
    return *this;
  }
  memmove(&data, &rhs.data, sizeof(userrec));
  user_number_ = rhs.user_number_;
  rhs.user_number_ = -1;
  return *this;
}

void User::FixUp() {
  data.name[sizeof(data.name) - 1] = '\0';
  data.realname[sizeof(data.realname) - 1] = '\0';
  data.callsign[sizeof(data.callsign) - 1] = '\0';
  data.phone[sizeof(data.phone) - 1] = '\0';
  data.dataphone[sizeof(data.dataphone) - 1] = '\0';
  data.street[sizeof(data.street) - 1] = '\0';
  data.city[sizeof(data.city) - 1] = '\0';
  data.state[sizeof(data.state) - 1] = '\0';
  data.country[sizeof(data.country) - 1] = '\0';
  data.zipcode[sizeof(data.zipcode) - 1] = '\0';
  data.pw[sizeof(data.pw) - 1] = '\0';
  data.laston[sizeof(data.laston) - 1] = '\0';
  data.firston[sizeof(data.firston) - 1] = '\0';
  data.firston[2] = '/';
  data.firston[5] = '/';
  data.note[sizeof(data.note) - 1] = '\0';
  data.macros[0][sizeof(data.macros[0]) - 1] = '\0';
  data.macros[1][sizeof(data.macros[1]) - 1] = '\0';
  data.macros[2][sizeof(data.macros[2]) - 1] = '\0';
}

void User::ZeroUserData() { memset(&data, 0, sizeof(userrec)); }

bool User::guest_user() const {
  return iequals(name(), "GUEST");
}

bool User::CreateRandomPassword() {
  std::string chars("ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890");

  std::random_device rd;
  std::mt19937 e{rd()};
  std::uniform_int_distribution<int> dist(0, wwiv::stl::size_int(chars) - 1);
  std::string p;
  for (auto i = 0; i < 6; i++) {
    p.push_back(chars[dist(e)]);
  }
  password(p);
  return true;
}

bool User::hotkeys() const { return data.hot_keys != HOTKEYS_OFF; }

void User::set_hotkeys(bool enabled) { data.hot_keys = enabled ? HOTKEYS_ON : HOTKEYS_OFF; }

std::string User::menu_set() const { return data.menu_set; }

void User::set_menu_set(const std::string& menu_set) { to_char_array(data.menu_set, menu_set); }

bool User::asv(const validation_config_t& v) {
  if (sl() < v.sl) {
    sl(v.sl);
  }
  if (dsl() < v.dsl) {
    dsl(v.dsl);
  }
  const auto a = ar_int();
  ar_int(a | v.ar);

  const auto d = dar_int();
  dar_int(d | v.dar);

  const auto r = restriction();
  restriction(v.restrict & r);

  return true;
}

// static
bool User::CreateNewUserRecord(User* u, uint8_t sl, uint8_t dsl, uint16_t restr, float gold,
                               const std::vector<uint8_t>& colors,
                               const std::vector<uint8_t>& bwcolors) {
  u->ZeroUserData();

  const auto date = DateTime::now().to_string("%m/%d/%y");
  u->firston(date);
  u->laston("Never.");
  u->macro(0, "Wow! This is a GREAT BBS!");
  u->macro(1, "Guess you forgot to define this one....");
  u->macro(2, "User = Monkey + Keyboard");

  u->SetScreenLines(25);
  u->SetScreenChars(80);

  u->sl(sl);
  u->dsl(dsl);

  u->data.ontoday = 1;
  u->data.daten = 0;
  u->restriction(restr);

  u->set_flag(User::pauseOnPage);
  u->clear_flag(User::conference);
  u->clear_flag(User::nscanFileSystem);
  u->gold(gold);
  // Set to N so the BBS will prompt.
  u->gender('N');

  for (int i = 0; i <= 9; i++) {
    u->color(i, colors[i]);
    u->bwcolor(i, bwcolors[i]);
  }

  u->email_address("");

  // Set default menu set abd list plus colors.
  to_char_array(u->data.menu_set, "wwiv");
  u->data.hot_keys = HOTKEYS_ON;
  u->data.lp_options = cfl_fname | cfl_extension | cfl_dloads | cfl_kbytes | cfl_description;
  memset(u->data.lp_colors, static_cast<uint8_t>(Color::CYAN), sizeof(u->data.lp_colors));
  u->data.lp_colors[0] = static_cast<uint8_t>(Color::LIGHTGREEN);
  u->data.lp_colors[1] = static_cast<uint8_t>(Color::LIGHTGREEN);
  u->data.lp_colors[2] = static_cast<uint8_t>(Color::CYAN);
  u->data.lp_colors[3] = static_cast<uint8_t>(Color::CYAN);
  u->data.lp_colors[4] = static_cast<uint8_t>(Color::LIGHTCYAN);
  u->data.lp_colors[5] = static_cast<uint8_t>(Color::LIGHTCYAN);
  u->data.lp_colors[6] = static_cast<uint8_t>(Color::CYAN);
  u->data.lp_colors[7] = static_cast<uint8_t>(Color::CYAN);
  u->data.lp_colors[8] = static_cast<uint8_t>(Color::CYAN);
  u->data.lp_colors[9] = static_cast<uint8_t>(Color::CYAN);
  u->data.lp_colors[10] = static_cast<uint8_t>(Color::LIGHTCYAN);

  return true;
}

seconds User::add_extratime(duration<double> extra) {
  data.extratime += duration_cast<seconds>(extra).count();
  return seconds(static_cast<int64_t>(data.extratime));
}

seconds User::subtract_extratime(duration<double> extra) {
  data.extratime -= duration_cast<seconds>(extra).count();
  return seconds(static_cast<int64_t>(data.extratime));
}

duration<double> User::extra_time() const noexcept{
  const auto extratime_seconds = static_cast<int64_t>(data.extratime);
  return seconds(extratime_seconds);
}

seconds User::add_timeon(duration<double> d) {
  data.timeon += duration_cast<seconds>(d).count();
  return seconds(static_cast<int64_t>(data.timeon));
}

seconds User::add_timeon_today(duration<double> d) {
  data.timeontoday += duration_cast<seconds>(d).count();
  return seconds(static_cast<int64_t>(data.timeontoday));
}

seconds User::timeon() const {
  const auto secs_used = static_cast<int64_t>(data.timeon);
  return seconds(secs_used);
}

seconds User::timeontoday() const {
  const auto secs_used = static_cast<int64_t>(data.timeontoday);
  return seconds(secs_used);
}

std::string User::name_and_number() const {
  return fmt::format("{} #{}", properize(name()), user_number_);
}

int User::usernum() const noexcept {
  return user_number_;
}

int User::age() const { 
  if (data.year == 0 && data.month == 0 && data.day == 0) {
    // If the birthday is unset, then the age is also unset.
    return 0;
  }
  SystemClock clock{};
  return years_old(this, clock);
}

[[nodiscard]] DateTime User::birthday_dt() const { 
  tm tm{};
  tm.tm_year = data.year;
  tm.tm_mon = data.month - 1;
  tm.tm_mday = data.day;
  tm.tm_hour = 12;
  tm.tm_isdst = 0;
  return DateTime::from_tm(&tm);
}

[[nodiscard]] std::string User::birthday_mmddyy() const {
  const auto s = fmt::format("{:02}/{:02}/{:02}", data.month, data.day, data.year);
  return s;
}

std::string User::mailbox_state() const {
  if (forward_systemnum() == 0 &&
      forward_usernum() == 0) {
    return "Normal";
  }
  if (forward_systemnum() != 0) {
    if (IsMailboxForwarded()) {
      return fmt::format("Forward to #{} @{}", forward_usernum(), forward_systemnum());
    }
    return StrCat("Forwarded to: ", email_address());
  }

  if (IsMailboxClosed()) {
    return "Closed";
  }

  return StrCat("Forward to: User #", forward_usernum());
}

void ResetTodayUserStats(User* u) {
  u->data.ontoday = 0;
  u->data.timeontoday = 0;
  u->data.extratime = 0.0;
  u->posts_today(0);
  u->email_today(0);
  u->feedback_today(0);
}

int AddCallToday(User* u) { 
  u->increment_logons(); 
  return ++u->data.ontoday;
}

void User::birthday_mdy(int m, int d, int y) {
  data.month = static_cast<uint8_t>(m);
  data.day = static_cast<uint8_t>(d);
  if (y == 0) {
    data.year = 0;
  } else {
    data.year = static_cast<uint8_t>(y - 1900);
  }
}

int years_old(const User* u, Clock& clock) {
  return core::years_old(u->birthday_month(), u->birthday_mday(), 
                         u->birthday_year(), clock);
}

float User::ratio() const {
  if (dk() == 0) {
    return 99.999f;
  }
  const auto r = static_cast<float>(uk()) / static_cast<float>(dk());
  return std::max<float>(99.998f, r);
}

} // namespace wwiv::sdk
