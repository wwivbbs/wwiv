/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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
#include "bbs/sysoplog.h"

#include <cstdarg>
#include <cstddef>
#include <string>

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/utility.h"

#include "bbs/datetime.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/datetime.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
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
std::string GetTemporaryInstanceLogFileName() {
  return StringPrintf("inst-%3.3u.log", a()->instance_number());
}

/*
* Copies temporary/instance sysoplog to primary sysoplog file.
*/
void catsl() {
  auto temporary_log_filename = GetTemporaryInstanceLogFileName();
  auto instance_logfilename = FilePath(a()->config()->gfilesdir(), temporary_log_filename);

  if (File::Exists(instance_logfilename)) {
    auto basename = GetSysopLogFileName(date());
    File wholeLogFile(FilePath(a()->config()->gfilesdir(), basename));

    auto buffer = std::make_unique<char[]>(CAT_BUFSIZE);
    if (wholeLogFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      wholeLogFile.Seek(0, File::Whence::begin);
      wholeLogFile.Seek(0, File::Whence::end);

      File instLogFile(instance_logfilename);
      if (instLogFile.Open(File::modeReadOnly | File::modeBinary)) {
        int num_read = 0;
        do {
          num_read = instLogFile.Read(buffer.get(), CAT_BUFSIZE);
          if (num_read > 0) {
            wholeLogFile.Write(buffer.get(), num_read);
          }
        } while (num_read == CAT_BUFSIZE);

        instLogFile.Close();
        instLogFile.Delete();
      }
      wholeLogFile.Close();
    }
  }
}

/*
* Writes a line to the sysoplog.
*/
void AddLineToSysopLogImpl(int cmd, const string& text) {
  static string::size_type midline = 0;
  
  if (a()->config()->gfilesdir().empty()) {
    LOG(ERROR) << "gfilesdir empty, can't write to sysop log";
    return;
  }
  const static auto s_sysoplog_filename =
      FilePath(a()->config()->gfilesdir(), GetTemporaryInstanceLogFileName());

  switch (cmd) {
  case LOG_STRING: {  // Write line to sysop's log
    File logFile(s_sysoplog_filename);
    if (!logFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      return;
    }
    if (logFile.length()) {
      logFile.Seek(0L, File::Whence::end);
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
    File logFile(s_sysoplog_filename);
    if (!logFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      // sysop log ?
      return;
    }
    if (logFile.length()) {
      logFile.Seek(0L, File::Whence::end);
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
* Writes a string to the sysoplog.
*/
void sysopchar(const string& text) {
  if (!text.empty()) {
    AddLineToSysopLogImpl(LOG_CHAR, text);
  }
}

sysoplog::~sysoplog() {
  if (indent_) {
    AddLineToSysopLogImpl(LOG_STRING, StrCat("   ", stream_.str()));
  } else {
    AddLineToSysopLogImpl(LOG_STRING, stream_.str());
  }
}
