/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services            */
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
#ifndef __INCLUDED_LOCAL_IO_LOCAL_IO_WIN32_H__
#define __INCLUDED_LOCAL_IO_LOCAL_IO_WIN32_H__

#include <string>

#include "local_io/keycodes.h"
#include "local_io/local_io.h"
#include "core/file.h"

// This C++ class should encompass all Local Input/Output from The BBS.
// You should use a routine in here instead of using printf, puts, etc.

struct coord_t {
  int16_t X;
  int16_t Y;
};

typedef void* HANDLE;

class Win32ConsoleIO : public LocalIO {
 public:
  // Constructor/Destructor
  Win32ConsoleIO();
  Win32ConsoleIO(const LocalIO& copy) = delete;
  virtual ~Win32ConsoleIO();

  void GotoXY(int x, int y) override;
  int WhereX() const noexcept override;
  int WhereY() const noexcept override;
  void Lf() override;
  void Cr() override;
  void Cls() override;
  void ClrEol() override;
  void Backspace() override;
  void PutchRaw(unsigned char ch) override;
  // Overridden by TestLocalIO in tests
  void Putch(unsigned char ch) override;
  void Puts(const std::string& text) override;
  void PutsXY(int x, int y, const std::string& text) override;
  void PutsXYA(int x, int y, int a, const std::string& text) override;
  int  PrintfXY(int x, int y, const char *formatted_text, ...) override;
  int  PrintfXYA(int x, int y, int nAttribute, const char *formatted_text, ...) override;
  void set_protect(int l) override;
  void savescreen() override;
  void restorescreen() override;
  bool KeyPressed() override;
  unsigned char GetChar() override;
  void MakeLocalWindow(int x, int y, int xlen, int ylen) override;
  void SetCursor(int cursorStyle) override;
  void WriteScreenBuffer(const char *buffer) override;
  int GetDefaultScreenBottom() const noexcept override;

  void EditLine(char *s, int len, AllowedKeys allowed_keys, int *returncode, const char *ss) override;
  void UpdateNativeTitleBar(const std::string& system_name, int instance_number) override;

private:
  void FastPuts(const std::string &text) override;

private:
  void set_attr_xy(int x, int y, int a);

  bool extended_key_waiting_{false};
  // We set this when calling wherex, but logically WhereX should
  // be const correct, so we'll handle this nasty stuff here.
  mutable coord_t cursor_pos_{};
  HANDLE out_ = (void*) -1;
  HANDLE in_ = (void*) -1;
  DWORD saved_input_mode_{0};
  coord_t original_size_{};
};


#endif // __INCLUDED_LOCAL_IO_LOCAL_IO_WIN32_H__
