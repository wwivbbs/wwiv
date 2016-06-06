/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
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
#include "bbs/stdio_local_io.h"

#include <cstdarg>
#include <cstdio>

StdioLocalIO::StdioLocalIO() {}
StdioLocalIO::~StdioLocalIO() {}

void StdioLocalIO::LocalPutch(unsigned char ch) {
  putchar(ch);
};

void StdioLocalIO::LocalLf() {
  putchar(10);
}

void StdioLocalIO::LocalCr() {
  putchar(13);
}

void StdioLocalIO::LocalCls() {
  // NOP
}

void StdioLocalIO::LocalBackspace() {
  putchar(8);
}

void StdioLocalIO::LocalPutchRaw(unsigned char ch) {
  putchar(ch);
}

void StdioLocalIO::LocalPuts(const std::string& s) {
  puts(s.c_str());
}

void StdioLocalIO::LocalXYPuts(int, int, const std::string& text) {
  LocalPuts(text);
}

void StdioLocalIO::LocalFastPuts(const std::string& text) {
  LocalPuts(text);
}

int StdioLocalIO::LocalPrintf(const char *formatted_text, ...) { 
  va_list ap;
  char szBuffer[1024];

  va_start(ap, formatted_text);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), formatted_text, ap);
  va_end(ap);
  LocalFastPuts(szBuffer);
  return nNumWritten;
}

int StdioLocalIO::LocalXYPrintf(int, int, const char *formatted_text, ...) { 
  va_list ap;
  char szBuffer[1024];

  va_start(ap, formatted_text);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), formatted_text, ap);
  va_end(ap);
  LocalFastPuts(szBuffer);
  return nNumWritten;
}

int StdioLocalIO::LocalXYAPrintf(int, int, int, const char *formatted_text, ...) { 
  va_list ap;
  char szBuffer[1024];

  va_start(ap, formatted_text);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), formatted_text, ap);
  va_end(ap);
  LocalFastPuts(szBuffer);
  return nNumWritten;
}
