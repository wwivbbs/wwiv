/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2017, WWIV Software Services             */
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
/**************************************************************************/

#include "core/log.h"

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include "core/command_line.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/version.h"

using std::ofstream;
using std::string;
using namespace wwiv::strings;

namespace wwiv {
namespace core {

std::string Logger::log_filename_;
std::string Logger::exit_filename_;
int Logger::cmdline_verbosity_;
bool Logger::log_to_console_;
bool Logger::log_to_file_;
std::unordered_map<LoggerLevel, std::unordered_set<std::shared_ptr<Appender>>, enum_hash>
    Logger::log_to_;


static constexpr char log_date_format[] = "%F %T";

static std::shared_ptr<Appender> console_appender;
static std::shared_ptr<Appender> logfile_appender;

class ConsoleAppender : public Appender {
  virtual bool append(const std::string& message) const {
    std::cerr << message << std::endl;
    return true;
  }
};

class LogFileAppender : public Appender {
public:
  LogFileAppender(const std::string& fn) : filename_(fn) {}
  virtual bool append(const std::string& message) const {
    // Not super performant, but we'll start here and see how far this
    // gets us.
    if (message.empty()) {
      return true;
    }
    TextFile out(filename_, "a");
    return out.WriteLine(message) != 0;
  }

private:
  const std::string filename_;
};

const std::string FormatLogLevel(LoggerLevel l, int v) {
  if (l == LoggerLevel::verbose) {
    return StrCat("VER-", v);
  }
  static const std::unordered_map<LoggerLevel, std::string, wwiv::stl::enum_hash> map = {
      {LoggerLevel::start, "START"},
      {LoggerLevel::debug, "DEBUG"},
      {LoggerLevel::error, "ERROR"},
      {LoggerLevel::info, "INFO "},
      {LoggerLevel::warning, "WARN "}};
  return map.at(l);
}

static std::string FormatLogMessage(LoggerLevel level, int verbosity, const std::string& msg) {
  auto now = std::time(nullptr);
  auto tm = *std::localtime(&now);
  // TODO(rushfan): Fix getting millis.
  return StrCat(wwiv::strings::put_time(&tm, log_date_format), ",000 ",
                FormatLogLevel(level, verbosity), " ", msg);
}

static void LogToStdError(const std::string& msg) { std::cerr << msg << std::endl; }
static void LogToFile(const std::string& filename, const std::string& msg) {
  // Not super performant, but we'll start here and see how far this
  // gets us.
  TextFile out(filename, "a");
  out.WriteLine(msg);
}

Logger::Logger(LoggerLevel level, int verbosity) : level_(level), verbosity_(verbosity) {}

Logger::~Logger() {
  if (level_ == LoggerLevel::verbose) {
    if (!vlog_is_on(verbosity_)) {
      return;
    }
  }
  const auto msg = FormatLogMessage(level_, verbosity_, ss_.str());
  const auto& appenders = log_to_[level_];
  for (auto appender : appenders) {
    appender->append(msg);
  }
  if (level_ == LoggerLevel::fatal) {
    abort();
  }
}

// static
bool Logger::vlog_is_on(int level) { return level <= cmdline_verbosity_; }

// static
void Logger::StartupLog(int argc, char* argv[]) {
  time_t t = time(nullptr);
  string l(asctime(localtime(&t)));
  StringTrim(&l);
  LOG(STARTUP) << exit_filename_ << " version " << wwiv_version << beta_version << " (" << wwiv_date
               << ")";
  LOG(STARTUP) << exit_filename_ << " starting at " << l;
  if (argc > 1) {
    string cmdline;
    for (int i = 1; i < argc; i++) {
      cmdline += argv[i];
      cmdline += " ";
    }
    LOG(STARTUP) << "command line: " << cmdline;
  }
}

// static
void Logger::ExitLogger() {
  time_t t = time(nullptr);
  LOG(STARTUP) << exit_filename_ << " exiting at " << asctime(localtime(&t));
}

// static
void Logger::Init(int argc, char** argv, bool startup_log) {
  cmdline_verbosity_ = 0;
  CommandLine cmdline(argc, argv, "xxxx");
  cmdline.AddStandardArgs();
  cmdline.set_no_args_allowed(true);
  cmdline.Parse();

  // Set --v from commandline
  cmdline_verbosity_ = cmdline.iarg("v");

  string filename(argv[0]);
  if (ends_with(filename, ".exe") || ends_with(filename, ".EXE")) {
    filename = filename.substr(0, filename.size() - 4);
  }
  std::size_t last_slash = filename.rfind(File::pathSeparatorChar);
  if (last_slash != string::npos) {
    filename = filename.substr(last_slash + 1);
  }
  log_filename_ = StrCat(filename, ".log");
  exit_filename_ = filename;

  // Setup the default appenders.
  console_appender.reset(new ConsoleAppender{});
  logfile_appender.reset(new LogFileAppender{log_filename_});

  log_to_[LoggerLevel::error].emplace(console_appender);
  log_to_[LoggerLevel::error].emplace(logfile_appender);

  log_to_[LoggerLevel::fatal].emplace(console_appender);
  log_to_[LoggerLevel::fatal].emplace(logfile_appender);

  log_to_[LoggerLevel::info].emplace(console_appender);
  log_to_[LoggerLevel::info].emplace(logfile_appender);

  log_to_[LoggerLevel::verbose].emplace(console_appender);
  log_to_[LoggerLevel::verbose].emplace(logfile_appender);

  log_to_[LoggerLevel::warning].emplace(console_appender);
  log_to_[LoggerLevel::warning].emplace(logfile_appender);

  log_to_[LoggerLevel::start].emplace(logfile_appender);

  if (startup_log) {
    StartupLog(argc, argv);
  }
}

void Logger::DisableFileLoging() {
  // TODO(rushfan): Implement me
}

} // namespace core
} // namespace wwiv
