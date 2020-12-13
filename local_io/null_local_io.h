/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#ifndef INCLUDED_LOCAL_IO_NULL_LOCAL_IO_H
#define INCLUDED_LOCAL_IO_NULL_LOCAL_IO_H
#include "local_io/local_io.h"

#include <string>

class NullLocalIO final : public LocalIO {
public:
  NullLocalIO();
  ~NullLocalIO() override;
  void Putch(unsigned char) override {}
  void GotoXY(int, int) override {}
  [[nodiscard]] int WhereX() const noexcept override { return 0; }
  [[nodiscard]] int WhereY() const noexcept override { return 0; }
  void Lf() override {}
  void Cr() override {}
  void Cls() override {}
  void Backspace() override {}
  void PutchRaw(unsigned char) override {}
  void Puts(const std::string&) override {}
  void PutsXY(int, int, const std::string&) override {}
  void PutsXYA(int, int, int, const std::string&) override {}
  void set_protect(int) override {}
  void savescreen() override {}
  void restorescreen() override {}
  [[nodiscard]] bool KeyPressed() override { return false; }
  [[nodiscard]] unsigned char GetChar() override { return static_cast<unsigned char>(getchar()); }
  void MakeLocalWindow(int, int, int, int) override {}
  void SetCursor(int) override {}
  void ClrEol() override {}
  void WriteScreenBuffer(const char*) override {}
  [[nodiscard]] int GetDefaultScreenBottom() const noexcept override { return 24; }
  void EditLine(char*, int, AllowedKeys, EditlineResult*,
                const char*) override {}
  void UpdateNativeTitleBar(const std::string&, int) override {}

private:
  void FastPuts(const std::string&) override {}
};

#endif