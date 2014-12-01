/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014,WWIV Software Services             */
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
#include <string>
#include <vector>

#include <direct.h>

#include "core/strings.h"
#include "core/file.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef WIN32_LEAN_AND_MEAN

using std::string;
using std::vector;

using wwiv::UpperCase;
using wwiv::strings::StrCat;

#ifndef NOT_BBS

void WWIV_make_abs_cmd(const std::string root, std::string* out) {
  
  static const vector<string> exts{
    "",
    ".com",
    ".exe",
    ".bat",
    ".btm",
    ".cmd",
  };

  // pszOutBuffer must be at least MAX_PATH in size.
  char s[MAX_PATH], s1[MAX_PATH], s2[MAX_PATH];

  strncpy(s1, out->c_str(), MAX_PATH);

  if (s1[1] == ':') {
    if (s1[2] != '\\') {
      _getdcwd(wwiv::UpperCase<char>(s1[0]) - 'A' + 1, s, MAX_PATH);
      if (s[0]) {
        _snprintf(s1, sizeof(s1), "%c:\\%s\\%s", s1[0], s, out->substr(2).c_str());
      } else {
        _snprintf(s1, sizeof(s1), "%c:\\%s", s1[0], out->substr(2).c_str());
      }
    }
  } else if (s1[0] == '\\') {
    _snprintf(s1, sizeof(s1), "%c:%s", root.c_str(), out->c_str());
  } else {
    strncpy(s2, s1, sizeof(s2));
    strtok(s2, " \t");
    if (strchr(s2, '\\')) {
      _snprintf(s1, sizeof(s1), "%s%s", root.c_str(), out->c_str());
    }
  }

  char* ss = strchr(s1, ' ');
  if (ss) {
    *ss = '\0';
    _snprintf(s2, sizeof(s2), " %s", ss + 1);
  } else {
    s2[0] = '\0';
  }
  for (const string& ext : exts) {
    if (ext.size() == 0) {
      const char* ss1 = strrchr(s1, '\\');
      if (!ss1) {
        ss1 = s1;
      }
      if (strchr(ss1, '.') == 0) {
        continue;
      }
    }
    _snprintf(s, sizeof(s), "%s%s", s1, ext.c_str());
    if (s1[1] == ':') {
      if (File::Exists(s)) {
        *out = StrCat(s, s2);
        return;
      }
    } else {
      if (File::Exists(s)) {
        *out = StrCat(root, s, s2);
        return;
      } else {
        char szFoundPath[MAX_PATH];
        _searchenv(s, "PATH", szFoundPath);
        if (strlen(szFoundPath) > 0) {
          *out = wwiv::strings::StringPrintf("%s%s", szFoundPath, s2);
          return;
        }
      }
    }
  }

  *out = StrCat(root, s1, s2);
}
#endif

void WWIV_Delay(unsigned long msec) {
  Sleep(msec);
}
