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
#include "local_io/local_io.h"

#include <string>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4125 4100)
#endif

class StdioLocalIO : public LocalIO {
public:
  StdioLocalIO();
  virtual ~StdioLocalIO();
  void Putch(unsigned char ch) override;
  void GotoXY(int x, int y) override {}
  int WhereX() const noexcept override { return 0; }
  int WhereY() const noexcept override { return 0; }

  void Lf() override;
  void Cr() override;
  void Cls() override;
  void Backspace() override;
  void PutchRaw(unsigned char ch) override;
  void Puts(const std::string& s) override;
  void PutsXY(int x, int y, const std::string& text) override;
  void PutsXYA(int x, int y, int a, const std::string& text) override;

  void set_protect(int l) override {}
  void savescreen() override {}
  void restorescreen() override {}
  bool KeyPressed() override { return false; }
  unsigned char GetChar() override { return static_cast<unsigned char>(getchar()); }
  void MakeLocalWindow(int x, int y, int xlen, int ylen) override {}
  void SetCursor(int cursorStyle) override {}
  void ClrEol() override {}
  void WriteScreenBuffer(const char* buffer) override {}
  int GetDefaultScreenBottom() const noexcept override { return 24; }
  void EditLine(char* s, int len, AllowedKeys allowed_keys, int* returncode,
                const char* ss) override {}
  void UpdateNativeTitleBar(const std::string& system_name, int instance_number) override {}

private:
  void FastPuts(const std::string& text) override;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif // _MSC_VER

#endif // __INCLUDED_BBS_STDIO_LOCAL_IO_H__
