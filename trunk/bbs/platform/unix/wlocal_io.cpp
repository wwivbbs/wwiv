/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#include <iostream>
#include <string>

#include "bbs/asv.h"
#include "bbs/datetime.h"
#include "bbs/wwiv.h"
#include "bbs/wcomm.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "core/file.h"
#include "core/os.h"
#include "core/strings.h"

using std::cout;
using std::string;
using namespace wwiv::strings;

WLocalIO::WLocalIO() {}

WLocalIO::~WLocalIO() {}

void WLocalIO::LocalGotoXY(int x, int y) {}

int WLocalIO::WhereX() { return 0; }

int WLocalIO::WhereY() { return 0; }

void WLocalIO::LocalLf() {}

void WLocalIO::LocalCr() {}

void WLocalIO::LocalCls() {}

void WLocalIO::LocalBackspace() {}

void WLocalIO::LocalPutchRaw(unsigned char ch) {}

void WLocalIO::LocalPutch(unsigned char ch) {}

void WLocalIO::LocalPuts(const string& s) {}

void WLocalIO::LocalXYPuts(int x, int y, const string& text) {}

void WLocalIO::LocalFastPuts(const string& text) {}

int WLocalIO::LocalPrintf(const char *pszFormattedText, ...) { return 0; }

int  WLocalIO::LocalXYPrintf(int x, int y, const char *pszFormattedText, ...) { return 0; }

int  WLocalIO::LocalXYAPrintf(int x, int y, int nAttribute, const char *pszFormattedText, ...) { return 0; }

void WLocalIO::set_protect(int l) {}

void WLocalIO::savescreen() {}

void WLocalIO::restorescreen() {}

void WLocalIO::skey(char ch) {}

void WLocalIO::tleft(bool bCheckForTimeOut) {}

/****************************************************************************/
/**
 * IsLocalKeyPressed - returns whether or not a key been pressed at the local console.
 *
 * @return true if a key has been pressed at the local console, false otherwise
 */
bool WLocalIO::LocalKeyPressed() {
  return false;
}

void WLocalIO::SaveCurrentLine(char *cl, char *atr, char *xl, char *cc) {
  *cl = 0;
  *atr = 0;
  *cc = static_cast<char>(curatr);
  strcpy(xl, endofline);
}

/**
 * LocalGetChar - gets character entered at local console.
 *                <B>Note: This is a blocking function call.</B>
 *
 * @return int value of key entered
 */
unsigned char WLocalIO::LocalGetChar() { return getchar(); }

void WLocalIO::MakeLocalWindow(int x, int y, int xlen, int ylen) {
  x = x;
  y = y;
  xlen = xlen;
  ylen = ylen;
}

void WLocalIO::SetCursor(int cursorStyle) {}

void WLocalIO::LocalClrEol() {}

void WLocalIO::LocalWriteScreenBuffer(const char *pszBuffer) {
  pszBuffer = pszBuffer; // No warning
}

int WLocalIO::GetDefaultScreenBottom() { return 25; }

/**
 * Edits a string, doing local screen I/O only.
 */
void WLocalIO::LocalEditLine(char *s, int len, int status, int *returncode, char *ss) {}

int WLocalIO::GetEditLineStringLength(const char *pszText) {
  int i = strlen(pszText);
  while (i >= 0 && (/*pszText[i-1] == 32 ||*/ static_cast<unsigned char>(pszText[i - 1]) == 176)) {
    --i;
  }
  return i;
}

void WLocalIO::UpdateNativeTitleBar() {}

void WLocalIO::UpdateTopScreen(WStatus* pStatus, WSession *pSession, int nInstanceNumber) {}
