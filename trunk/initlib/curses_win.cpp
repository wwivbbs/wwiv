/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

#include "curses.h"
#include "curses_win.h"

CursesWindow::CursesWindow(CursesWindow* parent, int nlines, int ncols, int begin_y, int begin_x) 
    : parent_(parent) {
  if (parent != nullptr) {
    window_ =  newwin(nlines, ncols, begin_y + getbegy(parent->window()), 
      begin_x + getbegx(parent->window()));
  } else {
    window_ = newwin(nlines, ncols, begin_y, begin_x);
  }
  keypad(window_, true);
  werase(window_);
  touchwin(window_);
  Refresh();
}

CursesWindow::~CursesWindow() {
  delwin(window_);
  if (parent_) {
    parent_->RedrawWin();
    parent_->Refresh();
    doupdate();
  }
}

void CursesWindow::GotoXY(int x, int y) {
  x = std::max<int>(x, 0);
  x = std::min<int>(x, GetMaxX());
  y = std::max<int>(y, 0);
  y = std::min<int>(y, GetMaxY());

  Move(y, x);
  Refresh();
}

void CursesWindow::Putch(unsigned char ch) {
  AddCh(ch);
  Refresh();
}

void CursesWindow::Puts(const std::string& text) {
  AddStr(text.c_str());
  Refresh();
}

void CursesWindow::PutsXY(int x, int y, const std::string& text) {
  GotoXY(x, y);
  Puts(text.c_str());
}

/**
 * Printf sytle output function.  Most init output code should use this.
 */
void CursesWindow::Printf(const char *pszFormat, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, pszFormat);
  vsnprintf(szBuffer, 1024, pszFormat, ap);
  va_end(ap);
  Puts(szBuffer);
}

void CursesWindow::PrintfXY(int x, int y, const char *pszFormat, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, pszFormat);
  vsnprintf(szBuffer, 1024, pszFormat, ap);
  va_end(ap);
  PutsXY(x, y, szBuffer);
}

