/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services            */
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
#ifndef INCLUDED_LOCAL_IO_CURSES_H
#define INCLUDED_LOCAL_IO_CURSES_H

#include "localui/curses_win.h"
#include "local_io/local_io.h"
#include <string>

namespace wwiv::local::io {

class CursesLocalIO final : public LocalIO {
public:
  // Constructor/Destructor
  CursesLocalIO();
  explicit CursesLocalIO(int num_lines, int num_cols);
  explicit CursesLocalIO(const CursesLocalIO& copy) = delete;
  explicit CursesLocalIO(CursesLocalIO&&) = delete;
  CursesLocalIO& operator=(const CursesLocalIO&) = delete;
  CursesLocalIO& operator=(CursesLocalIO&&) = delete;
  ~CursesLocalIO() override;

  void GotoXY(int x, int y) override;
  [[nodiscard]] int WhereX() const noexcept override;
  [[nodiscard]] int WhereY() const noexcept override;
  void Lf() override;
  void Cr() override;
  void Cls() override;
  void ClrEol() override;
  void Backspace() override;
  void PutchRaw(unsigned char ch) override;
  // Overridden by TestLocalIO in tests
  void Putch(unsigned char ch) override;
  void Puts(const std::string&) override;
  void PutsXY(int x, int y, const std::string& text) override;
  void PutsXYA(int x, int y, int a, const std::string& text) override;
  void set_protect(int l) override;
  void savescreen() override;
  void restorescreen() override;
  bool KeyPressed() override;
  [[nodiscard]] unsigned char GetChar() override;
  void MakeLocalWindow(int x, int y, int xlen, int ylen) override;
  void SetCursor(int cursorStyle) override;
  void WriteScreenBuffer(const char* buffer) override;
  [[nodiscard]] int GetDefaultScreenBottom() const noexcept override;

  void EditLine(char* s, int len, AllowedKeys allowed_keys, EditlineResult* returncode,
                const char* allowed_set) override;
  void UpdateNativeTitleBar(const std::string& system_name, int instance_number) override;
  void ResetColors();

  void DisableLocalIO() override;
  void ReenableLocalIO() override;

private:
  void FastPuts(const std::string& text) override;
  void SetColor(int color);
  int x_{0};
  int y_{0};

  std::unique_ptr<wwiv::local::ui::CursesWindow> window_;
};

}

#endif
