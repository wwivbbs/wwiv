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
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include "bbs/local_io_unix_console.h"

#include <algorithm>
#include <iostream>
#include <string>

#include "bbs/asv.h"
#include "bbs/datetime.h"
#include "bbs/wwiv.h"
#include "bbs/wcomm.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "core/file.h"
#include "core/os.h"
#include "core/strings.h"

using std::cout;
using std::string;
using namespace wwiv::strings;

UnixConsoleIO::UnixConsoleIO() {}
UnixConsoleIO::~UnixConsoleIO() {}

void UnixConsoleIO::LocalGotoXY(int x, int y) {
  x = std::max<int>(x, 0);
  x = std::min<int>(x, 79);
  y = std::max<int>(y, 0);
  y += GetTopLine();
  y = std::min<int>(y, GetScreenBottom());

  if (x_only) {
    capture_->set_wx(x);
    return;
  }
  m_cursorPositionX = static_cast< short >(x);
  m_cursorPositionY = static_cast< short >(y);

  cout << "\x1b[" << y << ";" << x << "H";
}

int UnixConsoleIO::WhereX() {
  if (x_only) {
    return capture_->wx();
  }
  return m_cursorPositionX;
}

int UnixConsoleIO::WhereY() {
  return m_cursorPositionY;
}

void UnixConsoleIO::LocalLf() {
  std::cout << "\n";
  m_cursorPositionY++;
  if (m_cursorPositionY > 24) {
    m_cursorPositionY = 24;
  }
}

void UnixConsoleIO::LocalCr() {
  cout << "\r";
  m_cursorPositionX = 0;
}

void UnixConsoleIO::LocalCls() {
  cout << "\x1b[2J";
  m_cursorPositionX = 0;
  m_cursorPositionY = 0;
}

void UnixConsoleIO::LocalBackspace() {
  cout << "\b";
  if (m_cursorPositionX >= 0) {
    m_cursorPositionX--;
  } else if (m_cursorPositionY != GetTopLine()) {
    m_cursorPositionX = 79;
    m_cursorPositionY--;
  }
}

void UnixConsoleIO::LocalPutchRaw(unsigned char ch) {
  cout << ch;
  if (m_cursorPositionX <= 79) {
    m_cursorPositionX++;
    return;
  }
  m_cursorPositionX = 0;
  if (m_cursorPositionY != GetScreenBottom()) {
    m_cursorPositionY++;
  }
}

void UnixConsoleIO::LocalPutch(unsigned char ch) {
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

void UnixConsoleIO::LocalPuts(const string& s) {
  for (char ch : s) {
    LocalPutch(ch);
  }
}

void UnixConsoleIO::LocalXYPuts(int x, int y, const string& text) {
  LocalGotoXY(x, y);
  LocalFastPuts(text);
}

void UnixConsoleIO::LocalFastPuts(const string& text) {
  m_cursorPositionX += text.length();
  m_cursorPositionX %= 80;

  // TODO: set current attributes
  cout << text;
}

int  UnixConsoleIO::LocalPrintf(const char *pszFormattedText, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, pszFormattedText);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), pszFormattedText, ap);
  va_end(ap);
  LocalFastPuts(szBuffer);
  return nNumWritten;
}

int UnixConsoleIO::LocalXYPrintf(int x, int y, const char *pszFormattedText, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, pszFormattedText);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), pszFormattedText, ap);
  va_end(ap);
  LocalXYPuts(x, y, szBuffer);
  return nNumWritten;
}

int  UnixConsoleIO::LocalXYAPrintf(int x, int y, int nAttribute, const char *pszFormattedText, ...) {
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

void UnixConsoleIO::set_protect(int l) {
  SetTopLine(l);
  session()->screenlinest = (session()->using_modem) ? session()->user()->GetScreenLines() :
                               defscreenbottom + 1 - GetTopLine();
}

void UnixConsoleIO::savescreen() {}
void UnixConsoleIO::restorescreen() {}

void UnixConsoleIO::skey(char ch) {
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
          if (session()->topdata > UnixConsoleIO::topdataUser) {
            session()->topdata = UnixConsoleIO::topdataNone;
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

void UnixConsoleIO::tleft(bool bCheckForTimeOut) {}
bool UnixConsoleIO::LocalKeyPressed() { return false; }
void UnixConsoleIO::SaveCurrentLine(char *cl, char *atr, char *xl, char *cc) {}

unsigned char UnixConsoleIO::LocalGetChar() {
  return getchar();
}

void UnixConsoleIO::MakeLocalWindow(int x, int y, int xlen, int ylen) {}
void UnixConsoleIO::SetCursor(int cursorStyle) {}
void UnixConsoleIO::LocalClrEol() {}
void UnixConsoleIO::LocalWriteScreenBuffer(const char *pszBuffer) {}
int UnixConsoleIO::GetDefaultScreenBottom() { return 25; }
void UnixConsoleIO::LocalEditLine(char *s, int len, int status, int *returncode, char *ss) {}
void UnixConsoleIO::UpdateNativeTitleBar() {}

void UnixConsoleIO::UpdateTopScreen(WStatus* pStatus, WSession *pSession, int nInstanceNumber) {}
