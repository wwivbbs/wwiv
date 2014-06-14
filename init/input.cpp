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
#include "input.h"

#include <curses.h>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

#include "ifcns.h"
#include "init.h"
#include "wwivinit.h"

using std::string;
using std::vector;

template<> int StringEditItem<char *>::Run() {
  app->localIO->LocalGotoXY(x_, y_);
  int return_code = 0;
  int status = uppercase_ ? UPPER_ONLY : ALL;
  editline(data_, maxsize_, status, &return_code, "");
  return return_code;
}

template<> int StringEditItem<unsigned char *>::Run() {
  app->localIO->LocalGotoXY(x_, y_);
  int return_code = 0;
  int status = uppercase_ ? UPPER_ONLY : ALL;
  editline(reinterpret_cast<char*>(data_), maxsize_, status, &return_code, "");
  return return_code;
}

template<typename T> 
static int EditNumberItem(T* data, int maxlen) {
  char s[21];
  int return_code = 0;
  sprintf(s, "%-7u", *data);
  editline(s, maxlen, NUM_ONLY, &return_code, "");
  *data = atoi(s);
  return return_code;
}

template<> int NumberEditItem<uint32_t>::Run() {
  app->localIO->LocalGotoXY(x_, y_);
  return EditNumberItem<uint32_t>(data_, 5);
}

template<> int NumberEditItem<int8_t>::Run() {
  app->localIO->LocalGotoXY(x_, y_);
  return EditNumberItem<int8_t>(data_, 3);
}

template<> int NumberEditItem<uint8_t>::Run() {
  app->localIO->LocalGotoXY(x_, y_);
  return EditNumberItem<uint8_t>(data_, 3);
}

int CustomEditItem::Run() {
  app->localIO->LocalGotoXY(x_, y_);
  std::string s = to_field_();
  char data[81];
  strcpy(data, s.c_str());

  int return_code = 0;
  editline(&s, maxsize_, ALL, &return_code, "");
  s.assign(data);
  from_field_(data);
  return return_code;
}

void CustomEditItem::Display() const {
  app->localIO->LocalGotoXY(x_, y_);
  std::string blanks(maxsize_, ' ');
  Puts(blanks.c_str());

  string s = to_field_();
  if (display_) {
    display_(s);
  } else {
    app->localIO->LocalGotoXY(x_, y_);
    Puts(s.c_str());
  }
}

void EditItems::Run() {
  int cp = 0;
  const int size = static_cast<int>(items_.size());
  Display();
  for (;;) {
    int i1 = items_[cp]->Run();
    if (i1 == PREV) {
      if (--cp < 0) {
        cp = size - 1;
      }
    } else if (i1 == NEXT) {
      if (++cp >= size) {
        cp = 0;
      }
    } else if (i1 == DONE) {
      return;
    }
  }
}

void EditItems::Display() const {
  textattr(COLOR_CYAN);

  for (BaseEditItem* item : items_) {
    item->Display();
  }
}

EditItems::~EditItems() {
  // Since we added raw pointers we must cleanup.  Since AFAIK there is 
  // no easy way to convert from std::initializer_list<T> to
  // std::initializer_list<unique_ptr<T>>
  for (auto item : items_) {
    delete item;
  }
}

/**
 * Printf sytle output function.  Most init output code should use this.
 */
void PrintfXY(int x, int y, const char *pszFormat, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, pszFormat);
  vsnprintf(szBuffer, 1024, pszFormat, ap);
  va_end(ap);
  app->localIO->LocalXYPuts(x, y, szBuffer);
}

void Puts(const char *pszText) {
  app->localIO->LocalPuts(pszText);
}

void PutsXY(int x, int y, const char *pszText) {
  app->localIO->LocalXYPuts(x, y,pszText);
}

/**
 * Printf sytle output function.  Most init output code should use this.
 */
void Printf(const char *pszFormat, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, pszFormat);
  vsnprintf(szBuffer, 1024, pszFormat, ap);
  va_end(ap);
  app->localIO->LocalPuts(szBuffer);
}

bool dialog_yn(const std::string prompt) {
  const int maxx = getmaxx(stdscr);
  const int maxy = getmaxy(stdscr);
  std::string s = prompt + " ? ";
  const int width = s.size() + 4;
  const int startx = (maxx - s.size()) / 2;
  const int starty = (maxy - 3) / 2;
  WINDOW *dialog = newwin(3, width, starty, startx);
  wbkgd(dialog, COLOR_PAIR(31));
  wattrset(dialog, COLOR_PAIR(31));
  wattron(dialog, A_BOLD);
  box(dialog, 0, 0);
  mvwaddstr(dialog, 1, 2, s.c_str());
  wrefresh(dialog);
  int ch = wgetch(dialog);
  delwin(dialog);
  redrawwin(stdscr);
  refresh();
  return ch == 'Y' || ch == 'y';
}