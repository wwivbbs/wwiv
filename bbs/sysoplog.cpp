/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include <cstdarg>
#include <cstddef>
#include <string>

#include "bbs/datetime.h"
#include "bbs/wwiv.h"
#include "core/strings.h"

using std::string;
using namespace wwiv::strings;

// Local function prototypes
void AddLineToSysopLogImpl(int cmd, const string& text);

static const int LOG_STRING = 0;
static const int LOG_CHAR = 4;
static const std::size_t CAT_BUFSIZE = 8192;

/*
* Creates sysoplog filename in s, from datestring.
*/
string GetSysopLogFileName(const string& d) {
  return StringPrintf("%c%c%c%c%c%c.log", d[6], d[7], d[0], d[1], d[3], d[4]);
}

/*
* Returns instance (temporary) sysoplog filename in s.
*/
void GetTemporaryInstanceLogFileName(char *pszInstanceLogFileName) {
  sprintf(pszInstanceLogFileName, "inst-%3.3u.log", application()->GetInstanceNumber());
}

/*
* Copies temporary/instance sysoplog to primary sysoplog file.
*/
void catsl() {
  char szInstanceBaseName[MAX_PATH];
  char szInstanceLogFileName[MAX_PATH];

  GetTemporaryInstanceLogFileName(szInstanceBaseName);
  sprintf(szInstanceLogFileName, "%s%s", syscfg.gfilesdir, szInstanceBaseName);

  if (File::Exists(szInstanceLogFileName)) {
    string basename = GetSysopLogFileName(date());
    File wholeLogFile(syscfg.gfilesdir, basename);

    char* pLogBuffer = static_cast<char *>(BbsAllocA(CAT_BUFSIZE));
    if (pLogBuffer) {
      if (wholeLogFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
        wholeLogFile.Seek(0, File::seekBegin);
        wholeLogFile.Seek(0, File::seekEnd);

        File instLogFile(szInstanceLogFileName);
        if (instLogFile.Open(File::modeReadOnly | File::modeBinary)) {
          int nNumRead = 0;
          do {
            nNumRead = instLogFile.Read(pLogBuffer, CAT_BUFSIZE);
            if (nNumRead > 0) {
              wholeLogFile.Write(pLogBuffer, nNumRead);
            }
          } while (nNumRead == CAT_BUFSIZE);

          instLogFile.Close();
          instLogFile.Delete();
        }
        wholeLogFile.Close();
      }
      free(pLogBuffer);
    }
  }
}

/*
* Writes a line to the sysoplog.
*/
void AddLineToSysopLogImpl(int cmd, const string& text) {
  static string::size_type midline = 0;
  static char s_szLogFileName[MAX_PATH];

  if (!(syscfg.gfilesdir)) {
    // TODO Use error log.
    return;
  }

  if (&syscfg.gfilesdir[0] == nullptr) {
    // If we try to write we will throw a NPE.
    return;
  }

  if (!s_szLogFileName[0]) {
    strcpy(s_szLogFileName, syscfg.gfilesdir);
    GetTemporaryInstanceLogFileName(s_szLogFileName + strlen(s_szLogFileName));
  }
  switch (cmd) {
  case LOG_STRING: {  // Write line to sysop's log
    File logFile(s_szLogFileName);
    if (!logFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      return;
    }
    if (logFile.GetLength()) {
      logFile.Seek(0L, File::seekEnd);
    }
    string logLine;
    if (midline > 0) {
      logLine = StrCat("\r\n", text);
      midline = 0;
    } else {
      logLine = text;
    }
    logLine += "\r\n";
    logFile.Write(logLine);
    logFile.Close();
  }
  break;
  case LOG_CHAR: {
    File logFile(s_szLogFileName);
    if (!logFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      // sysop log ?
      return;
    }
    if (logFile.GetLength()) {
      logFile.Seek(0L, File::seekEnd);
    }
    string logLine;
    if (midline == 0 || (midline + 2 + text.length()) > 78) {
      logLine = (midline) ? "\r\n   " : "  ";
      midline = 3 + text.length();
    } else {
      logLine = ", ";
      midline += 2 + text.length();
    }
    logLine += text;
    logFile.Write(logLine);
    logFile.Close();
  }
  break;
  default: {
    AddLineToSysopLogImpl(LOG_STRING, StrCat("Invalid Command passed to sysoplog::AddLineToSysopLogImpl, Cmd = ", std::to_string(cmd)));
  } break;
  }
}

/*
* Writes a string to the sysoplog, if user online and EffectiveSl < 255.
*/
void sysopchar(const string& text) {
  if ((incom || session()->GetEffectiveSl() != 255) && !text.empty()) {
    AddLineToSysopLogImpl(LOG_CHAR, text);
  }
}

/*
* Writes a string to the sysoplog, if EffectiveSl < 255 and user online,
* indented a few spaces.
*/
void sysoplog(const string& text, bool bIndent) {
  if (bIndent) {
    AddLineToSysopLogImpl(LOG_STRING, StrCat("   ", text));
  } else {
    AddLineToSysopLogImpl(LOG_STRING, text);
  }
}

// printf style function to write to the sysop log
void sysoplogf(const char *pszFormat, ...) {
  va_list ap;
  char szBuffer[2048];

  va_start(ap, pszFormat);
  vsnprintf(szBuffer, sizeof(szBuffer), pszFormat, ap);
  va_end(ap);
  sysoplog(szBuffer);
}

// printf style function to write to the sysop log
void sysoplogfi(bool bIndent, const char *pszFormat, ...) {
  va_list ap;
  char szBuffer[2048];

  va_start(ap, pszFormat);
  vsnprintf(szBuffer, sizeof(szBuffer), pszFormat, ap);
  va_end(ap);
  sysoplog(szBuffer, bIndent);
}
