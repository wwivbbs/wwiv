/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#include "localui/curses_win.h"

#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "localui/wwiv_curses.h"

#include <algorithm>
#include <chrono>
#include <string>

using namespace std::chrono;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::local::ui {

CursesWindow::CursesWindow(CursesWindow* parent, ColorScheme* color_scheme, int nlines, int ncols,
                           int begin_y, int begin_x)
    : UIWindow(parent, color_scheme), parent_(parent), color_scheme_(color_scheme) {
  WINDOW* window;
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
    auto* pw = std::any_cast<WINDOW*>(parent->window());
    window = newwin(nlines, ncols, begin_y + getbegy(pw), begin_x + getbegx(pw));
  } else {
    if (begin_x == -1) {
      begin_x = 0;
    }
    if (begin_y == -1) {
      begin_y = 0;
    }
    window = newwin(nlines, ncols, begin_y, begin_x);
  }
  window_ = window;

  keypad(window, true);
  werase(window);
  touchwin(window);
  Refresh();
}

CursesWindow::~CursesWindow() {
  try {
    auto* w = std::any_cast<WINDOW*>(window_);
    delwin(w);
    if (parent_) {
      parent_->RedrawWin();
      parent_->Refresh();
      doupdate();
    }
  } catch (...) {
    // NOP
  }
}

std::any CursesWindow::window() const { return window_; }

CursesWindow* CursesWindow::parent() const { return parent_; }

bool CursesWindow::IsGUI() const { return true; }

void CursesWindow::SetTitle(const std::string& title) {
  const auto saved_scheme(this->current_scheme_id());
  SetColor(SchemeId::WINDOW_TITLE);
  const auto max_title_size = GetMaxX() - 6;
  auto working_title = fmt::format(" {} ", title);
  if (ssize(title) > max_title_size) {
    working_title = fmt::format(" {} ", title.substr(0, max_title_size));
  }

  const auto x = GetMaxX() - 1 - size_int(working_title);
  PutsXY(x, 0, working_title);
  SetColor(saved_scheme);
}

void CursesWindow::Bkgd(uint32_t ch) { wbkgd(std::any_cast<WINDOW*>(window_), ch); }
int CursesWindow::RedrawWin() { return redrawwin(std::any_cast<WINDOW*>(window_)); }
int CursesWindow::TouchWin() { return touchwin(std::any_cast<WINDOW*>(window_)); }
int CursesWindow::Refresh() { return wrefresh(std::any_cast<WINDOW*>(window_)); }
int CursesWindow::Move(int y, int x) { return wmove(std::any_cast<WINDOW*>(window_), y, x); }
int CursesWindow::GetcurX() const { return getcurx(std::any_cast<WINDOW*>(window_)); }
int CursesWindow::GetcurY() const { return getcury(std::any_cast<WINDOW*>(window_)); }
int CursesWindow::Clear() { return wclear(std::any_cast<WINDOW*>(window_)); }
int CursesWindow::Erase() { return werase(std::any_cast<WINDOW*>(window_)); }
int CursesWindow::AttrSet(uint32_t attrs) {
  return wattrset(std::any_cast<WINDOW*>(window_), attrs);
}
int CursesWindow::Keypad(bool b) { return keypad(std::any_cast<WINDOW*>(window_), b); }
int CursesWindow::GetMaxX() const { return getmaxx(std::any_cast<WINDOW*>(window_)); }
int CursesWindow::GetMaxY() const { return getmaxy(std::any_cast<WINDOW*>(window_)); }
int CursesWindow::ClrtoEol() { return wclrtoeol(std::any_cast<WINDOW*>(window_)); }
int CursesWindow::AttrGet(uint32_t* a, short* c) const {
  return wattr_get(std::any_cast<WINDOW*>(window_), reinterpret_cast<attr_t*>(a), c, nullptr);
}
int CursesWindow::Box(uint32_t vert_ch, uint32_t horiz_ch) {
  return box(std::any_cast<WINDOW*>(window_), vert_ch, horiz_ch);
}

int CursesWindow::GetChar(duration<double> timeout) const {
  auto* window = std::any_cast<WINDOW*>(window_);
  const auto timeout_ms =  duration_cast<milliseconds>(timeout);
  const auto start = system_clock::now();
  const auto end = start + timeout_ms;

  for (;;) {
    wtimeout(window, static_cast<int>(timeout_ms.count()));
    const auto ch = wgetch(window);
    if (ch == KEY_RESIZE) {
      // Since we don't support online window resizing just ignore this
      // but don't return it as a key for the application to (badly) handle.
      continue;
    }
    if (ch == ERR && system_clock::now() >= end) {
      return ERR;
    }
    if (ch == ERR) {
      continue;
    }
    if (ch == 27) {
      wtimeout(window, 0);
      nodelay(window, true);
      wwiv::core::ScopeExit at_exit([=]{ nodelay(window, false); });
      // Check for alt.
      const auto ch2 = wgetch(window);
      if (ch2 == ERR) {
        return 27;
      }
      switch (ch2) {
        case 'I': 
        case 'i':
        return KEY_IC;
      }
      
      continue;
    }
    switch (ch) {
#ifdef __PDCURSES__
    // Win10 Terminal started using these
    case KEY_A1:
      return KEY_HOME;
    case KEY_A2: // upper middle on Virt. keypad.
      return KEY_UP;
    case KEY_A3:
      return KEY_PPAGE;
    case KEY_B1: // middle left on Virt. keypad. Win10 Terminal started using this.
      return KEY_LEFT;
    case KEY_B3: // middle right on Vir. keypad. Win10 Terminal started using this.
      return KEY_RIGHT;
    case KEY_C1:
      return KEY_END;
    case KEY_C2: // lower middle on Virt. keypad. Win10 Terminal started using this.
      return KEY_DOWN;
    case KEY_C3:
#endif

#ifdef PADENTER
    case PADENTER:
      return KEY_ENTER;
#endif

#ifdef KEY_SSAVE
    // https://github.com/wwivbbs/wwiv/issues/1360
    // control-Z not working through default terminal on RPI.
    case KEY_SSAVE:
      return 26; // Control-Z
#endif // KEY_SSAVE

    default:
      return ch;
    }
  }
}

int CursesWindow::GetChar() const {
  return GetChar(hours(24));
}

void CursesWindow::GotoXY(int x, int y) {
  x = std::max<int>(x, 0);
  x = std::min<int>(x, GetMaxX() - 1);
  y = std::max<int>(y, 0);
  y = std::min<int>(y, GetMaxY() - 1);

  Move(y, x);
  Refresh();
}

void CursesWindow::Putch(uint32_t ch) {
  waddch(std::any_cast<WINDOW*>(window_), ch);
  Refresh();
}

void CursesWindow::Puts(const std::string& text) {
  waddstr(std::any_cast<WINDOW*>(window_), text.c_str());
  Refresh();
}

void CursesWindow::PutsXY(int x, int y, const std::string& text) {
  mvwaddstr(std::any_cast<WINDOW*>(window_), y, x, text.c_str());
  Refresh();
}

void CursesWindow::PutchW(wchar_t ch) {
  // Curses seriously has no way to display a single wchar_t!!
  wchar_t c[2] = {ch, 0};
  waddwstr(std::any_cast<WINDOW*>(window_), c);

  Refresh();
}

void CursesWindow::PutsW(const std::wstring& text) {
  waddwstr(std::any_cast<WINDOW*>(window_), text.c_str());
  Refresh();
}

void CursesWindow::PutsXYW(int x, int y, const std::wstring& text) {
  mvwaddwstr(std::any_cast<WINDOW*>(window_), y, x, text.c_str());
  Refresh();
}

void CursesWindow::SetColor(SchemeId id) {
  AttrSet(color_scheme_->GetAttributesForScheme(id));
  set_current_scheme_id(id);
}

void CursesWindow::SetDosColor(int original_color) {
  const auto bold = (original_color & 8) != 0;
  auto color = original_color;
  const auto bg = (color >> 4) & 0x07;
  const auto fg = color & 0x07;
  color = (bg << 3) | fg;
  // DOS colors start at color pair 100.
  auto attr = COLOR_PAIR(color + 100);
  if (bold) {
    attr |= A_BOLD;
  }
  AttrSet(attr);
}

std::vector<int> colors = {7, 11, 14, 5, 31, 2, 12, 9, 6, 3};

enum class pipe_state_t { text, pipe };

void CursesWindow::PutsWithPipeColors(const std::string& s) {
  auto state = pipe_state_t::text;
  std::string curpipe;
  auto curatr = 0x07;
  for (auto it = std::begin(s); it != std::end(s); ++it) {
    const auto c = *it;
    switch (state) {
    case pipe_state_t::text: {
      if (c == '|') {
        state = pipe_state_t::pipe;
        curpipe.push_back(c);
      } else {
        Putch(c);
      }
    } break;
    // grab first
    case pipe_state_t::pipe: {
      const auto pipe_size = ssize(curpipe);
      if (std::isdigit(c) && pipe_size == 2) {
        curpipe.push_back(c);
        if (curpipe[1] == '#') {
          curatr = at(colors, curpipe[2] - '0');
        } else {
          curatr = to_number<int>(curpipe.substr(1));
        }
        curpipe.clear();
        SetDosColor(curatr);
        state = pipe_state_t::text;
      } else if ((std::isdigit(c) || c == '#') && pipe_size == 1) {
        curpipe.push_back(c);
      } else {
        state = pipe_state_t::text;
        Puts(curpipe);
        curpipe.clear();
      }
    } break;
    }
  }
}

void CursesWindow::PutsWithPipeColors(int x, int y, const std::string& text) {
  GotoXY(x, y);
  PutsWithPipeColors(text);
}

}
