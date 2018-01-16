/**************************************************************************/
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
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/**************************************************************************/
#ifndef __INCLUDED_BBS_STDIO_LOCAL_IO_H__
#define __INCLUDED_BBS_STDIO_LOCAL_IO_H__
#include "bbs/local_io.h"

#include <string>

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4125 4100 )
#endif

class StdioLocalIO : public LocalIO {
public:
  StdioLocalIO();
  virtual ~StdioLocalIO();
  void Putch(unsigned char ch) override;
  void GotoXY(int x, int y) override {}
  size_t WhereX() override { return 0; }
  size_t WhereY() override { return 0; }

  void Lf() override;
  void Cr() override;
  void Cls() override;
  void Backspace() override;
  void PutchRaw(unsigned char ch) override;
  void Puts(const std::string& s) override;
  void PutsXY(int x, int y, const std::string& text) override;
  void FastPuts(const std::string& text) override;
  int Printf(const char *formatted_text, ...) override;
  int PrintfXY(int x, int y, const char *formatted_text, ...) override;
  int PrintfXYA(int x, int y, int nAttribute, const char *formatted_text, ...) override;

  void set_protect(Application* session, int l) override {}
  void savescreen() override {}
  void restorescreen() override {}
  bool KeyPressed() override { return false; }
  unsigned char GetChar() override { return static_cast<unsigned char>(getchar()); }
  void MakeLocalWindow(int x, int y, int xlen, int ylen) override {}
  void SetCursor(int cursorStyle) override {}
  void ClrEol() override {}
  void WriteScreenBuffer(const char *buffer) override {}
  size_t GetDefaultScreenBottom() override { return 24; }
  void EditLine(char *s, int len, AllowedKeys allowed_keys, int *returncode, const char *ss) override {}
  void UpdateNativeTitleBar(Application* session) override {}
};

#if defined( _MSC_VER )
#pragma warning( pop )
#endif // _MSC_VER

#endif  // __INCLUDED_BBS_STDIO_LOCAL_IO_H__
