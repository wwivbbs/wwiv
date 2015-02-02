/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015,WWIV Software Services             */
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
#ifndef __INCLUDED_PLATFORM_WLOCALIO_H__
#define __INCLUDED_PLATFORM_WLOCALIO_H__

#include <string>
#include "bbs/keycodes.h"
#include "core/file.h"

#ifdef _WIN32
#define NOGDICAPMASKS
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOATOM
#define NOCLIPBOARD
#define NODRAWTEXT
#define NOKERNEL
#define NONLS
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NOCRYPT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#endif // _WIN32
// This C++ class should encompass all Local Input/Output from The BBS.
// You should use a routine in here instead of using printf, puts, etc.

#define GLOBAL_SIZE 4096

class WStatus;
class WSession;

struct screentype {
  short x1, y1, topline1, curatr1;

#ifdef _WIN32
  CHAR_INFO* scrn1;
#else
  char *scrn1;
#endif
};

class WLocalIO {
 public:
  // Constructor/Destructor
  WLocalIO();
  WLocalIO(const WLocalIO& copy) = delete;
  virtual ~WLocalIO();

  static const int cursorNone = 0;
  static const int cursorNormal = 1;
  static const int cursorSolid = 2;

  static const int topdataNone = 0;
  static const int topdataSystem = 1;
  static const int topdataUser = 2;

  void SetChatReason(const std::string& chat_reason) { m_chatReason = chat_reason; }
  void ClearChatReason() { m_chatReason.clear(); }

  const int GetTopLine() const { return m_nTopLine; }
  void SetTopLine(int nTopLine) { m_nTopLine = nTopLine; }

  const int GetScreenBottom() const { return m_nScreenBottom; }
  void SetScreenBottom(int nScreenBottom) { m_nScreenBottom = nScreenBottom; }

  void SetSysopAlert(bool b) { m_bSysopAlert = b; }
  const bool GetSysopAlert() const { return m_bSysopAlert; }

  void set_global_handle(bool bOpenFile, bool bOnlyUpdateVariable = false);
  void global_char(char ch);
  void set_x_only(bool tf, const char *pszFileName, bool ovwr);

  void LocalGotoXY(int x, int y);
  int  WhereX();
  int  WhereY();
  void LocalLf();
  void LocalCr();
  void LocalCls();
  void LocalClrEol();
  void LocalBackspace();
  void LocalPutchRaw(unsigned char ch);
  // Overridden by TestLocalIO in tests.
  virtual void LocalPutch(unsigned char ch);
  void LocalPuts(const std::string& text);
  void LocalXYPuts(int x, int y, const std::string& text);
  int  LocalPrintf(const char *pszFormattedText, ...);
  int  LocalXYPrintf(int x, int y, const char *pszFormattedText, ...);
  int  LocalXYAPrintf(int x, int y, int nAttribute, const char *pszFormattedText, ...);
  void set_protect(int l);
  void savescreen();
  void restorescreen();
  void skey(char ch);
  void tleft(bool bCheckForTimeOut);
  void UpdateTopScreen(WStatus* pStatus, WSession *pSession, int nInstanceNumber);
  bool LocalKeyPressed();
  unsigned char LocalGetChar();
  void SaveCurrentLine(char *cl, char *atr, char *xl, char *cc);
  /*
   * MakeLocalWindow makes a "shadowized" window with the upper-left hand corner at
   * (x,y), and the lower-right corner at (x+xlen,y+ylen).
   */
  void MakeLocalWindow(int x, int y, int xlen, int ylen);
  void SetCursor(int cursorStyle);
  void LocalWriteScreenBuffer(const char *pszBuffer);
  int  GetDefaultScreenBottom();

  void LocalEditLine(char *s, int len, int status, int *returncode, char *ss);
  void UpdateNativeTitleBar();

private:
  void LocalFastPuts(const std::string &text);

private:
  std::string  m_chatReason;
  File   fileGlobalCap; // g_hGlobalCapHandle;
  bool    m_bSysopAlert;
  int     m_nTopLine;
  int     m_nScreenBottom;
  screentype m_ScreenSave;
  std::string global_buf;
  int wx;

  int GetEditLineStringLength(const char *pszText);
  int ExtendedKeyWaiting;

#if defined ( _WIN32 )
  COORD  m_cursorPosition;
  HANDLE m_hConOut;
  HANDLE m_hConIn;
  CONSOLE_SCREEN_BUFFER_INFO m_consoleBufferInfo;
  DWORD saved_input_mode_ = 0;
#endif

#if defined ( __APPLE__ )
  short m_cursorPositionX;
  short m_cursorPositionY;
#endif

#if defined ( _WIN32 )
  void set_attr_xy(int x, int y, int a);
  COORD m_originalConsoleSize;
#endif // _WIN32
};



#endif // __INCLUDED_PLATFORM_WLOCALIO_H__
