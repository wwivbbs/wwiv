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
#include "curses.h"

CursesIO::CursesIO() {
  initscr();
  raw();
  keypad(stdscr, TRUE);
  noecho();
  nonl();
  start_color();
  for (short b = 0; b < COLORS; b++) {
    for (short f = 0; f < COLORS; f++) {
      init_pair((b * 16) + f, f, b);
    }
  }

  int stdscr_maxx = getmaxx(stdscr);
  int stdscr_maxy = getmaxy(stdscr);
  header_ = newwin(1, 0, 0, 0);
  footer_ = newwin(2, 0, stdscr_maxy-2, 0);
  wbkgd(header_, COLOR_PAIR((16 * COLOR_BLUE) + COLOR_WHITE));
  wattrset(header_, COLOR_PAIR((16 * COLOR_BLUE) + COLOR_YELLOW));
  wattron(header_, A_BOLD);
  char s[81];
  sprintf(s, "WWIV %s%s Initialization/Configuration Program.", wwiv_version, beta_version);
  waddstr(header_, s);
  wbkgd(footer_, COLOR_PAIR((COLOR_BLUE * 16) + COLOR_YELLOW));
  wrefresh(header_);
  wrefresh(footer_);
  redrawwin(header_);

  window_ = newwin(stdscr_maxy-3, stdscr_maxx, 1, 0);
  keypad(window_, TRUE);

  touchwin(stdscr);
  max_x_ = getmaxx(window_);
  max_y_ = getmaxy(window_);
  touchwin(window_);
  wrefresh(window_);
}

CursesIO::~CursesIO() {
  delwin(footer_);
  delwin(header_);
  delwin(window_);
  endwin();
}

void CursesIO::Refresh() { wrefresh(window_); }

/**
 * Moves the cursor to the location specified
 * Note: this function is 0 based, so (0,0) is the upper left hand corner.
 */
void CursesIO::GotoXY(int x, int y) {
  x = std::max<int>(x, 0);
  x = std::min<int>(x, max_x_);
  y = std::max<int>(y, 0);
  y = std::min<int>(y, max_y_);

  wmove(window_, y, x);
  wrefresh(window_);
}

/* This function returns the current X cursor position, as the number of
* characters from the left hand side of the screen.  An X position of zero
* means the cursor is at the left-most position
*/
int CursesIO::WhereX() {
  return getcurx(window_);
}

/* This function returns the Y cursor position, as the line number from
* the top of the logical window_.
*/
int CursesIO::WhereY() {
  return getcury(window_);
}

/**
 * Clears the local logical screen
 */
void CursesIO::Cls() {
  wattrset(window_, COLOR_PAIR(7));
  wclear(window_);
  wrefresh(window_);
  GotoXY(0, 0);
}

void CursesIO::SetCursesAttribute() {
  unsigned long attr = COLOR_PAIR(curatr);
  unsigned long f = curatr & 0x0f;
  wattrset(window_, attr);
  if (f > 7) {
    wattron(window_, A_BOLD);
  } else {
    wattroff(window_, A_BOLD);
  }
}

void CursesIO::Putch(unsigned char ch) {
  SetCursesAttribute();
  waddch(window_, ch);
  wrefresh(window_);
}

void CursesIO::Puts(const char *pszText) {
  SetCursesAttribute();
  if (strlen(pszText) == 2) {
    if (pszText[0] == '\r' && pszText[1] == '\n') {
      GotoXY(0, WhereY() + 1);
      return;
    }
  }
  waddstr(window_, pszText);
  wrefresh(window_);
}

void CursesIO::PutsXY(int x, int y, const char *pszText) {
  GotoXY(x, y);
  Puts(pszText);
}

int CursesIO::GetChar() {
  return wgetch(window_);
}

void CursesIO::ClrEol() {
  SetCursesAttribute();
  wclrtoeol(window_);
  wrefresh(window_);
}
