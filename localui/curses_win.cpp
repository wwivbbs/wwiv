/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
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

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "core/stl.h"
#include "core/strings.h"
#include "localui/wwiv_curses.h"
#include "localui/curses_win.h"
#include "fmt/format.h"

using std::string;
using namespace wwiv::stl;
using namespace wwiv::strings;

static constexpr size_t VSN_BUFFER_SIZE = 1024;

CursesWindow::CursesWindow(CursesWindow* parent, ColorScheme* color_scheme, int nlines, int ncols, int begin_y, int begin_x) 
  : UIWindow(parent, color_scheme), parent_(parent), color_scheme_(color_scheme) {
  WINDOW* window = nullptr;
  if (parent != nullptr) {
    if (ncols > parent->GetMaxX()) {
      ncols = parent->GetMaxX();
    }
    if (nlines > parent->GetMaxY()) {
      nlines = parent->GetMaxY();
    }
    if (begin_x == -1) {
      begin_x = (parent->GetMaxX() - ncols) / 2;
    }
    if (begin_y == -1) {
      begin_y = (parent->GetMaxY() - nlines) / 2;
    }
    auto* pw = reinterpret_cast<WINDOW*>(parent->window());
    window =  newwin(nlines, ncols, begin_y + getbegy(pw),
      begin_x + getbegx(pw));
  } else {
    if (begin_x == -1) { begin_x = 0; }
    if (begin_y == -1) { begin_y = 0; }
    window = newwin(nlines, ncols, begin_y, begin_x);
  }
  window_ = window;

  keypad(window, true);
  werase(window);
  touchwin(window);
  Refresh();
}

CursesWindow::~CursesWindow() {
  auto* w = reinterpret_cast<WINDOW*>(window_);
  delwin(w);
  if (parent_) {
    parent_->RedrawWin();
    parent_->Refresh();
    doupdate();
  }
}

void CursesWindow::SetTitle(const std::string& title) {
  SchemeId saved_scheme(this->current_scheme_id());
  SetColor(SchemeId::WINDOW_TITLE);
  auto max_title_size = GetMaxX() - 6;
  auto working_title = fmt::format(" {} ", title);
  if (size_int(title) > max_title_size) {
    working_title = fmt::format(" {} ", title.substr(0, max_title_size));
  }

  auto x = GetMaxX() - 1 - working_title.size();
  PutsXY(x, 0, working_title);
  SetColor(saved_scheme);
}

void CursesWindow::Bkgd(uint32_t ch) { wbkgd(reinterpret_cast<WINDOW*>(window_), ch); }
int CursesWindow::RedrawWin() { return redrawwin(reinterpret_cast<WINDOW*>(window_)); }
int CursesWindow::TouchWin() { return touchwin(reinterpret_cast<WINDOW*>(window_)); }
int CursesWindow::Refresh() { return wrefresh(reinterpret_cast<WINDOW*>(window_)); }
int CursesWindow::Move(int y, int x) { return wmove(reinterpret_cast<WINDOW*>(window_), y, x); }
int CursesWindow::GetcurX() const { return getcurx(reinterpret_cast<WINDOW*>(window_)); }
int CursesWindow::GetcurY() const { return getcury(reinterpret_cast<WINDOW*>(window_)); }
int CursesWindow::Clear() { return wclear(reinterpret_cast<WINDOW*>(window_)); }
int CursesWindow::Erase() { return werase(reinterpret_cast<WINDOW*>(window_)); }
int CursesWindow::AttrSet(uint32_t attrs) { return wattrset(reinterpret_cast<WINDOW*>(window_), attrs); }
int CursesWindow::Keypad(bool b) { return keypad(reinterpret_cast<WINDOW*>(window_), b); }
int CursesWindow::GetMaxX() const { return getmaxx(reinterpret_cast<WINDOW*>(window_)); }
int CursesWindow::GetMaxY() const { return getmaxy(reinterpret_cast<WINDOW*>(window_)); }
int CursesWindow::ClrtoEol() { return wclrtoeol(reinterpret_cast<WINDOW*>(window_)); }
int CursesWindow::AttrGet(uint32_t* a, short* c) const { 
  return wattr_get(reinterpret_cast<WINDOW*>(window_), (attr_t*)a, c, nullptr); 
}
int CursesWindow::Box(uint32_t vert_ch, uint32_t horiz_ch) { return box(reinterpret_cast<WINDOW*>(window_), vert_ch, horiz_ch); }

int CursesWindow::GetChar() const {
  for (;;) {
    int ch = wgetch(reinterpret_cast<WINDOW*>(window_));
    if (ch == KEY_RESIZE) {
      // Since we don't support online window resizing just ignore this
      // but don't return it as a key for the application to (badly) handle.
      continue;
    }
    return ch;
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

void CursesWindow::Putch(uint32_t ch) {
  waddch(reinterpret_cast<WINDOW*>(window_), ch);
  Refresh();
}

void CursesWindow::Puts(const std::string& text) {
  waddstr(reinterpret_cast<WINDOW*>(window_), text.c_str());
  Refresh();
}

void CursesWindow::PutsXY(int x, int y, const std::string& text) {
  mvwaddstr(reinterpret_cast<WINDOW*>(window_), y, x, text.c_str());
  Refresh();
}

void CursesWindow::SetColor(SchemeId id) {
  AttrSet(color_scheme_->GetAttributesForScheme(id));
  set_current_scheme_id(id);
}
