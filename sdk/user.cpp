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
#include "sdk/user.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <memory>
#include "core/strings.h"
#include "core/file.h"
#include "sdk/names.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"
#include "sdk/wwivcolors.h"

using namespace std::chrono;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

User::User() {
  ZeroUserData();
}

User::~User() {}

User::User(const User& w) {
  memcpy(&data, &w.data, sizeof(userrec));
}

User& User::operator=(const User& rhs) {
  if (this == &rhs) {
    return *this;
  }
  memcpy(&data, &rhs.data, sizeof(userrec));
  return *this;
}

void User::FixUp() {
  data.name[sizeof(data.name)-1] = '\0';
  data.realname[sizeof(data.realname)-1]  = '\0';
  data.callsign[sizeof(data.callsign)-1]  = '\0';
  data.phone[sizeof(data.phone)-1] = '\0';
  data.dataphone[sizeof(data.dataphone)-1]  = '\0';
  data.street[sizeof(data.street)-1] = '\0';
  data.city[sizeof(data.city)-1] = '\0';
  data.state[sizeof(data.state)-1] = '\0';
  data.country[sizeof(data.country)-1] = '\0';
  data.zipcode[sizeof(data.zipcode)-1] = '\0';
  data.pw[sizeof(data.pw)-1] = '\0';
  data.laston[sizeof(data.laston)-1] = '\0';
  data.firston[sizeof(data.firston)-1] = '\0';
  data.firston[2] = '/';
  data.firston[5] = '/';
  data.note[sizeof(data.note)-1] = '\0';
  data.macros[0][sizeof(data.macros[0])-1] = '\0';
  data.macros[1][sizeof(data.macros[1])-1] = '\0';
  data.macros[2][sizeof(data.macros[2])-1] = '\0';
}

void User::ZeroUserData() {
  memset(&data, 0, sizeof(userrec));
}

bool User::CreateRandomPassword() {
  std::string password;
  for (int i = 0; i < 6; i++) {
    char ch = static_cast<char>(rand() % 36);
    if (ch < 10) {
      ch += '0';
    }
    else {
      ch += 'A' - 10;
    }
    password.push_back(ch);
  }
  SetPassword(password);
  return true;
}

// static 
bool User::CreateNewUserRecord(User* u,
  uint8_t sl, uint8_t dsl, uint16_t restr, float gold,
  const std::vector<uint8_t>& colors, const std::vector<uint8_t>& bwcolors) {
  u->ZeroUserData();

  const std::string date = daten_to_mmddyy(system_clock::to_time_t(system_clock::now()));
  u->SetFirstOn(date.c_str());
  u->SetLastOn("Never.");
  u->SetMacro(0, "Wow! This is a GREAT BBS!");
  u->SetMacro(1, "Guess you forgot to define this one....");
  u->SetMacro(2, "User = Monkey + Keyboard");

  u->SetScreenLines(25);
  u->SetScreenChars(80);

  u->SetSl(sl);
  u->SetDsl(dsl);

  u->SetTimesOnToday(1);
  u->SetLastOnDateNumber(0);
  u->SetRestriction(restr);

  u->SetStatusFlag(User::pauseOnPage);
  u->ClearStatusFlag(User::conference);
  u->ClearStatusFlag(User::nscanFileSystem);
  u->SetGold(gold);

  for (int i = 0; i <= 9; i++) {
    u->SetColor(i, colors[i]);
    u->SetBWColor(i, bwcolors[i]);
  }

  u->SetEmailAddress("");

  // Set default menu set abd listplus colors.
  strcpy(u->data.szMenuSet, "wwiv");
  u->data.cHotKeys = 0;
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

// TODO(rushfan): May be able to templatize these.
seconds User::add_extratime(duration<double> extra) {
  auto extratime_seconds = static_cast<int64_t>(GetExtraTime());
  extratime_seconds += duration_cast<seconds>(extra).count();
  SetExtraTime(static_cast<float>(extratime_seconds));
  return seconds(extratime_seconds);
}

seconds User::add_timeon(duration<double> d) {
  auto timeon = static_cast<int64_t>(GetTimeOn());
  timeon += duration_cast<seconds>(d).count();
  SetTimeOn(static_cast<float>(timeon));
  return seconds(timeon);
}

seconds User::add_timeon_today(duration<double> d) {
  auto t = static_cast<int64_t>(GetTimeOnToday());
  t += duration_cast<seconds>(d).count();
  SetTimeOnToday(static_cast<float>(t));
  return seconds(t);
}

seconds User::timeon() const {
  auto secs_used = static_cast<int64_t>(GetTimeOn());
  return seconds(secs_used);
}

}  // namespace sdk
}  // namespace wwiv
