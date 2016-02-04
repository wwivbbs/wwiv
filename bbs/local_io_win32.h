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
#ifndef __INCLUDED_LOCAL_IO_WIN32_H__
#define __INCLUDED_LOCAL_IO_WIN32_H__
// Always declare wwiv_windows.h first to avoid collisions on defines.
#include "bbs/wwiv_windows.h"

#include <string>

#include "bbs/keycodes.h"
#include "bbs/local_io.h"
#include "core/file.h"

// This C++ class should encompass all Local Input/Output from The BBS.
// You should use a routine in here instead of using printf, puts, etc.

class WStatus;
class WSession;

class Win32ConsoleIO : public LocalIO {
 public:
  // Constructor/Destructor
  Win32ConsoleIO();
  Win32ConsoleIO(const LocalIO& copy) = delete;
  virtual ~Win32ConsoleIO();

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

  void LocalEditLine(char *s, int len, int editor_status, int *returncode, char *ss) override;
  void UpdateNativeTitleBar(WSession* session) override;

private:
  void LocalFastPuts(const std::string &text) override;

private:
  std::string m_chatReason;
  bool ExtendedKeyWaiting;

  COORD cursor_pos_;
  HANDLE out_;
  HANDLE in_;
  CONSOLE_SCREEN_BUFFER_INFO buffer_info_;
  DWORD saved_input_mode_ = 0;

  void set_attr_xy(int x, int y, int a);
  COORD original_size_;
};


#endif // __INCLUDED_LOCAL_IO_WIN32_H__
