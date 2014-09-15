/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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

#include <sys/stat.h>
#include <unistd.h>
#include <string>

#include "bbs/wwiv.h"

#include "core/strings.h"
#include "core/wfile.h"
#include "core/wwivport.h"

using std::string;
using wwiv::strings::StrCat;

#if !defined(NOT_BBS)

void WWIV_make_abs_cmd(const string root, string* out) {
  if (out->find("/") != string::npos) {
    string s(*out);
    *out = StrCat(GetApplication()->GetHomeDir(), s);
  }
}
#endif  // NOT_BBS


#define LAST(s) s[strlen(s)-1]

int WWIV_make_path(const char *s) {
  char current_path[MAX_PATH];
  getcwd(current_path, MAX_PATH);

  char* flp = strdup(s);
  char* p = flp;
  if (LAST(p) == WFile::pathSeparatorChar) {
    LAST(p) = 0;
  }
  if (*p == WFile::pathSeparatorChar) {
    chdir(WFile::pathSeparatorString);
    p++;
  }
  for (; (p = strtok(p, WFile::pathSeparatorString)) != 0; p = 0) {
    if (chdir(p)) {
      if (mkdir(p)) {
        chdir(current_path);
        return -1;
      }
      chdir(p);
    }
  }
  chdir(current_path);
  if (flp) {
    free(flp);
  }
  return 0;
}

#if defined (LAST)
#undef LAST
#endif

void WWIV_Delay(unsigned long msec) {
  if (msec) {
    unsigned long usec = msec * 1000;
    usleep(usec);
  }
}

void WWIV_OutputDebugString(const char *pszString) { }
