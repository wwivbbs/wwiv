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
  std::vector<int> lowbit_colors = {
    COLOR_BLACK,
    COLOR_BLUE,
    COLOR_GREEN,
    COLOR_CYAN,
    COLOR_RED,
    COLOR_MAGENTA,
    COLOR_YELLOW,
    COLOR_WHITE};
  int num = 0;
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
  bool bold = (original_color & 8);
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

void CursesLocalIO::savescreen() {}
void CursesLocalIO::restorescreen() {}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4125 4100 )
#endif

void CursesLocalIO::tleft(WSession* session, bool temp_sysop, bool sysop, bool user_online) {}

static int last_key_pressed = ERR;

bool CursesLocalIO::LocalKeyPressed() {
  if (last_key_pressed != ERR) {
    return true;
  }
  nodelay(window_->window(), TRUE);
  last_key_pressed = window_->GetChar();
  return last_key_pressed != ERR;
}

void CursesLocalIO::SaveCurrentLine(char *cl, char *atr, char *xl, char *cc) {}

unsigned char CursesLocalIO::LocalGetChar() {
  if (last_key_pressed != ERR) {
    int ch = last_key_pressed;
    last_key_pressed = ERR;
    return static_cast<unsigned char>(ch);
  }
  nodelay(window_->window(), FALSE);
  last_key_pressed = ERR;
  return static_cast<unsigned char>(window_->GetChar());
}

void CursesLocalIO::MakeLocalWindow(int x, int y, int xlen, int ylen) {}

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
	  SetColor(*p++);
	  LocalXYPuts(x, y, s);
    }
  }
}

size_t CursesLocalIO::GetDefaultScreenBottom() { return window_->GetMaxY() - 1; }

void CursesLocalIO::LocalEditLine(char *s, int len, int edit_status, int *returncode, char *ss) {}

void CursesLocalIO::UpdateNativeTitleBar(WSession* session) {}

void CursesLocalIO::ResetColors() {
	InitPairs();
}


void CursesLocalIO::UpdateTopScreen(WStatus* pStatus, WSession *pSession, int nInstanceNumber) {}
#if defined( _MSC_VER )
#pragma warning( pop )
#endif // _MSC_VER
