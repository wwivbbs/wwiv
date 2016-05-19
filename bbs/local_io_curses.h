/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016,WWIV Software Services             */
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
#ifndef __INCLUDED_LOCAL_IO_CURSES_H__
#define __INCLUDED_LOCAL_IO_CURSES_H__

#include <cstdint>
#include <cstdio>
#include <string>

#include "bbs/local_io.h"
#include "initlib/colors.h"
#include "initlib/curses_win.h"

class WStatus;
class WSession;

// Describes a color scheme.
class AnsiColor {
public:
  AnsiColor(uint8_t f, uint8_t b, bool bold) : f_(f), b_(b), bold_(bold) {}
  AnsiColor() : AnsiColor(7, 0, true) {}

  // Don't provide a user defined destructor since that will block move semantics
  uint8_t f() const { return f_; }
  uint8_t b() const { return b_; }
  bool bold() const { return bold_; }

private:
  uint8_t f_;
  uint8_t b_;
  bool bold_;
};

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4100 )
#endif

class CursesLocalIO : public LocalIO {
 public:
  // Constructor/Destructor
  CursesLocalIO();
  explicit CursesLocalIO(int num_lines);
  CursesLocalIO(const LocalIO& copy) = delete;
  virtual ~CursesLocalIO();

  virtual void SetColor(int color);
  void LocalGotoXY(int x, int y) override;
  size_t WhereX() override;
  size_t WhereY() override;
  void LocalLf() override;
  void LocalCr() override;
  void LocalCls() override;
  void LocalClrEol() override;
  void LocalBackspace() override;
  void LocalPutchRaw(unsigned char ch) override;
  // Overridden by TestLocalIO in tests
  void LocalPutch(unsigned char ch) override;
  void LocalPuts(const std::string& text) override;
  void LocalXYPuts(int x, int y, const std::string& text) override;
  int  LocalPrintf(const char *formatted_text, ...) override;
  int  LocalXYPrintf(int x, int y, const char *formatted_text, ...) override;
  int  LocalXYAPrintf(int x, int y, int nAttribute, const char *formatted_text, ...) override;
  void set_protect(WSession* session, int l) override;
  void savescreen() override;
  void restorescreen() override;
  void tleft(WSession* session, bool temp_sysop, bool sysop, bool user_online) override;
  void UpdateTopScreen(WStatus* pStatus, WSession *pSession, int nInstanceNumber) override;
  bool LocalKeyPressed() override;
  unsigned char LocalGetChar() override;
  void SaveCurrentLine(char *cl, char *atr, char *xl, char *cc) override;
  void MakeLocalWindow(int x, int y, int xlen, int ylen) override;
  void SetCursor(int cursorStyle) override;
  void LocalWriteScreenBuffer(const char *buffer) override;
  size_t GetDefaultScreenBottom() override;

  void LocalEditLine(char *s, int len, int status, int *returncode, char *ss) override;
  void UpdateNativeTitleBar(WSession* session) override;

private:
  void LocalFastPuts(const std::string &text) override;
  size_t x_ = 0;
  size_t y_ = 0;

  std::unique_ptr<CursesWindow> window_;
  const std::map<int, AnsiColor> scheme_;
};

#if defined( _MSC_VER )
#pragma warning( pop )
#endif // _MSC_VER

#endif // __INCLUDED_LOCAL_IO_CURSES_H__
