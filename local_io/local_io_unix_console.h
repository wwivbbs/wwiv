/**************************************************************************/
/*                                                                        */
/*                                WWIV Version 5                          */
/*             Copyright (C)1998-2019, WWIV Software Services            */
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
#ifndef __INCLUDED_LOCAL_IO_UNIX_CONSOLE_H__
#define __INCLUDED_LOCAL_IO_UNIX_CONSOLE_H__

#include <cstdio>
#include <string>
#include <termios.h>
#include "local_io/local_io.h"

class UnixConsoleIO : public LocalIO {
 public:
  // Constructor/Destructor
  UnixConsoleIO();
  UnixConsoleIO(const LocalIO& copy) = delete	;
  virtual ~UnixConsoleIO();

  virtual void LocalGotoXY(int x, int y) override;
  virtual int WhereX() const noexcept override;
  virtual int WhereY() const noexcept override;
  virtual void LocalLf() override;
  virtual void LocalCr() override;
  virtual void LocalCls() override;
  virtual void LocalClrEol() override;
  virtual void LocalBackspace() override;
  virtual void LocalPutchRaw(unsigned char ch) override;
  // Overridden by TestLocalIO in tests
  virtual void LocalPutch(unsigned char ch) override;
  virtual void LocalPuts(const std::string& text) override;
  virtual void LocalXYPuts(int x, int y, const std::string& text) override;
  virtual void set_protect(int l) override;
  virtual void savescreen() override;
  virtual void restorescreen() override;
  virtual void tleft(bool bCheckForTimeOut) override;
  virtual bool LocalKeyPressed() override;
  virtual unsigned char LocalGetChar() override;
  virtual void SaveCurrentLine(char *cl, char *atr, char *xl, char *cc) override;
  virtual void MakeLocalWindow(int x, int y, int xlen, int ylen) override;
  virtual void SetCursor(int cursorStyle) override;
  virtual void LocalWriteScreenBuffer(const char *pszBuffer) override;
  virtual int  GetDefaultScreenBottom() override;

  virtual void UpdateNativeTitleBar(const std::string& system_name, int instance_number) override;

private:
  virtual void LocalFastPuts(const std::string &text) override;
  int m_cursorPositionX = 0;
  int m_cursorPositionY = 0;
  FILE *ttyf = nullptr;
  struct termios ttysav;


  void set_attr_xy(int x, int y, int a);
};


#endif // __INCLUDED_LOCAL_IO_UNIX_CONSOLE_H__
