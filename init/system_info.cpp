/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2017, WWIV Software Services           */
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
#include "init/system_info.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "core/strings.h"
#include "init/init.h"
#include "localui/wwiv_curses.h"
#include "localui/input.h"
#include "init/utility.h"
#include "init/wwivinit.h"

using std::unique_ptr;
using std::string;
using namespace wwiv::strings;

static string print_time(uint16_t t) {
  return StringPrintf("%02d:%02d", t / 60, t % 60);
}

static uint16_t get_time(const string& s) {
  if (s[2] != ':') {
    return std::numeric_limits<uint16_t>::max();
  }

  uint16_t h = to_number<uint16_t>(s);
  string minutes = s.substr(3);
  uint16_t m = to_number<uint16_t>(minutes);
  if (h > 23 || m > 59) {
    return std::numeric_limits<uint16_t>::max();
  }
  return static_cast<uint16_t>(h * 60 + m);
}

static const int MAX_TIME_EDIT_LEN = 5;

class TimeEditItem : public EditItem<uint16_t*> {
public:
  TimeEditItem(int x, int y, uint16_t* data) : EditItem<uint16_t*>(x, y, 5, data) {}
  virtual ~TimeEditItem() {}

  virtual int Run(CursesWindow* window) {
    window->GotoXY(this->x_, this->y_);
    string s = print_time(*this->data_);
    int return_code = editline(window, &s, MAX_TIME_EDIT_LEN + 1, EditLineMode::ALL, "");
    *this->data_ = get_time(s);
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    string s = print_time(*this->data_);
    DefaultDisplayString(window, s);
  }
};

class Float53EditItem : public EditItem<float*> {
public:
  Float53EditItem(int x, int y, float* data) : EditItem<float*>(x, y, 5, data) {}
  virtual ~Float53EditItem() {}

  virtual int Run(CursesWindow* window) {
    window->GotoXY(this->x_, this->y_);
   
    // passing *this->data_ to StringPrintf is causing a bus error
    // on GCC/ARM (RPI).  See http://stackoverflow.com/questions/26158510
    float d = *this->data_;
    string s = StringPrintf("%5.3f", d);
    int return_code = editline(window, &s, 5 + 1, EditLineMode::NUM_ONLY, "");

    float f;
    sscanf(s.c_str(), "%f", &f);
    if (f > 9.999 || f < 0.001) {
      f = 0.0;
    }
    *this->data_ = f;
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    // passing *this->data_ to StringPrintf is causing a bus error
    // on GCC/ARM (RPI).  See http://stackoverflow.com/questions/26158510
    float d = *this->data_;
    auto s = StringPrintf("%5.3f", d);
    DefaultDisplayString(window, s);
  }
};

void sysinfo1(const std::string& datadir) {
  read_status(datadir);

  if (statusrec.callernum != 65535) {
    statusrec.callernum1 = static_cast<long>(statusrec.callernum);
    statusrec.callernum = 65535;
    save_status(datadir);
  }

  static constexpr int LABEL1_POSITION = 2;
  static constexpr int LABEL1_WIDTH = 18;
  static constexpr int LABEL2_WIDTH = 10;
  static constexpr int COL1_POSITION = LABEL1_POSITION + LABEL1_WIDTH + 1;
  static constexpr int LABEL2_POSITION = COL1_POSITION + 8;
  static constexpr int COL2_POSITION = LABEL2_POSITION + LABEL2_WIDTH + 1;

  out->Cls(ACS_CKBOARD);

  int y = 1;
  EditItems items{};
  items.add_labels(
      {new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "System PW:"),
       new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "System name:"),
       new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "System phone:"),
       new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Closed system:"),
       new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Newuser PW: "),
       new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Newuser restrict:"),
       new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Newuser SL:"),
       new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Newuser DSL:"),
       new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Newuser gold:"),
       new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Sysop name:"),
       new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "Sysop time: from:"),
       new Label(LABEL2_POSITION, y++, LABEL2_WIDTH, "to:"),
       new Label(LABEL1_POSITION, y, LABEL1_WIDTH, "Ratios    :  U/D:"),
       new Label(LABEL2_POSITION, y++, LABEL2_WIDTH, "Post/Call:"),
       new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Max waiting:"),
       new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Max users:"),
       new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Caller number:"),
       new Label(LABEL1_POSITION, y++, LABEL1_WIDTH, "Days active:")});

  bool closed_system = syscfg.closedsystem > 0;
  int gold = static_cast<int>(syscfg.newusergold);
  y = 1;
  items.add_items({
    new StringEditItem<char*>(COL1_POSITION, y++, 20, syscfg.systempw, true),
      new StringEditItem<char*>(COL1_POSITION, y++ , 50, syscfg.systemname, false),
      new StringEditItem<char*>(COL1_POSITION, y++, 12, syscfg.systemphone, true),
      new BooleanEditItem(out, COL1_POSITION, y++, &closed_system),
      new StringEditItem<char*>(COL1_POSITION, y++, 20, syscfg.newuserpw, true),
      new RestrictionsEditItem(COL1_POSITION, y++, &syscfg.newuser_restrict),
      new NumberEditItem<uint8_t>(COL1_POSITION, y++, &syscfg.newusersl),
      new NumberEditItem<uint8_t>(COL1_POSITION, y++, &syscfg.newuserdsl),
      new NumberEditItem<int>(COL1_POSITION, y++, &gold),
      new StringEditItem<char*>(COL1_POSITION, y++, 50, syscfg.sysopname, false),
      new TimeEditItem(COL1_POSITION, y, &syscfg.sysoplowtime),
      new TimeEditItem(COL2_POSITION, y++, &syscfg.sysophightime),

      new Float53EditItem(COL1_POSITION, y, &syscfg.req_ratio),
      new Float53EditItem(COL2_POSITION, y++, &syscfg.post_call_ratio),

      new NumberEditItem<uint8_t>(COL1_POSITION, y++, &syscfg.maxwaiting),
      new NumberEditItem<uint16_t>(COL1_POSITION, y++, &syscfg.maxusers),
      new NumberEditItem<uint32_t>(COL1_POSITION, y++, &statusrec.callernum1),
      new NumberEditItem<uint16_t>(COL1_POSITION, y++, &statusrec.days),
  });

  items.Run("System Configuration");
  syscfg.closedsystem = closed_system;
  syscfg.newusergold = static_cast<float>(gold);

  save_config();
}
