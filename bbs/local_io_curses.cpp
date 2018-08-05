//**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
/*    either  express  or impliedf.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
// Always declare wwiv_windows.h first to avoid collisions on defines.
#include "core/wwiv_windows.h"

#include "bbs/local_io_curses.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <string>
#include <fcntl.h>
#ifdef __unix__
#include <unistd.h>
#endif

#include "bbs/asv.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsovl2.h"
#include "bbs/confutil.h"
#include "bbs/datetime.h"
#include "bbs/bbs.h"
#include "bbs/vars.h"
#include "bbs/remote_io.h"
#include "bbs/wconstants.h"
#include "sdk/status.h"
#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "localui/wwiv_curses.h"
#include "localui/curses_io.h"
#include "localui/curses_win.h"

using std::string;
using namespace wwiv::strings;

extern CursesIO* out;

static const int default_screen_bottom = 20;

static void InitPairs() {
  std::vector<short> lowbit_colors = {
    COLOR_BLACK,
    COLOR_BLUE,
    COLOR_GREEN,
    COLOR_CYAN,
    COLOR_RED,
    COLOR_MAGENTA,
    COLOR_YELLOW,
    COLOR_WHITE};
  short num = 0;
  for (int bg = 0; bg < 8; bg++) {
    for (int fg = 0; fg < 8; fg++) {
      init_pair(num++, lowbit_colors[fg], lowbit_colors[bg]);
    }
  }
 }

CursesLocalIO::CursesLocalIO() : CursesLocalIO(default_screen_bottom + 1) {}

CursesLocalIO::CursesLocalIO(int num_lines) {
  InitPairs();
  window_.reset(new CursesWindow(nullptr, out->color_scheme(), num_lines, 80, 0, 0));
  auto* w = reinterpret_cast<WINDOW*>(window_->window());
  scrollok(w, true);
  window_->Clear();
}

CursesLocalIO::~CursesLocalIO() {
  SetCursor(LocalIO::cursorNormal);
}

void CursesLocalIO::SetColor(int original_color) {
  bool bold = (original_color & 8) != 0;
  int color = original_color;
  int bg = (color >> 4) & 0x07;
  int fg = color & 0x07;
  color = (bg << 3) | fg;
  attr_t attr = COLOR_PAIR(color);
  if (bold) {
    attr |= A_BOLD;
  }
  window_->AttrSet(attr);
}

void CursesLocalIO::GotoXY(int x, int y) {
  window_->GotoXY(x, y);
}

int CursesLocalIO::WhereX() const noexcept {
  return window_->GetcurX();
}

int CursesLocalIO::WhereY() const noexcept {
  return window_->GetcurY();
}

void CursesLocalIO::Lf() {
  Cr();
  y_ = WhereY();
  x_ = WhereX();
  y_++;

  if (y_ > GetScreenBottom()) {
    auto* w = reinterpret_cast<WINDOW*>(window_->window());
    scroll(w);
    y_ = GetScreenBottom();
  }
  window_->GotoXY(x_, y_);
}

void CursesLocalIO::Cr() {
  int y = WhereY();
  window_->GotoXY(0, y);
}

void CursesLocalIO::Cls() {
  SetColor(curatr());
  window_->Clear();
}

void CursesLocalIO::Backspace() {
  SetColor(curatr());
  window_->Putch('\b');
}

void CursesLocalIO::PutchRaw(unsigned char ch) {
  SetColor(curatr());
  window_->Putch(ch);
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
    if (!outcom) {
      // TODO Make the bell sound configurable.
      // wwiv::os::sound(500, milliseconds(4));
    }
  }
}

void CursesLocalIO::Puts(const string& s) {
  for (char ch : s) {
    Putch(ch);
  }
}

void CursesLocalIO::PutsXY(int x, int y, const string& text) {
  GotoXY(x, y);
  FastPuts(text);
}

void CursesLocalIO::PutsXYA(int x, int y, int a, const string& text) {
  auto old_color = curatr();
  curatr(a);

  GotoXY(x, y);
  FastPuts(text);

  curatr(old_color);
}

void CursesLocalIO::FastPuts(const string& text) {
  SetColor(curatr());
  window_->Puts(text);
}

int CursesLocalIO::PrintfXY(int x, int y, const char *formatted_text, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, formatted_text);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), formatted_text, ap);
  va_end(ap);
  PutsXY(x, y, szBuffer);
  return nNumWritten;
}

int CursesLocalIO::PrintfXYA(int x, int y, int nAttribute, const char *formatted_text, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, formatted_text);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), formatted_text, ap);
  va_end(ap);

  PutsXYA(x, y, nAttribute, szBuffer);
  return nNumWritten;
}

void CursesLocalIO::set_protect(Application* session, int l) {
  SetTopLine(l);
  if (!session->using_modem) {
    session->screenlinest = session->defscreenbottom + 1 - GetTopLine();
  }
}

static std::vector<chtype*> saved_screen;
void CursesLocalIO::savescreen() {
  saved_screen.clear();
  window_->Refresh();

  int width = window_->GetMaxX();
  int height = window_->GetMaxY();

  for (int line = 0; line < height; line++) {
    chtype* buf = new chtype[width];
    auto* w = reinterpret_cast<WINDOW*>(window_->window());
    mvwinchnstr(w, line, 0, buf, width);
    saved_screen.push_back(buf);
  }
}

void CursesLocalIO::restorescreen() {
  int width = window_->GetMaxX();
  int y = 0;
  auto* w = reinterpret_cast<WINDOW*>(window_->window());
  for (auto line : saved_screen) {
    mvwaddchnstr(w, y++, 0, line, width);
  }
  window_->Refresh();
}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4125 4100 )
#endif

static int last_key_pressed = ERR;

bool CursesLocalIO::KeyPressed() {
  if (last_key_pressed != ERR) {
    return true;
  }
  auto* w = reinterpret_cast<WINDOW*>(window_->window());
  nodelay(w, TRUE);
  last_key_pressed = window_->GetChar();
  return last_key_pressed != ERR;
}

static int CursesToWin32KeyCodes(int curses_code) {
  switch (curses_code) {
  case KEY_F(1): return F1;
  case KEY_F(2): return F2;
  case KEY_F(3): return F3;
  case KEY_F(4): return F4;
  case KEY_F(5): return F5;
  case KEY_F(6): return F6;
  case KEY_F(7): return F7;
  case KEY_F(8): return F8;
  case KEY_F(9): return F9;
  case KEY_F(10): return F10;

  case KEY_UP: return UPARROW;
  case KEY_DOWN: return DNARROW;
  case KEY_HOME: return HOME;
  case KEY_END: return END;
  case KEY_LEFT: return LARROW;
  case KEY_RIGHT: return RARROW;
  case KEY_DELETE: return BACKSPACE;
  case KEY_BACKSPACE: return BACKSPACE;
  case KEY_PPAGE: return PAGEUP;
  case KEY_NPAGE: return PAGEDOWN;
  // TODO: implement the rest.
  default: {
    LOG(INFO) << "unknown key:"  << curses_code;
    return 0;
  }
  }
}

unsigned char CursesLocalIO::GetChar() {
  if (last_key_pressed != ERR) {
    int ch = last_key_pressed;
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
  auto* w = reinterpret_cast<WINDOW*>(window_->window());
  nodelay(w, FALSE);
  last_key_pressed = ERR;
  return static_cast<unsigned char>(window_->GetChar());
}

void CursesLocalIO::SetCursor(int cursorStyle) {
  curs_set(cursorStyle);
}

void CursesLocalIO::ClrEol() {
  SetColor(curatr());
  window_->ClrtoEol();
}

void CursesLocalIO::WriteScreenBuffer(const char *buffer) {
  // TODO(rushfan): Optimize me.
  const char *p = buffer;
  auto* w = reinterpret_cast<WINDOW*>(window_->window());
  scrollok(w, false);

  char s[2];
  s[1] = 0;
  for (int y = 0; y < 25; y++) {
	for (int x = 0; x < 80; x++) {
	  s[0] = *p++;
    curatr(*p++);
	  PutsXY(x, y, s);
    }
  }
  scrollok(w, true);
}

int CursesLocalIO::GetDefaultScreenBottom() const noexcept { return window_->GetMaxY() - 1; }

// copied from local_io_win32.cpp
#define PREV                1
#define NEXT                2
#define DONE                4
#define ABORTED             8

static int GetEditLineStringLength(const char *text) {
  int i = strlen(text);
  while (i >= 0 && (static_cast<unsigned char>(text[i - 1]) == 176)) {
    --i;
  }
  return i;
}

void CursesLocalIO::EditLine(char *pszInOutText, int len, AllowedKeys allowed_keys,
  int *returncode, const char *pszAllowedSet) {
  int oldatr = curatr();
  int cx = WhereX();
  int cy = WhereY();
  for (auto i = strlen(pszInOutText); i < static_cast<size_t>(len); i++) {
    pszInOutText[i] = static_cast<unsigned char>(176);
  }
  pszInOutText[len] = '\0';
  curatr(GetEditLineColor());
  FastPuts(pszInOutText);
  GotoXY(cx, cy);
  bool done = false;
  int pos = 0;
  bool insert = false;
  do {
    unsigned char ch = GetChar();
    if (ch == 0 || ch == 224) {
      ch = GetChar();
      switch (ch) {
      case F1:
        done = true;
        *returncode = DONE;
        break;
      case HOME:
        pos = 0;
        GotoXY(cx, cy);
        break;
      case END:
        pos = GetEditLineStringLength(pszInOutText);
        GotoXY(cx + pos, cy);
        break;
      case RARROW:
        if (pos < GetEditLineStringLength(pszInOutText)) {
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
        *returncode = PREV;
        break;
      case DARROW:
        done = true;
        *returncode = NEXT;
        break;
      case INSERT:
        if (allowed_keys != AllowedKeys::SET) {
          insert = !insert;
        }
        break;
      case KEY_DELETE:
        if (allowed_keys != AllowedKeys::SET) {
          for (int i = pos; i < len; i++) {
            pszInOutText[i] = pszInOutText[i + 1];
          }
          pszInOutText[len - 1] = static_cast<unsigned char>(176);
          PutsXY(cx, cy, pszInOutText);
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
            bool bLookingForSpace = true;
            for (int i = 0; i < len; i++) {
              if (ch == pszAllowedSet[i] && bLookingForSpace) {
                bLookingForSpace = false;
                pos = i;
                GotoXY(cx + pos, cy);
                if (pszInOutText[pos] == SPACE) {
                  ch = pszAllowedSet[pos];
                } else {
                  ch = SPACE;
                }
              }
            }
            if (bLookingForSpace) {
              ch = pszAllowedSet[pos];
            }
          }
        }
        if ((pos < len) && ((allowed_keys == AllowedKeys::ALL) 
            || (allowed_keys == AllowedKeys::UPPER_ONLY) || (allowed_keys == AllowedKeys::SET)
            || ((allowed_keys == AllowedKeys::NUM_ONLY) 
            && (((ch >= '0') && (ch <= '9')) || (ch == SPACE))))) {
          if (insert) {
            for (int i = len - 1; i > pos; i--) {
              pszInOutText[i] = pszInOutText[i - 1];
            }
            pszInOutText[pos++] = ch;
            PutsXY(cx, cy, pszInOutText);
            GotoXY(cx + pos, cy);
          } else {
            pszInOutText[pos++] = ch;
            Putch(ch);
          }
        }
      } else {
        // ch is > 32
        switch (ch) {
        case RETURN:
        case TAB:
          done = true;
          *returncode = NEXT;
          break;
        case ESC:
          done = true;
          *returncode = ABORTED;
          break;
        case CA:
          pos = 0;
          GotoXY(cx, cy);
          break;
        case CE:
          pos = GetEditLineStringLength(pszInOutText);   // len;
          GotoXY(cx + pos, cy);
          break;
        case BACKSPACE:
          if (pos > 0) {
            if (insert) {
              for (int i = pos - 1; i < len; i++) {
                pszInOutText[i] = pszInOutText[i + 1];
              }
              pszInOutText[len - 1] = static_cast<unsigned char>(176);
              pos--;
              PutsXY(cx, cy, pszInOutText);
              GotoXY(cx + pos, cy);
            } else {
              int nStringLen = GetEditLineStringLength(pszInOutText);
              pos--;
              if (pos == (nStringLen - 1)) {
                pszInOutText[pos] = static_cast<unsigned char>(176);
              } else {
                pszInOutText[pos] = SPACE;
              }
              PutsXY(cx, cy, pszInOutText);
              GotoXY(cx + pos, cy);
            }
          }
          break;
        }
      }
    }
  } while (!done);

  int z = strlen(pszInOutText);
  while (z >= 0 && static_cast<unsigned char>(pszInOutText[z - 1]) == 176) {
    --z;
  }
  pszInOutText[z] = '\0';

  char szFinishedString[260];
  snprintf(szFinishedString, sizeof(szFinishedString), "%-255s", pszInOutText);
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

  int xx = WhereX();
  int yy = WhereY();

  // we expect to be offset by GetTopLine()
  y += GetTopLine();

  // Loop through Y, each time looping through X adding the right character
  for (int yloop = 0; yloop < ylen; yloop++) {
    for (int xloop = 0; xloop < xlen; xloop++) {
      curatr(static_cast<int16_t>(GetUserEditorColor()));
      if ((yloop == 0) || (yloop == ylen - 1)) {
        PutsXY(x + xloop, y + yloop, "\xC4");      // top and bottom
      } else {
        if ((xloop == 0) || (xloop ==xlen - 1)) {
          PutsXY(x + xloop, y + yloop, "\xB3");  // right and left sides
        } else {
          PutsXY(x + xloop, y + yloop, "\x20");   // nothing... Just filler (space)
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

void CursesLocalIO::UpdateNativeTitleBar(Application* session) {
#ifdef _WIN32
	// Set console title
	std::stringstream ss;
	ss << "WWIV Node " << session->instance_number() 
     << " (" << a()->config()->system_name() << ")";
	SetConsoleTitle(ss.str().c_str());
#endif  // _WIN32
}

void CursesLocalIO::ResetColors() {
	InitPairs();
}

void CursesLocalIO::DisableLocalIO() {
  endwin();
}

void CursesLocalIO::ReenableLocalIO() {
  refresh();
  window_->Refresh();
}

#if defined( _MSC_VER )
#pragma warning( pop )
#endif // _MSC_VER
