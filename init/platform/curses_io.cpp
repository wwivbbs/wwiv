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

#include "wwivinit.h"
#include "platform/curses_io.h"

//TODO(rushfan): Don't include windows.h from wlocal_io anymore. Make a WinConsoleIU subclass like
// this CursesIO class and move the Window.h definition into there.
#undef MOUSE_MOVED

#include "curses.h"


#ifdef _WIN32
void CursesIO::set_attr_xy(int x, int y, int a) {
  //TODO(rushfan): Implement me if needed for compile.cpp
}
#endif  // _WIN32

CursesIO::CursesIO() {
  initscr();
  raw();
  keypad(stdscr, TRUE);
  noecho();
  nonl();
  max_x_ = getmaxx(stdscr);
  max_y_ = getmaxy(stdscr);

  start_color();

  for (short b = 0; b < COLORS; b++) {
    for (short f = 0; f < COLORS; f++) {
      init_pair((b * 16) + f, f, b);
    }
  }
  refresh();
}

CursesIO::~CursesIO() {
  endwin();
}

void CursesIO::LocalGotoXY(int x, int y) {
// Moves the cursor to the location specified
// Note: this function is 0 based, so (0,0) is the upper left hand corner.
  x = std::max<int>(x, 0);
  x = std::min<int>(x, max_x_);
  y = std::max<int>(y, 0);
  y = std::min<int>(y, max_y_);

  move(y, x);
  refresh();
}

int CursesIO::WhereX() {
  /* This function returns the current X cursor position, as the number of
  * characters from the left hand side of the screen.  An X position of zero
  * means the cursor is at the left-most position
  */
  return getcurx(stdscr);
}

int CursesIO::WhereY() {
  /* This function returns the Y cursor position, as the line number from
  * the top of the logical window.  The offset due to the protected top
  * of the screen display is taken into account.  A WhereY() of zero means
  * the cursor is at the top-most position it can be at.
  */
  return getcury(stdscr);
}

/**
 * Clears the local logical screen
 */
void CursesIO::LocalCls() {
  attrset(COLOR_PAIR(7));
  clear();
  refresh();
  LocalGotoXY(0, 0);
}

static void SetCursesAttribute() {
  unsigned long attr = COLOR_PAIR(curatr);
  unsigned long f = curatr & 0x0f;
  attrset(attr);
  if (f > 7) {
    attr |= A_BOLD;
  } else {
    attroff(A_BOLD);
  }
}

/**
 * This function outputs one character to the local screen.  C/R, L/F, TOF,
 * BS, and BELL are interpreted as commands instead of characters.
 */
void CursesIO::LocalPutch(unsigned char ch) {
  SetCursesAttribute();
  addch(ch);
  refresh();
}

void CursesIO::LocalPuts(const char *pszText) {
  SetCursesAttribute();
  if (strlen(pszText) == 2) {
    if (pszText[0] == '\r' && pszText[1] == '\n') {
      LocalGotoXY(0, WhereY() + 1);
      return;
    }
  }
  addstr(pszText);
  refresh();
}

void CursesIO::LocalXYPuts(int x, int y, const char *pszText) {
  LocalGotoXY(x, y);
  LocalPuts(pszText);
}

int CursesIO::getchd() {
  return getch();
}

void CursesIO::LocalClrEol() {
  SetCursesAttribute();
  clrtoeol();
  refresh();
}

void CursesIO::LocalScrollScreen(int nTop, int nBottom, int nDirection) {
  //TODO(rushfan): Implement if needed for compile.cpp
}
