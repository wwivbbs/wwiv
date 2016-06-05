//**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include "bbs/wwiv_windows.h"

#include "bbs/local_io_curses.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <string>
#include <fcntl.h>
#ifdef __unix__
// TODO(rushfan): Remove this
#include <unistd.h>
#endif

#include "curses.h"

#include "bbs/asv.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsovl2.h"
#include "bbs/confutil.h"
#include "bbs/datetime.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/remote_io.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "core/file.h"
#include "core/os.h"
#include "core/strings.h"
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
  scrollok(window_->window(), true);
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

void CursesLocalIO::LocalGotoXY(int x, int y) {
  window_->GotoXY(x, y);
}

size_t CursesLocalIO::WhereX() {
  return window_->GetcurX();
}

size_t CursesLocalIO::WhereY() {
  return window_->GetcurY();
}

void CursesLocalIO::LocalLf() {
  y_ = WhereY();
  x_ = WhereX();
  y_++;

  if (y_ > GetScreenBottom()) { // GetScreenBottom()) {
    scroll(window_->window());
    y_ = GetScreenBottom();
  }
  window_->GotoXY(x_, y_);
}

void CursesLocalIO::LocalCr() {
  int y = WhereY();
  window_->GotoXY(0, y);
}

void CursesLocalIO::LocalCls() {
  SetColor(curatr);
  window_->Clear();
}

void CursesLocalIO::LocalBackspace() {
  SetColor(curatr);
  window_->Putch('\b');
}

void CursesLocalIO::LocalPutchRaw(unsigned char ch) {
  SetColor(curatr);
  window_->Putch(ch);
}

void CursesLocalIO::LocalPutch(unsigned char ch) {
  if (ch > 31) {
    LocalPutchRaw(ch);
  } else if (ch == CM) {
    LocalCr();
  } else if (ch == CJ) {
    LocalLf();
  } else if (ch == CL) {
    LocalCls();
  } else if (ch == BACKSPACE) {
    LocalBackspace();
  } else if (ch == CG) {
    if (!outcom) {
      // TODO Make the bell sound configurable.
      // wwiv::os::sound(500, milliseconds(4));
    }
  }
}

void CursesLocalIO::LocalPuts(const string& s) {
  for (char ch : s) {
    LocalPutch(ch);
  }
}

void CursesLocalIO::LocalXYPuts(int x, int y, const string& text) {
  LocalGotoXY(x, y);
  LocalFastPuts(text);
}

void CursesLocalIO::LocalFastPuts(const string& text) {
  SetColor(curatr);
  window_->Puts(text.c_str());
}

int CursesLocalIO::LocalPrintf(const char *formatted_text, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, formatted_text);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), formatted_text, ap);
  va_end(ap);
  LocalFastPuts(szBuffer);
  return nNumWritten;
}

int CursesLocalIO::LocalXYPrintf(int x, int y, const char *formatted_text, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, formatted_text);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), formatted_text, ap);
  va_end(ap);
  LocalXYPuts(x, y, szBuffer);
  return nNumWritten;
}

int CursesLocalIO::LocalXYAPrintf(int x, int y, int nAttribute, const char *formatted_text, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, formatted_text);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), formatted_text, ap);
  va_end(ap);

  int nOldColor = curatr;
  curatr = nAttribute;
  LocalXYPuts(x, y, szBuffer);
  curatr = nOldColor;
  return nNumWritten;
}

void CursesLocalIO::set_protect(WSession* session, int l) {
  SetTopLine(l);
  if (!session->using_modem) {
    session->screenlinest = defscreenbottom + 1 - GetTopLine();
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
    mvwinchnstr(window_->window(), line, 0, buf, width);
    saved_screen.push_back(buf);
  }
}

void CursesLocalIO::restorescreen() {
  int width = window_->GetMaxX();
  int y = 0;
  for (auto line : saved_screen) {
    mvwaddchnstr(window_->window(), y++, 0, line, width);
  }
  window_->Refresh();
}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4125 4100 )
#endif

static int last_key_pressed = ERR;

bool CursesLocalIO::LocalKeyPressed() {
  if (last_key_pressed != ERR) {
    return true;
  }
  nodelay(window_->window(), TRUE);
  last_key_pressed = window_->GetChar();
  return last_key_pressed != ERR;
}

void CursesLocalIO::SaveCurrentLine(char *cl, char *atr, char *xl, char *cc) {
  *cl = 0;
  *atr = 0;
  strcpy(xl, endofline);
  *cc = static_cast<char>(curatr);
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
  }
  return curses_code;
}

unsigned char CursesLocalIO::LocalGetChar() {
  if (last_key_pressed != ERR) {
    int ch = last_key_pressed;
    if (ch > 255) {
      // special key
      last_key_pressed = CursesToWin32KeyCodes(ch);
      return 0;
    }
    last_key_pressed = ERR;
    return static_cast<unsigned char>(ch);
  }
  nodelay(window_->window(), FALSE);
  last_key_pressed = ERR;
  return static_cast<unsigned char>(window_->GetChar());
}

void CursesLocalIO::SetCursor(int cursorStyle) {
  curs_set(cursorStyle);
}

void CursesLocalIO::LocalClrEol() {
  SetColor(curatr);
  window_->ClrtoEol();
}

void CursesLocalIO::LocalWriteScreenBuffer(const char *buffer) {
  // TODO(rushfan): Optimize me.
  const char *p = buffer;

  char s[2];
  s[1] = 0;
  for (int y = 0; y < 25; y++) {
	for (int x = 0; x < 80; x++) {
	  s[0] = *p++;
    curatr = *p++;
	  LocalXYPuts(x, y, s);
    }
  }
}

size_t CursesLocalIO::GetDefaultScreenBottom() { return window_->GetMaxY() - 1; }

// copied from local_io_win32.cpp
#define PREV                1
#define NEXT                2
#define DONE                4
#define ABORTED             8

static int GetEditLineStringLength(const char *text) {
  size_t i = strlen(text);
  while (i >= 0 && (static_cast<unsigned char>(text[i - 1]) == 176)) {
    --i;
  }
  return i;
}

void CursesLocalIO::LocalEditLine(char *pszInOutText, int len, int editor_status,
  int *returncode, char *pszAllowedSet) {
  int oldatr = curatr;
  int cx = WhereX();
  int cy = WhereY();
  for (size_t i = strlen(pszInOutText); i < static_cast<size_t>(len); i++) {
    pszInOutText[i] = static_cast<unsigned char>(176);
  }
  pszInOutText[len] = '\0';
  curatr = GetEditLineColor();
  LocalFastPuts(pszInOutText);
  LocalGotoXY(cx, cy);
  bool done = false;
  int pos = 0;
  bool insert = false;
  do {
    unsigned char ch = LocalGetChar();
    if (ch == 0 || ch == 224) {
      ch = LocalGetChar();
      switch (ch) {
      case F1:
        done = true;
        *returncode = DONE;
        break;
      case HOME:
        pos = 0;
        LocalGotoXY(cx, cy);
        break;
      case END:
        pos = GetEditLineStringLength(pszInOutText);
        LocalGotoXY(cx + pos, cy);
        break;
      case RARROW:
        if (pos < GetEditLineStringLength(pszInOutText)) {
          pos++;
          LocalGotoXY(cx + pos, cy);
        }
        break;
      case LARROW:
        if (pos > 0) {
          pos--;
          LocalGotoXY(cx + pos, cy);
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
        if (editor_status != SET) {
          insert = !insert;
        }
        break;
      case KEY_DELETE:
        if (editor_status != SET) {
          for (int i = pos; i < len; i++) {
            pszInOutText[i] = pszInOutText[i + 1];
          }
          pszInOutText[len - 1] = static_cast<unsigned char>(176);
          LocalXYPuts(cx, cy, pszInOutText);
          LocalGotoXY(cx + pos, cy);
        }
        break;
      }
    } else {
      if (ch > 31) {
        if (editor_status == UPPER_ONLY) {
          ch = wwiv::UpperCase<unsigned char>(ch);
        }
        if (editor_status == SET) {
          ch = wwiv::UpperCase<unsigned char>(ch);
          if (ch != SPACE) {
            bool bLookingForSpace = true;
            for (int i = 0; i < len; i++) {
              if (ch == pszAllowedSet[i] && bLookingForSpace) {
                bLookingForSpace = false;
                pos = i;
                LocalGotoXY(cx + pos, cy);
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
        if ((pos < len) && ((editor_status == ALL) || (editor_status == UPPER_ONLY) || (editor_status == SET) ||
          ((editor_status == NUM_ONLY) && (((ch >= '0') && (ch <= '9')) || (ch == SPACE))))) {
          if (insert) {
            for (int i = len - 1; i > pos; i--) {
              pszInOutText[i] = pszInOutText[i - 1];
            }
            pszInOutText[pos++] = ch;
            LocalXYPuts(cx, cy, pszInOutText);
            LocalGotoXY(cx + pos, cy);
          } else {
            pszInOutText[pos++] = ch;
            LocalPutch(ch);
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
          LocalGotoXY(cx, cy);
          break;
        case CE:
          pos = GetEditLineStringLength(pszInOutText);   // len;
          LocalGotoXY(cx + pos, cy);
          break;
        case BACKSPACE:
          if (pos > 0) {
            if (insert) {
              for (int i = pos - 1; i < len; i++) {
                pszInOutText[i] = pszInOutText[i + 1];
              }
              pszInOutText[len - 1] = static_cast<unsigned char>(176);
              pos--;
              LocalXYPuts(cx, cy, pszInOutText);
              LocalGotoXY(cx + pos, cy);
            } else {
              int nStringLen = GetEditLineStringLength(pszInOutText);
              pos--;
              if (pos == (nStringLen - 1)) {
                pszInOutText[pos] = static_cast<unsigned char>(176);
              } else {
                pszInOutText[pos] = SPACE;
              }
              LocalXYPuts(cx, cy, pszInOutText);
              LocalGotoXY(cx + pos, cy);
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
  LocalGotoXY(cx, cy);
  curatr = oldatr;
  LocalFastPuts(szFinishedString);
  LocalGotoXY(cx, cy);
}

void CursesLocalIO::MakeLocalWindow(int x, int y, int xlen, int ylen) {
  // Make sure that we are within the range of {(0,0), (80,GetScreenBottom())}
  curatr = GetUserEditorColor();
  xlen = std::min(xlen, 80);
  if (static_cast<size_t>(ylen) > (GetScreenBottom() + 1 - GetTopLine())) {
    ylen = (GetScreenBottom() + 1 - GetTopLine());
  }
  if ((x + xlen) > 80) {
    x = 80 - xlen;
  }
  if (static_cast<size_t>(y + ylen) > GetScreenBottom() + 1) {
    y = GetScreenBottom() + 1 - ylen;
  }

  int xx = WhereX();
  int yy = WhereY();

  // we expect to be offset by GetTopLine()
  y += GetTopLine();

  // Loop through Y, each time looping through X adding the right character
  for (int yloop = 0; yloop < ylen; yloop++) {
    for (int xloop = 0; xloop < xlen; xloop++) {
      curatr = static_cast<int16_t>(GetUserEditorColor());
      if ((yloop == 0) || (yloop == ylen - 1)) {
        LocalXYPuts(x + xloop, y + yloop, "\xC4");      // top and bottom
      } else {
        if ((xloop == 0) || (xloop ==xlen - 1)) {
          LocalXYPuts(x + xloop, y + yloop, "\xB3");  // right and left sides
        } else {
          LocalXYPuts(x + xloop, y + yloop, "\x20");   // nothing... Just filler (space)
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
  LocalGotoXY(xx, yy);
}

void CursesLocalIO::UpdateNativeTitleBar(WSession* session) {
#ifdef _WIN32
	// Set console title
	std::stringstream consoleTitleStream;
	consoleTitleStream << "WWIV Node " << session->instance_number() << " (" << syscfg.systemname << ")";
	SetConsoleTitle(consoleTitleStream.str().c_str());
#endif  // _WIN32
}

void CursesLocalIO::ResetColors() {
	InitPairs();
}

#if defined( _MSC_VER )
#pragma warning( pop )
#endif // _MSC_VER
