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

#include "platform/curses_io.h"
#include "curses.h"

extern const char *wwiv_version;
extern const char *beta_version;
extern const char *wwiv_date;
extern unsigned short wwiv_num_version;

static const char* copyrightString = "Copyright (c) 1998-2014, WWIV Software Services";

CursesIO* out;

CursesIO::CursesIO() 
    : max_x_(0), max_y_(0), window_(nullptr), footer_(nullptr), 
      header_(nullptr), indicator_mode_(IndicatorMode::NONE) {
  initscr();
  raw();
  keypad(stdscr, TRUE);
  noecho();
  nonl();
  start_color();
  scheme_ = LoadColorSchemes();

  int stdscr_maxx = getmaxx(stdscr);
  int stdscr_maxy = getmaxy(stdscr);
  header_ = newwin(2, 0, 0, 0);
  footer_ = newwin(2, 0, stdscr_maxy-2, 0);
  wbkgd(header_, GetAttributesForScheme(Scheme::HEADER));
  char s[81];
  sprintf(s, "WWIV %s%s Initialization/Configuration Program.", wwiv_version, beta_version);
  SetColor(header_, Scheme::HEADER);
  mvwprintw(header_, 0, 0, s);
  SetColor(header_, Scheme::HEADER_COPYRIGHT);
  mvwaddstr(header_, 1, 0, copyrightString);
  wbkgd(footer_, GetAttributesForScheme(Scheme::HEADER));
  wrefresh(header_);
  wrefresh(footer_);
  redrawwin(header_);

  window_ = newwin(stdscr_maxy-4, stdscr_maxx, 2, 0);
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
  SetColor(Scheme::NORMAL);
  wclear(window_);
  wrefresh(window_);
  GotoXY(0, 0);
}

void CursesIO::Putch(unsigned char ch) {
  waddch(window_, ch);
  wrefresh(window_);
}

void CursesIO::Puts(const char *pszText) {
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

void CursesIO::SetDefaultFooter() {
  werase(footer_);
  wmove(footer_, 0, 0);
  SetColor(footer_, Scheme::FOOTER_KEY);
  mvwaddstr(footer_, 0, 0, "Esc");
  SetColor(footer_, Scheme::FOOTER_TEXT);
  waddstr(footer_, "-Exit ");
  wrefresh(footer_);
}

void CursesIO::SetIndicatorMode(IndicatorMode mode) {
  if (mode == indicator_mode_) {
    return;
  }
  int x = max_x_ - 5;
  std::string s = "   ";
  switch (mode) {
  case IndicatorMode::INSERT:
    s = "INS";
    break;
  case IndicatorMode::OVERWRITE:
    s = "OVR";
    break;
  }
  mvwaddstr(header_, 1, x, s.c_str());
  wrefresh(header_);
  indicator_mode_ = mode;
}

attr_t CursesIO::GetAttributesForScheme(Scheme id) {
  const SchemeDescription& s = scheme_[id];
  attr_t attr = COLOR_PAIR(s.color_pair());
  if (s.bold()) {
    attr |= A_BOLD;
  }
  return attr;
}

void CursesIO::SetColor(WINDOW* window, Scheme id) {
  wattrset(window, GetAttributesForScheme(id));
}

// static 
std::map<Scheme, SchemeDescription> CursesIO::LoadColorSchemes() {
  std::map<Scheme, SchemeDescription> scheme;

  // Create a default color scheme, ideally this would be loaded from an INI file or some other
  // configuration file, but this is a sufficient start.
  scheme[Scheme::NORMAL] = SchemeDescription(Scheme::NORMAL, COLOR_CYAN, COLOR_BLACK, false);
  scheme[Scheme::ERROR_TEXT] = SchemeDescription(Scheme::ERROR_TEXT, COLOR_RED, COLOR_BLACK, false);
  scheme[Scheme::WARNING] = SchemeDescription(Scheme::WARNING, COLOR_MAGENTA, COLOR_BLACK, true);
  scheme[Scheme::HIGHLIGHT] = SchemeDescription(Scheme::HIGHLIGHT, COLOR_CYAN, COLOR_BLACK, true);
  scheme[Scheme::HEADER] = SchemeDescription(Scheme::HEADER, COLOR_YELLOW, COLOR_BLUE, true);
  scheme[Scheme::HEADER_COPYRIGHT] = SchemeDescription(Scheme::HEADER_COPYRIGHT, COLOR_WHITE, COLOR_BLUE, true);
  scheme[Scheme::FOOTER_KEY] = SchemeDescription(Scheme::FOOTER_KEY, COLOR_YELLOW, COLOR_BLUE, true);
  scheme[Scheme::FOOTER_TEXT] = SchemeDescription(Scheme::FOOTER_TEXT, COLOR_CYAN, COLOR_BLUE, false);
  scheme[Scheme::PROMPT] = SchemeDescription(Scheme::PROMPT, COLOR_YELLOW, COLOR_BLACK, true);
  scheme[Scheme::EDITLINE] = SchemeDescription(Scheme::EDITLINE, COLOR_WHITE, COLOR_BLUE, true);
  scheme[Scheme::INFO] = SchemeDescription(Scheme::INFO, COLOR_CYAN, COLOR_BLACK, false);
  scheme[Scheme::DIALOG_PROMPT] = SchemeDescription(Scheme::DIALOG_PROMPT, COLOR_YELLOW, COLOR_BLUE, true);
  scheme[Scheme::DIALOG_BOX] = SchemeDescription(Scheme::DIALOG_BOX, COLOR_GREEN, COLOR_BLUE, true);
  scheme[Scheme::DIALOG_TEXT] = SchemeDescription(Scheme::DIALOG_TEXT, COLOR_CYAN, COLOR_BLUE, true);

  // Create the color pairs for each of the colors defined in the color scheme.
  for (const auto& kv : scheme) {
    init_pair(kv.second.color_pair(), kv.second.f(), kv.second.b());
  }
  return scheme;
}

// static
void CursesIO::Init() {
    out = new CursesIO();
}
