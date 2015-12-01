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

#include <cstring>
#include <string>

#include "bbs/bbs.h"
#include "bbs/wsession.h"
#include "bbs/local_io.h"
#include "core/wwivport.h"
#include "core/wfndfile.h"
#include "core/strings.h"

using namespace wwiv::strings;

// prototype from utility.cpp
bool okansi();
char *stripfn(const char *pszFileName);

// from xfer.cpp
void align(char *pszFileName);

// Displays list of files matching filespec pszFileName in directory pszDirectoryName.
void show_files(const char *pszFileName, const char *pszDirectoryName) {
  char s[MAX_PATH];
  char drive[MAX_PATH], direc[MAX_PATH], file[MAX_PATH], ext[MAX_PATH];

  char c = (okansi()) ? '\xCD' : '=';
  bout.nl();
#if defined (_WIN32)
  _splitpath(pszDirectoryName, drive, direc, file, ext);
#else
  strcpy(direc, pszDirectoryName);
  strcpy(drive, "");
  strcpy(file, pszFileName);
  strcpy(ext, "");
#endif

  snprintf(s, sizeof(s), "|#7[|B1|15 FileSpec: %s    Dir: %s%s |B0|#7]", strupr(stripfn(pszFileName)), drive, direc);
  int i = (session()->user()->GetScreenChars() - 1) / 2 - strlen(stripcolors(s)) / 2;
  bout << "|#7" << std::string(i, c) << s;
  i = session()->user()->GetScreenChars() - 1 - i - strlen(stripcolors(s));
  bout << "|#7" << std::string(i, c);

  std::string full_pathname = StrCat(pszDirectoryName, strupr(stripfn(pszFileName)));
  WFindFile fnd;
  bool bFound = fnd.open(full_pathname, 0);
  while (bFound) {
    strncpy(s, fnd.GetFileName(), MAX_PATH);
    align(s);
    full_pathname = StrCat("|#7[|#2", s, "|#7]|#1 ");
    if (session()->localIO()->WhereX() > (session()->user()->GetScreenChars() - 15)) {
      bout.nl();
    }
    bout << full_pathname;
    bFound = fnd.next();
  }

  bout.nl();
  bout.Color(7);
  bout << std::string(session()->user()->GetScreenChars() - 1, c);
  bout.nl(2);
}
