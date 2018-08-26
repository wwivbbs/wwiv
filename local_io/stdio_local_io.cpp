/**************************************************************************/
/*                                                                        */
/*                                WWIV Version 5                          */
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
#include "local_io/stdio_local_io.h"

#include <cstdarg>
#include <cstdio>

StdioLocalIO::StdioLocalIO() {}
StdioLocalIO::~StdioLocalIO() {}

void StdioLocalIO::Putch(unsigned char ch) {
  putchar(ch);
};

void StdioLocalIO::Lf() {
  putchar(10);
}

void StdioLocalIO::Cr() {
  putchar(13);
}

void StdioLocalIO::Cls() {
  // NOP
}

void StdioLocalIO::Backspace() {
  putchar(8);
}

void StdioLocalIO::PutchRaw(unsigned char ch) {
  putchar(ch);
}

void StdioLocalIO::Puts(const std::string& s) {
  puts(s.c_str());
}

void StdioLocalIO::PutsXY(int, int, const std::string& text) {
  Puts(text);
}

void StdioLocalIO::PutsXYA(int, int, int, const std::string& text) {
  Puts(text);
}

void StdioLocalIO::FastPuts(const std::string& text) {
  Puts(text);
}
