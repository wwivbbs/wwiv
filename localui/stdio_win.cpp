/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
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
/*                                                                        */
/**************************************************************************/

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "curses.h"
#include "core/strings.h"
#include "localui/stdio_win.h"

using std::string;
using wwiv::strings::StringPrintf;

int StdioWindow::GetChar() const {
  return std::cin.get();
}

void StdioWindow::Putch(unsigned char ch) {
  std::cout << ch;
}

void StdioWindow::Puts(const std::string& text) {
  std::cout << text;
}

void StdioWindow::PutsXY(int x, int y, const std::string& text) {
  Puts(text);
}

void StdioWindow::Printf(const char* format, ...) {
  va_list ap;
  char buffer[1024];

  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);
  Puts(buffer);
}

void StdioWindow::PrintfXY(int x, int y, const char* format, ...) {
  va_list ap;
  char buffer[1024];

  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);
  Puts(buffer);
}
