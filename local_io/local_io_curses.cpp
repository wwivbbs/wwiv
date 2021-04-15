//**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
// Always declare wwiv_windows.h first to avoid collisions on defines.
#include "core/wwiv_windows.h"

#include "local_io/local_io_curses.h"
#include "core/cp437.h"
#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "localui/curses_io.h"
#include "localui/curses_win.h"
#include "localui/wwiv_curses.h"
#include "local_io/keycodes.h"
#include <algorithm>
#include <cstdio>
#include <string>

#ifdef __unix__
#include <unistd.h>
#endif

namespace wwiv::local::io {

using namespace wwiv::strings;

static const int default_screen_bottom = 20;

static void InitPairs() {
  std::vector<short> lowbit_colors = {COLOR_BLACK, COLOR_BLUE,    COLOR_GREEN,  COLOR_CYAN,
                                      COLOR_RED,   COLOR_MAGENTA, COLOR_YELLOW, COLOR_WHITE};
  short num = 0;
  for (auto bg = 0; bg < 8; bg++) {
    for (auto fg = 0; fg < 8; fg++) {
      init_pair(num++, lowbit_colors[fg], lowbit_colors[bg]);
    }
  }
}

CursesLocalIO::CursesLocalIO() : CursesLocalIO(default_screen_bottom + 1, 80) {}

CursesLocalIO::CursesLocalIO(int num_lines, int num_cols) {
  InitPairs();
  window_.reset(new wwiv::local::ui::CursesWindow(nullptr, wwiv::local::ui::curses_out->color_scheme(), num_lines, num_cols, 0, 0));
  auto* w = std::any_cast<WINDOW*>(window_->window());
  scrollok(w, true);
  window_->Clear();
}

CursesLocalIO::~CursesLocalIO() {
  CursesLocalIO::SetCursor(LocalIO::cursorNormal);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void CursesLocalIO::SetColor(int original_color) {
  const auto bold = (original_color & 8) != 0;
  auto color = original_color;
  const auto bg = (color >> 4) & 0x07;
  const auto fg = color & 0x07;
  color = (bg << 3) | fg;
  auto attr = COLOR_PAIR(color);
  if (bold) {
    attr |= A_BOLD;
  }
  window_->AttrSet(attr);
}

void CursesLocalIO::GotoXY(int x, int y) { window_->GotoXY(x, y); }

int CursesLocalIO::WhereX() const noexcept { return window_->GetcurX(); }

int CursesLocalIO::WhereY() const noexcept { return window_->GetcurY(); }

void CursesLocalIO::Lf() {
  Cr();
  y_ = WhereY();
  x_ = WhereX();
  y_++;

  if (y_ > GetScreenBottom()) {
    auto* w = std::any_cast<WINDOW*>(window_->window());
    scroll(w);
    y_ = GetScreenBottom();
  }
  window_->GotoXY(x_, y_);
}

void CursesLocalIO::Cr() {
  const auto y = WhereY();
  window_->GotoXY(0, y);
}

void CursesLocalIO::Cls() {
  SetColor(curatr());
  window_->Clear();
}

void CursesLocalIO::Backspace() {
  PutchRaw('\b');
}

void CursesLocalIO::PutchRaw(unsigned char ch) {
  SetColor(curatr());
  const auto wch = wwiv::core::cp437_to_utf8(static_cast<uint8_t>(ch));
  window_->PutchW(wch);
}

void CursesLocalIO::Putch(unsigned char ch) {
  if (ch > 31) {
    PutchRaw(ch);
  } else if (ch == CM) {
    Cr();
  } else if (ch == CJ) {
    Lf();
  } else if (ch == CL) {
    Cls();
  } else if (ch == BACKSPACE) {
    Backspace();
  } else if (ch == CG) {
    // No bell please.
  }
}

void CursesLocalIO::Puts(const std::string& s) {
  for (auto ch : s) {
    Putch(ch);
  }
}

void CursesLocalIO::PutsXY(int x, int y, const std::string& text) {
  GotoXY(x, y);
  FastPuts(text);
}

void CursesLocalIO::PutsXYA(int x, int y, int a, const std::string& text) {
  const auto old_color = curatr();
  curatr(a);

  GotoXY(x, y);
  FastPuts(text);

  curatr(old_color);
}

void CursesLocalIO::FastPuts(const std::string& text) {
  SetColor(curatr());
  const auto w = wwiv::core::cp437_to_utf8w(text);
  window_->PutsW(w);
}

void CursesLocalIO::set_protect(int l) { SetTopLine(l); }

static std::vector<chtype*> saved_screen;
void CursesLocalIO::savescreen() {
  saved_screen.clear();
  window_->Refresh();

  const auto width = window_->GetMaxX();
  const auto height = window_->GetMaxY();

  for (auto line = 0; line < height; line++) {
    auto* buf = new chtype[width];
    auto* w = std::any_cast<WINDOW*>(window_->window());
    mvwinchnstr(w, line, 0, buf, width);
    saved_screen.push_back(buf);
  }
}

void CursesLocalIO::restorescreen() {
  const auto width = window_->GetMaxX();
  auto y = 0;
  auto* w = std::any_cast<WINDOW*>(window_->window());
  for (auto* line : saved_screen) {
    mvwaddchnstr(w, y++, 0, line, width);
  }
  window_->Refresh();
}

static int last_key_pressed = ERR;

bool CursesLocalIO::KeyPressed() {
  if (last_key_pressed != ERR) {
    return true;
  }
  auto* w = std::any_cast<WINDOW*>(window_->window());
  nodelay(w, TRUE);
  last_key_pressed = window_->GetChar(std::chrono::milliseconds(0));
  return last_key_pressed != ERR;
}

static int CursesToWin32KeyCodes(int curses_code) {
  switch (curses_code) {
  case KEY_F(1):
    return F1;
  case KEY_F(2):
    return F2;
  case KEY_F(3):
    return F3;
  case KEY_F(4):
    return F4;
  case KEY_F(5):
    return F5;
  case KEY_F(6):
    return F6;
  case KEY_F(7):
    return F7;
  case KEY_F(8):
    return F8;
  case KEY_F(9):
    return F9;
  case KEY_F(10):
    return F10;

#ifdef __PDCURSES__
  case KEY_A1:
    return HOME;
  case KEY_A2: // upper middle on Virt. keypad. Win10 Terminal started using this
    return UPARROW;
  case KEY_A3:
    return PGUP;
  case KEY_B1: // middle left on Virt. keypad. Win10 Terminal started using this.
    return LARROW;
  case KEY_B3: // middle right on Vir. keypad. Win10 Terminal started using this.
    return RARROW;
  case KEY_C1:
    return END;
  case KEY_C2: // lower middle on Virt. keypad. Win10 Terminal started using this.
    return DNARROW;
  case KEY_C3:
    return PGDN;
#endif

  case KEY_UP:
    return UPARROW;
  case KEY_DOWN:
    return DNARROW;
  case KEY_HOME:
    return HOME;
  case KEY_END:
    return END;
  case KEY_LEFT:
    return LARROW;
  case KEY_RIGHT:
    return RARROW;
  case KEY_DELETE:
  case KEY_BACKSPACE:
    return BACKSPACE;
  case KEY_PPAGE:
    return PAGEUP;
  case KEY_NPAGE:
    return PAGEDOWN;
  // TODO: implement the rest.
  default: {
    LOG(INFO) << "unknown key:" << curses_code;
    return 0;
  }
  }
}

unsigned char CursesLocalIO::GetChar() {
  if (last_key_pressed != ERR) {
    const auto ch = last_key_pressed;
    if (ch > 255) {
      // special key
      switch (ch) {
      case KEY_DELETE:
      case KEY_BACKSPACE:
        last_key_pressed = ERR;
        return static_cast<unsigned char>(BACKSPACE);

      default:
        last_key_pressed = CursesToWin32KeyCodes(ch);
        return 0;
      }
    }
    last_key_pressed = ERR;
    return static_cast<unsigned char>(ch);
  }
  auto* w = std::any_cast<WINDOW*>(window_->window());
  nodelay(w, FALSE);
  last_key_pressed = ERR;
  return static_cast<unsigned char>(window_->GetChar());
}

void CursesLocalIO::SetCursor(int cursorStyle) { curs_set(cursorStyle); }

void CursesLocalIO::ClrEol() {
  SetColor(curatr());
  window_->ClrtoEol();
}

void CursesLocalIO::WriteScreenBuffer(const char* buffer) {
  // TODO(rushfan): Optimize me.
  auto* p = buffer;
  auto* w = std::any_cast<WINDOW*>(window_->window());
  scrollok(w, false);

  char s[2];
  s[1] = 0;
  for (auto y = 0; y < 25; y++) {
    for (auto x = 0; x < 80; x++) {
      s[0] = *p++;
      curatr(*p++);
      PutsXY(x, y, s);
    }
  }
  scrollok(w, true);
}

int CursesLocalIO::GetDefaultScreenBottom() const noexcept { return window_->GetMaxY() - 1; }

static int GetEditLineStringLength(const char* text) {
  auto i = ssize(text);
  while (i >= 0 && (static_cast<unsigned char>(text[i - 1]) == 176)) {
    --i;
  }
  return i;
}

void CursesLocalIO::EditLine(char* s, int len, AllowedKeys allowed_keys, EditlineResult* returncode,
                             const char* allowed_set) {
  const auto oldatr = curatr();
  const auto cx = WhereX();
  const auto cy = WhereY();
  for (auto i = ssize(s); i < len; i++) {
    s[i] = '\xb0';
  }
  s[len] = '\0';
  curatr(GetEditLineColor());
  FastPuts(s);
  GotoXY(cx, cy);
  auto done = false;
  auto pos = 0;
  auto insert = false;
  do {
    auto ch = GetChar();
    if (ch == 0 || ch == 224) {
      ch = GetChar();
      switch (ch) {
      case F1:
        done = true;
        *returncode = EditlineResult::DONE;
        break;
      case HOME:
        pos = 0;
        GotoXY(cx, cy);
        break;
      case END:
        pos = GetEditLineStringLength(s);
        GotoXY(cx + pos, cy);
        break;
      case RARROW:
        if (pos < GetEditLineStringLength(s)) {
          pos++;
          GotoXY(cx + pos, cy);
        }
        break;
      case LARROW:
        if (pos > 0) {
          pos--;
          GotoXY(cx + pos, cy);
        }
        break;
      case UARROW:
      case CO:
        done = true;
        *returncode = EditlineResult::PREV;
        break;
      case DARROW:
        done = true;
        *returncode = EditlineResult::NEXT;
        break;
      case KEY_INSERT:
        if (allowed_keys != AllowedKeys::SET) {
          insert = !insert;
        }
        break;
      case KEY_DELETE:
        if (allowed_keys != AllowedKeys::SET) {
          for (int i = pos; i < len; i++) {
            s[i] = s[i + 1];
          }
          s[len - 1] = '\xb0';
          PutsXY(cx, cy, s);
          GotoXY(cx + pos, cy);
        }
        break;
      }
    } else {
      if (ch > 31) {
        if (allowed_keys == AllowedKeys::UPPER_ONLY) {
          ch = to_upper_case<unsigned char>(ch);
        }
        if (allowed_keys == AllowedKeys::SET) {
          ch = to_upper_case<unsigned char>(ch);
          if (ch != SPACE) {
            auto bLookingForSpace = true;
            for (auto i = 0; i < len; i++) {
              if (ch == allowed_set[i] && bLookingForSpace) {
                bLookingForSpace = false;
                pos = i;
                GotoXY(cx + pos, cy);
                if (s[pos] == SPACE) {
                  ch = allowed_set[pos];
                } else {
                  ch = SPACE;
                }
              }
            }
            if (bLookingForSpace) {
              ch = allowed_set[pos];
            }
          }
        }
        if ((pos < len) &&
            ((allowed_keys == AllowedKeys::ALL) || (allowed_keys == AllowedKeys::UPPER_ONLY) ||
             (allowed_keys == AllowedKeys::SET) ||
             ((allowed_keys == AllowedKeys::NUM_ONLY) &&
              (((ch >= '0') && (ch <= '9')) || (ch == SPACE))))) {
          if (insert) {
            for (int i = len - 1; i > pos; i--) {
              s[i] = s[i - 1];
            }
            s[pos++] = static_cast<char>(ch);
            PutsXY(cx, cy, s);
            GotoXY(cx + pos, cy);
          } else {
            s[pos++] = static_cast<char>(ch);
            Putch(ch);
          }
        }
      } else {
        // ch is > 32
        switch (ch) {
        case RETURN:
        case TAB:
          done = true;
          *returncode = EditlineResult::NEXT;
          break;
        case ESC:
          done = true;
          *returncode = EditlineResult::ABORTED;
          break;
        case CA:
          pos = 0;
          GotoXY(cx, cy);
          break;
        case CE:
          pos = GetEditLineStringLength(s); // len;
          GotoXY(cx + pos, cy);
          break;
        case BACKSPACE:
          if (pos > 0) {
            if (insert) {
              for (int i = pos - 1; i < len; i++) {
                s[i] = s[i + 1];
              }
              s[len - 1] = '\xb0';
              pos--;
              PutsXY(cx, cy, s);
              GotoXY(cx + pos, cy);
            } else {
              const auto string_len = GetEditLineStringLength(s);
              pos--;
              if (pos == (string_len - 1)) {
                s[pos] = '\xb0';
              } else {
                s[pos] = SPACE;
              }
              PutsXY(cx, cy, s);
              GotoXY(cx + pos, cy);
            }
          }
          break;
        }
      }
    }
  } while (!done);

  auto z = ssize(s);
  while (z >= 0 && static_cast<unsigned char>(s[z - 1]) == 176) {
    --z;
  }
  s[z] = '\0';

  char szFinishedString[260];
  snprintf(szFinishedString, sizeof(szFinishedString), "%-255s", s);
  szFinishedString[len] = '\0';
  GotoXY(cx, cy);
  curatr(oldatr);
  FastPuts(szFinishedString);
  GotoXY(cx, cy);
}

void CursesLocalIO::MakeLocalWindow(int x, int y, int xlen, int ylen) {
  // Make sure that we are within the range of {(0,0), (80,GetScreenBottom())}
  curatr(GetUserEditorColor());
  xlen = std::min(xlen, 80);
  if (ylen > (GetScreenBottom() + 1 - GetTopLine())) {
    ylen = (GetScreenBottom() + 1 - GetTopLine());
  }
  if ((x + xlen) > 80) {
    x = 80 - xlen;
  }
  if ((y + ylen) > GetScreenBottom() + 1) {
    y = GetScreenBottom() + 1 - ylen;
  }

  const auto xx = WhereX();
  const auto yy = WhereY();

  // we expect to be offset by GetTopLine()
  y += GetTopLine();

  // Loop through Y, each time looping through X adding the right character
  for (int yloop = 0; yloop < ylen; yloop++) {
    for (int xloop = 0; xloop < xlen; xloop++) {
      curatr(static_cast<int16_t>(GetUserEditorColor()));
      if ((yloop == 0) || (yloop == ylen - 1)) {
        PutsXY(x + xloop, y + yloop, "\xC4"); // top and bottom
      } else {
        if ((xloop == 0) || (xloop == xlen - 1)) {
          PutsXY(x + xloop, y + yloop, "\xB3"); // right and left sides
        } else {
          PutsXY(x + xloop, y + yloop, "\x20"); // nothing... Just filler (space)
        }
      }
    }
  }

  // sum of the lengths of the previous lines (+0) is the start of next line

  /*
  ci[0].Char.AsciiChar = '\xDA';      // upper left
  ci[xlen - 1].Char.AsciiChar = '\xBF';    // upper right

  ci[xlen * (ylen - 1)].Char.AsciiChar = '\xC0';  // lower left
  ci[xlen * (ylen - 1) + xlen - 1].Char.AsciiChar = '\xD9'; // lower right
  */
  GotoXY(xx, yy);
}

void CursesLocalIO::UpdateNativeTitleBar(const std::string& system_name, int instance_number) {
#ifdef _WIN32
  // Set console title
  const auto s = StrCat("WWIV Node ", instance_number, " (", system_name, ")");
  SetConsoleTitle(s.c_str());
#endif // _WIN32
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void CursesLocalIO::ResetColors() { InitPairs(); }

void CursesLocalIO::DisableLocalIO() { endwin(); }

void CursesLocalIO::ReenableLocalIO() {
  refresh();
  window_->Refresh();
}

}