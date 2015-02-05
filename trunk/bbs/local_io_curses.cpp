//**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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

#include "curses.h"

#include "bbs/asv.h"
#include "bbs/datetime.h"
#include "bbs/wwiv.h"
#include "bbs/wcomm.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "core/file.h"
#include "core/os.h"
#include "core/strings.h"
#include "initlib/curses_io.h"
#include "initlib/curses_win.h"

using std::string;
using namespace wwiv::strings;

extern CursesIO* out;

static const int screen_bottom = 20;

static std::map<int, AnsiColor> CreateAnsiScheme() {
  std::map<int, AnsiColor> scheme;

  // Create a default color scheme, ideally this would be loaded from an INI file or some other
  // configuration file, but this is a sufficient start.
  scheme[0] = AnsiColor(COLOR_BLUE, COLOR_BLACK, false);
  scheme[1] = AnsiColor(COLOR_BLUE, COLOR_BLACK, false);
  scheme[2] = AnsiColor(COLOR_GREEN, COLOR_BLACK, false);
  scheme[3] = AnsiColor(COLOR_CYAN, COLOR_BLACK, false);
  scheme[4] = AnsiColor(COLOR_RED, COLOR_BLACK, false);
  scheme[5] = AnsiColor(COLOR_MAGENTA, COLOR_BLACK, false);
  scheme[6] = AnsiColor(COLOR_YELLOW, COLOR_BLACK, false);
  scheme[7] = AnsiColor(COLOR_WHITE, COLOR_BLACK, false);

  scheme[8] = AnsiColor(COLOR_BLACK, COLOR_BLACK, true);
  scheme[9] = AnsiColor(COLOR_BLUE, COLOR_BLACK, true);
  scheme[10] = AnsiColor(COLOR_GREEN, COLOR_BLACK, true);
  scheme[11] = AnsiColor(COLOR_CYAN, COLOR_BLACK, true);
  scheme[12] = AnsiColor(COLOR_RED, COLOR_BLACK, true);
  scheme[13] = AnsiColor(COLOR_MAGENTA, COLOR_BLACK, true);
  scheme[14] = AnsiColor(COLOR_YELLOW, COLOR_BLACK, true);
  scheme[15] = AnsiColor(COLOR_WHITE, COLOR_BLACK, true);
  // TODO(rushfan): Set this correctly.
  scheme[31] = AnsiColor(COLOR_WHITE, COLOR_BLUE, true);

  // Create the color pairs for each of the colors defined in the color scheme.
  for (const auto& kv : scheme) {
    init_pair(kv.first + 100, kv.second.f(), kv.second.b());
  }
  return scheme;
}

CursesLocalIO::CursesLocalIO() : scheme_(CreateAnsiScheme()) {
  window = new CursesWindow(out->window(), out->color_scheme(), screen_bottom + 1, 80, 0, 0);
  scrollok(window->window(), true);
  window->Clear();
}

CursesLocalIO::~CursesLocalIO() {
  delete window;
}

void CursesLocalIO::SetColor(int color) {
  const AnsiColor& s = scheme_.at(color);
  attr_t attr = COLOR_PAIR(color + 100);
  if (s.bold()) {
    attr |= A_BOLD;
  }
  window->AttrSet(attr);
}

void CursesLocalIO::LocalGotoXY(int x, int y) {
  window->GotoXY(x, y);
}

int CursesLocalIO::WhereX() {
  return window->GetcurX();
}

int CursesLocalIO::WhereY() {
  return window->GetcurY();
}

void CursesLocalIO::LocalLf() {
  m_cursorPositionY = WhereY();
  m_cursorPositionX = WhereX();
  m_cursorPositionY++;

  if (m_cursorPositionY > screen_bottom) { // GetScreenBottom()) {
    scroll(window->window());
    m_cursorPositionY = screen_bottom;
  }
  window->GotoXY(m_cursorPositionX, m_cursorPositionY);
}

void CursesLocalIO::LocalCr() {
  int x = WhereX();
  int y = WhereY();
  window->GotoXY(0, y);
}

void CursesLocalIO::LocalCls() {
  SetColor(curatr);
  window->Clear();
}

void CursesLocalIO::LocalBackspace() {
  SetColor(curatr);
  window->Putch('\b');
}

void CursesLocalIO::LocalPutchRaw(unsigned char ch) {
  SetColor(curatr);
  window->Putch(ch);
}

void CursesLocalIO::LocalPutch(unsigned char ch) {
  if (x_only) {
    int wx = capture_->wx();
    if (ch > 31) {
      wx = (wx + 1) % 80;
    } else if (ch == RETURN || ch == CL) {
      wx = 0;
    } else if (ch == BACKSPACE) {
      if (wx) {
        wx--;
      }
    }
    capture_->set_wx(wx);
    return;
  }
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
  window->Puts(text.c_str());
}

int CursesLocalIO::LocalPrintf(const char *pszFormattedText, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, pszFormattedText);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), pszFormattedText, ap);
  va_end(ap);
  LocalFastPuts(szBuffer);
  return nNumWritten;
}

int CursesLocalIO::LocalXYPrintf(int x, int y, const char *pszFormattedText, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, pszFormattedText);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), pszFormattedText, ap);
  va_end(ap);
  LocalXYPuts(x, y, szBuffer);
  return nNumWritten;
}

int CursesLocalIO::LocalXYAPrintf(int x, int y, int nAttribute, const char *pszFormattedText, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, pszFormattedText);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), pszFormattedText, ap);
  va_end(ap);

  int nOldColor = curatr;
  curatr = nAttribute;
  LocalXYPuts(x, y, szBuffer);
  curatr = nOldColor;
  return nNumWritten;
}

void CursesLocalIO::set_protect(int l) {
  SetTopLine(l);
  session()->screenlinest = 
    (session()->using_modem) ? session()->user()->GetScreenLines() :
                               defscreenbottom + 1 - GetTopLine();
}

void CursesLocalIO::savescreen() {}
void CursesLocalIO::restorescreen() {}

void CursesLocalIO::skey(char ch) {
  int nKeyCode = static_cast<unsigned char>(ch);
  int i, i1;

  if ((syscfg.sysconfig & sysconfig_no_local) == 0) {
    if (okskey) {
      if (nKeyCode >= AF1 && nKeyCode <= AF10) {
        set_autoval(nKeyCode - 104);
      } else {
        switch (nKeyCode) {
        case F1:                          /* F1 */
          OnlineUserEditor();
          break;
        case SF1:                          /* Shift-F1 */
          capture_->set_global_handle(!capture_->is_open());
          application()->UpdateTopScreen();
          break;
        case CF1:                          /* Ctrl-F1 */
          application()->ToggleShutDown();
          break;
        case F2:                          /* F2 */
          session()->topdata++;
          if (session()->topdata > CursesLocalIO::topdataUser) {
            session()->topdata = CursesLocalIO::topdataNone;
          }
          application()->UpdateTopScreen();
          break;
        case F3:                          /* F3 */
          if (session()->using_modem) {
            incom = !incom;
            dump();
            tleft(false);
          }
          break;
        case F4:                          /* F4 */
          chatcall = false;
          application()->UpdateTopScreen();
          break;
        case F5:                          /* F5 */
          hangup = true;
          session()->remoteIO()->dtr(false);
          break;
        case SF5:                          /* Shift-F5 */
          i1 = (rand() % 20) + 10;
          for (i = 0; i < i1; i++) {
            bputch(static_cast< unsigned char >(rand() % 256));
          }
          hangup = true;
          session()->remoteIO()->dtr(false);
          break;
        case CF5:                          /* Ctrl-F5 */
          bout << "\r\nCall back later when you are there.\r\n\n";
          hangup = true;
          session()->remoteIO()->dtr(false);
          break;
        case F6:                          /* F6 */
          SetSysopAlert(!GetSysopAlert());
          tleft(false);
          break;
        case F7:                          /* F7 */
          session()->user()->SetExtraTime(session()->user()->GetExtraTime() -
              static_cast<float>(5.0 * SECONDS_PER_MINUTE_FLOAT));
          tleft(false);
          break;
        case F8:                          /* F8 */
          session()->user()->SetExtraTime(session()->user()->GetExtraTime() +
              static_cast<float>(5.0 * SECONDS_PER_MINUTE_FLOAT));
          tleft(false);
          break;
        case F9:                          /* F9 */
          if (session()->user()->GetSl() != 255) {
            if (session()->GetEffectiveSl() != 255) {
              session()->SetEffectiveSl(255);
            } else {
              session()->ResetEffectiveSl();
            }
            changedsl();
            tleft(false);
          }
          break;
        case F10:                          /* F10 */
          if (chatting == 0) {
            char szUnusedChatLine[81];
            szUnusedChatLine[0] = 0;
            if (syscfg.sysconfig & sysconfig_2_way) {
              chat1(szUnusedChatLine, true);
            } else {
              chat1(szUnusedChatLine, false);
            }
          } else {
            chatting = 0;
          }
          break;
        case CF10:                         /* Ctrl-F10 */
          if (chatting == 0) {
            char szUnusedChatLine[81];
            szUnusedChatLine[0] = 0;
            chat1(szUnusedChatLine, false);
          } else {
            chatting = 0;
          }
          break;
        case HOME:                          /* HOME */
          if (chatting == 1) {
            chat_file = !chat_file;
          }
          break;
        default:
          break;
        }
      }
    } else {
    }
  }
}

void CursesLocalIO::tleft(bool bCheckForTimeOut) {}

static int last_key_pressed = ERR;

bool CursesLocalIO::LocalKeyPressed() {
  if (last_key_pressed != ERR) {
    return true;
  }
  nodelay(window->window(), TRUE);
  last_key_pressed = window->GetChar();
  return last_key_pressed != ERR;
}

void CursesLocalIO::SaveCurrentLine(char *cl, char *atr, char *xl, char *cc) {}

unsigned char CursesLocalIO::LocalGetChar() {
  if (last_key_pressed != ERR) {
    int ch = last_key_pressed;
    last_key_pressed = ERR;
    return ch;
  }
  nodelay(window->window(), FALSE);
  last_key_pressed = ERR;
  return window->GetChar();
}

void CursesLocalIO::MakeLocalWindow(int x, int y, int xlen, int ylen) {}
void CursesLocalIO::SetCursor(int cursorStyle) {}

void CursesLocalIO::LocalClrEol() {
  SetColor(curatr);
  window->ClrtoEol();
}

void CursesLocalIO::LocalWriteScreenBuffer(const char *pszBuffer) {}
int CursesLocalIO::GetDefaultScreenBottom() { return window->GetMaxY() - 1; }
void CursesLocalIO::LocalEditLine(char *s, int len, int status, int *returncode, char *ss) {}
void CursesLocalIO::UpdateNativeTitleBar() {}

void CursesLocalIO::UpdateTopScreen(WStatus* pStatus, WSession *pSession, int nInstanceNumber) {}
