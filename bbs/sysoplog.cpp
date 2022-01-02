/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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

#include "bbs/bbs.h"
#include "core/datetime.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "common/context.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

// Local function prototypes
enum class log_cmd_t{ log_string, log_char };
void AddLineToSysopLogImpl(log_cmd_t cmd, const std::string& text);


/*
* Creates sysop log filename in s, from date string.
*/
std::string sysoplog_filename(const std::string& d) {
  return fmt::sprintf("%c%c%c%c%c%c.log", d[6], d[7], d[0], d[1], d[3], d[4]);
}

/*
* Returns instance (temporary) sysop log filename in s.
*/
std::string GetTemporaryInstanceLogFileName() {
  return fmt::sprintf("inst-%3.3u.log", a()->sess().instance_number());
}

/*
* Copies temporary/instance sysop log to primary sysop log file.
*/
void catsl() {
  const auto templog_fn = FilePath(a()->config()->gfilesdir(), GetTemporaryInstanceLogFileName());
  if (!File::Exists(templog_fn)) {
    return;
  }

  std::string instance_text;
  {
    TextFile tmplog(templog_fn, "rt");
    if (!tmplog) {
      return;
    }
    instance_text = tmplog.ReadFileIntoString();
  }

  const auto basename = sysoplog_filename(date());
  TextFile sysoplog_fn(FilePath(a()->config()->gfilesdir(), basename), "at");
  if (sysoplog_fn) {
    sysoplog_fn.WriteLine(instance_text);
    File::Remove(templog_fn);
  }
}

/*
* Writes a line to the sysop log.
*/
void AddLineToSysopLogImpl(log_cmd_t cmd, const std::string& text) {
  static std::string::size_type midline = 0;
  
  if (a()->config()->gfilesdir().empty()) {
    LOG(ERROR) << "gfilesdir empty, can't write to sysop log: " << text;
    return;
  }
  const static auto s_sysoplog_filename =
      FilePath(a()->config()->gfilesdir(), GetTemporaryInstanceLogFileName());

  switch (cmd) {
  case log_cmd_t::log_string: {  // Write line to sysop log
    File logFile(s_sysoplog_filename);
    if (!logFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      return;
    }
    if (logFile.length()) {
      logFile.Seek(0L, File::Whence::end);
    }
    std::string logLine;
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
  case log_cmd_t::log_char: {
    File logFile(s_sysoplog_filename);
    if (!logFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      // sysop log ?
      return;
    }
    if (logFile.length()) {
      logFile.Seek(0L, File::Whence::end);
    }
    std::string logLine;
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
  }
}

/*
* Writes a string to the sysop log.
*/
void sysopchar(const std::string& text) {
  if (!text.empty()) {
    AddLineToSysopLogImpl(log_cmd_t::log_char, text);
  }
}

sysoplog::~sysoplog() {
  try {
    if (indent_) {
      AddLineToSysopLogImpl(log_cmd_t::log_string, StrCat("   ", stream_.str()));
    } else {
      AddLineToSysopLogImpl(log_cmd_t::log_string, stream_.str());
    }
  } catch (...) {
    // NOP
  }
}
