/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2020, WWIV Software Services           */
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
#include "wwivconfig/system_info.h"

#include "core/strings.h"
#include "fmt/printf.h"
#include "localui/input.h"
#include "localui/curses_win.h"
#include "sdk/vardec.h"
#include "wwivconfig/utility.h"
#include <cstdint>
#include <memory>
#include <string>

using std::unique_ptr;
using std::string;
using namespace wwiv::strings;

static string print_time(uint16_t t) {
  return fmt::sprintf("%02d:%02d", t / 60, t % 60);
}

static uint16_t get_time(const string& s) {
  if (s[2] != ':') {
    return std::numeric_limits<uint16_t>::max();
  }

  const auto h = to_number<uint16_t>(s);
  const auto minutes = s.substr(3);
  const auto m = to_number<uint16_t>(minutes);
  if (h > 23 || m > 59) {
    return std::numeric_limits<uint16_t>::max();
  }
  return static_cast<uint16_t>(h * 60 + m);
}

static const int MAX_TIME_EDIT_LEN = 5;

class TimeEditItem final : public EditItem<uint16_t*> {
public:
  TimeEditItem(int x, int y, uint16_t* data) : EditItem<uint16_t*>(x, y, 5, data) {}
  ~TimeEditItem() override = default;

  EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    auto s = print_time(*this->data_);
    const auto return_code = editline(window, &s, MAX_TIME_EDIT_LEN + 1, EditLineMode::ALL, "");
    *this->data_ = get_time(s);
    return return_code;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    const auto s = print_time(*this->data_);
    DefaultDisplayString(window, s);
  }
};

class Float53EditItem final : public EditItem<float*> {
public:
  Float53EditItem(int x, int y, float* data) : EditItem<float*>(x, y, 5, data) {}
  ~Float53EditItem() override = default;

  EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
   
    // passing *this->data_ to String Printf is causing a bus error
    // on GCC/ARM (RPI).  See http://stackoverflow.com/questions/26158510
    const auto d = *this->data_;
    auto s = fmt::sprintf("%5.3f", d);
    const auto return_code = editline(window, &s, 5 + 1, EditLineMode::NUM_ONLY, "");

    char* e;
    auto f = strtof(s.c_str(), &e);
    if (f > 9.999 || f < 0.001) {
      f = 0.0;
    }
    *this->data_ = f;
    return return_code;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    // passing *this->data_ to fmt::sprintf is causing a bus error
    // on GCC/ARM (RPI).  See http://stackoverflow.com/questions/26158510
    const auto d = *this->data_;
    const auto s = fmt::sprintf("%5.3f", d);
    DefaultDisplayString(window, s);
  }
};

void sysinfo1(wwiv::sdk::Config& config) {
  auto cfg = *config.config();
  statusrec_t statusrec{};
  read_status(config.datadir(), statusrec);

  if (statusrec.callernum != 65535) {
    statusrec.callernum1 = static_cast<long>(statusrec.callernum);
    statusrec.callernum = 65535;
    save_status(config.datadir(), statusrec);
  }

  static constexpr auto LABEL1_POSITION = 2;
  static constexpr auto LABEL1_WIDTH = 18;
  static constexpr auto LABEL2_WIDTH = 14;
  static constexpr auto COL1_POSITION = LABEL1_POSITION + LABEL1_WIDTH + 1;
  static constexpr auto LABEL2_POSITION = COL1_POSITION + 17;
  static constexpr auto COL2_POSITION = LABEL2_POSITION + LABEL2_WIDTH + 1;

  auto closed_system = cfg.closedsystem > 0;
  auto gold = static_cast<int>(cfg.newusergold);

  auto y = 1;
  EditItems items{};
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "System name:"),
            new StringEditItem<char*>(COL1_POSITION, y, 50, cfg.systemname, EditLineMode::ALL),
    "Name of the BBS System");
  ++y;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "Sysop name:"),
            new StringEditItem<char*>(COL1_POSITION, y, 50, cfg.sysopname, EditLineMode::ALL),
    "The name (or handle/alias) of the System Operator");
  ++y;
  items.add(
      new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "System phone:"),
      new StringEditItem<char*>(COL1_POSITION, y, 12, cfg.systemphone, EditLineMode::UPPER_ONLY),
    "Phone number for the BBS (if you have one)");
  ++y;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "System PW:"),
            new StringEditItem<char*>(COL1_POSITION, y, 20, cfg.systempw, EditLineMode::UPPER_ONLY),
            "Password needed to log in as sysop");
  y+=2;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "Closed system?"),
            new BooleanEditItem(COL1_POSITION, y, &closed_system),
    "Are users allowed to establish accounts on this BBS");
  items.add(
      new Label(LABEL2_POSITION, y, LABEL2_WIDTH, "New User PW:"),
      new StringEditItem<char*>(COL2_POSITION, y, 20, cfg.newuserpw, EditLineMode::UPPER_ONLY),
    "Optional password users must provide in order create an account");
  ++y;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "New User SL:"),
            new NumberEditItem<uint8_t>(COL1_POSITION, y, &cfg.newusersl),
    "The security level that all new users are given. The default is 10");
  items.add(new Label(LABEL2_POSITION, y, LABEL2_WIDTH, "New User DSL:"),
            new NumberEditItem<uint8_t>(COL2_POSITION, y, &cfg.newuserdsl),
        "The download security level that all new users are given. The default is 10");
  ++y;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "New User restrict:"),
            new RestrictionsEditItem(COL1_POSITION, y, &cfg.newuser_restrict),
    "Restrictions given to new users from certain features of the system.");
  items.add(new Label(LABEL2_POSITION, y, LABEL2_WIDTH, "New User gold:"),
            new NumberEditItem<int>(COL2_POSITION, y, &gold),
    "The default amount of gold given to new users");
  y+=2;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "Sysop time: from:"),
            new TimeEditItem(COL1_POSITION, y, &cfg.sysoplowtime),
    "Set the time limits that the sysop is available for chat");
  items.add(new Label(LABEL2_POSITION, y, LABEL2_WIDTH, "to:"),
            new TimeEditItem(COL2_POSITION, y, &cfg.sysophightime),
    "Set the time limits that the sysop is available for chat");
  ++y;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "Ratios    :  U/D:"),
            new Float53EditItem(COL1_POSITION, y, &cfg.req_ratio),
    "Optional required ratio of (uploads/downloads) for downloading files");
  items.add(new Label(LABEL2_POSITION, y, LABEL2_WIDTH, "Post/Call:"),
            new Float53EditItem(COL2_POSITION, y, &cfg.post_call_ratio),
    "Optional required ratio of (uploads/downloads) for downloading files");
  ++y;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "Max waiting:"),
            new NumberEditItem<uint8_t>(COL1_POSITION, y, &cfg.maxwaiting),
    "Maximum number of emails allowed for a user");
  items.add(new Label(LABEL2_POSITION, y, LABEL2_WIDTH, "Max users:"),
            new NumberEditItem<uint16_t>(COL2_POSITION, y, &cfg.maxusers),
    "The maximum number of users that can be on the system");
  ++y;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "Total Calls:"),
            new NumberEditItem<uint32_t>(COL1_POSITION, y, &statusrec.callernum1),
    "Caller number for the last call to the BBS.");
  items.add(new Label(LABEL2_POSITION, y, LABEL2_WIDTH, "Days active:"),
            new NumberEditItem<uint16_t>(COL2_POSITION, y, &statusrec.days),
    "Number of days the BBS has been active");
  ++y;
  items.add(new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "4.x Reg Number:"),
            new NumberEditItem<uint32_t>(COL1_POSITION, y, &cfg.wwiv_reg_number),
    "Legacy registration # from WWIV 4.xx. Just used for bragging rights");

  items.add(new Label(LABEL2_POSITION, y, LABEL2_WIDTH, "Max Backups:"),
            new NumberEditItem<uint8_t>(COL2_POSITION, y, &cfg.max_backups),
    "Max number of backup to keep when making new datafile backups (0=unlimited)");

  ++y;

  items.Run("System Configuration");
  cfg.closedsystem = closed_system;
  cfg.newusergold = static_cast<float>(gold);
  config.set_config(&cfg, true);
}
