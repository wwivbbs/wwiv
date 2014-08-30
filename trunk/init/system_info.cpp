/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*                 Copyright (C)2014, WWIV Software Services              */
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
#include "user_editor.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "utility.h"
#include "wwivinit.h"

using std::auto_ptr;

static const int COL1_LINE = 2;
static const int COL1_POSITION = 21;

static void print_time(uint16_t t, char *s) {
  sprintf(s, "%02d:%02d", t / 60, t % 60);
}

static uint16_t get_time(char *s) {
  if (s[2] != ':') {
    return std::numeric_limits<uint16_t>::max();
  }

  unsigned short h = atoi(s);
  unsigned short m = atoi(s + 3);
  if (h > 23 || m > 59) {
    return std::numeric_limits<uint16_t>::max();
  }
  return static_cast<uint16_t>(h * 60 + m);
}

static const int MAX_TIME_EDIT_LEN = 5;

class TimeEditItem : public EditItem<uint16_t*> {
public:
  TimeEditItem(int x, int y, uint16_t* data) : EditItem<uint16_t*>(x, y, 0, data) {}
  virtual ~TimeEditItem() {}

  virtual int Run(CursesWindow* window) {
    window->GotoXY(this->x_, this->y_);
    char s[21];
    int return_code = 0;
    print_time(*this->data_, s);
    editline(window, s, MAX_TIME_EDIT_LEN + 1, ALL, &return_code, "");
    *this->data_ = get_time(s);
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    std::string blanks(this->maxsize_, ' ');
    window->PutsXY(this->x_, this->y_, blanks.c_str());
    char s[21];
    print_time(*this->data_, s);
    window->PrintfXY(this->x_, this->y_, "%s", s);
  }
};

class Float53EditItem : public EditItem<float*> {
public:
  Float53EditItem(int x, int y, float* data) : EditItem<float*>(x, y, 0, data) {}
  virtual ~Float53EditItem() {}

  virtual int Run(CursesWindow* window) {
    window->GotoXY(this->x_, this->y_);
    char s[21];
    int return_code = 0;
    sprintf(s, "%5.3f", *this->data_);
    editline(window, s, 5 + 1, NUM_ONLY, &return_code, "");

    float f;
    sscanf(s, "%f", &f);
    if (f > 9.999 || f < 0.001) {
      f = 0.0;
    }
    *this->data_ = f;
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    std::string blanks(this->maxsize_, ' ');
    window->PutsXY(this->x_, this->y_, blanks.c_str());
    window->PrintfXY(this->x_, this->y_, "%5.3f", *this->data_);
  }
};

void sysinfo1() {
  read_status();

  if (status.callernum != 65535) {
    status.callernum1 = static_cast<long>(status.callernum);
    status.callernum = 65535;
    save_status();
  }

  out->Cls(ACS_BOARD);
  auto_ptr<CursesWindow> window(new CursesWindow(out->window(), 19, 76));
  window->SetColor(out->color_scheme(), SchemeId::WINDOW_BOX);
  window->Box(0, 0);
  window->SetColor(out->color_scheme(), SchemeId::WINDOW_TEXT);

  int y = 1;
  window->PrintfXY(COL1_LINE, y++, "System PW        : ");
  window->PrintfXY(COL1_LINE, y++, "System name      : ");
  window->PrintfXY(COL1_LINE, y++, "System phone     : ");
  window->PrintfXY(COL1_LINE, y++, "Closed system    : ");

  window->PrintfXY(COL1_LINE, y++, "Newuser PW       : ");
  window->PrintfXY(COL1_LINE, y++, "Newuser restrict : ");
  window->PrintfXY(COL1_LINE, y++, "Newuser SL       : ");
  window->PrintfXY(COL1_LINE, y++, "Newuser DSL      : ");
  window->PrintfXY(COL1_LINE, y++, "Newuser gold     : ");

  window->PrintfXY(COL1_LINE, y++, "Sysop name       : ");
  window->PrintfXY(COL1_LINE, y++, "Sysop time: from : ");

  window->PrintfXY(COL1_LINE, y++, "Net time  : from :       to: ");

  window->PrintfXY(COL1_LINE, y++, "Ratios    :  U/D :        Post/Call: ");

  window->PrintfXY(COL1_LINE, y++, "Max waiting      : ");
  window->PrintfXY(COL1_LINE, y++, "Max users        : ");
  window->PrintfXY(COL1_LINE, y++, "Caller number    : ");
  window->PrintfXY(COL1_LINE, y++, "Days active      : ");

  bool closed_system = syscfg.closedsystem > 0;
  EditItems items{
    new StringEditItem<char*>(COL1_POSITION, 1, 20, syscfg.systempw, true),
    new StringEditItem<char*>(COL1_POSITION, 2, 50, syscfg.systemname, false),
    new StringEditItem<char*>(COL1_POSITION, 3, 12, syscfg.systemphone, true),
    new BooleanEditItem(COL1_POSITION, 4, &closed_system),
    new StringEditItem<char*>(COL1_POSITION, 5, 20, syscfg.newuserpw, true),
    new RestrictionsEditItem(COL1_POSITION, 6, &syscfg.newuser_restrict),
    new NumberEditItem<uint8_t>(COL1_POSITION, 7, &syscfg.newusersl),
    new NumberEditItem<uint8_t>(COL1_POSITION, 8, &syscfg.newuserdsl),
    new NumberEditItem<float>(COL1_POSITION, 9, &syscfg.newusergold),
    new StringEditItem<char*>(COL1_POSITION, 10, 50, syscfg.sysopname, false),
    new TimeEditItem(COL1_POSITION, 11, &syscfg.sysoplowtime),
    new TimeEditItem(COL1_POSITION + 10, 11, &syscfg.sysophightime),
    new TimeEditItem(COL1_POSITION, 12, &syscfg.netlowtime),
    new TimeEditItem(COL1_POSITION + 10, 12, &syscfg.nethightime),

    new Float53EditItem(COL1_POSITION, 13, &syscfg.req_ratio),
    new Float53EditItem(COL1_POSITION + 18, 13, &syscfg.post_call_ratio),

    new NumberEditItem<uint8_t>(COL1_POSITION, 14, &syscfg.maxwaiting),
    new NumberEditItem<uint16_t>(COL1_POSITION, 15, &syscfg.maxusers),
    new NumberEditItem<uint32_t>(COL1_POSITION, 16, &status.callernum1),
    new NumberEditItem<uint16_t>(COL1_POSITION, 17, &status.days),
  };

  items.set_curses_io(out, window.get());
  items.Run();
  syscfg.closedsystem = closed_system;


  save_config();
}
