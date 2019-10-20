/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2019, WWIV Software Services             */
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

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "core/command_line.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/version.h"

#include "fmt/core.h"
#include "fmt/printf.h"


using std::ofstream;
using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::core {

static constexpr char log_date_format[] = "%F %T";

static std::shared_ptr<Appender> console_appender;
static std::shared_ptr<Appender> logfile_appender;
LoggerConfig Logger::config_;

class ConsoleAppender : public Appender {
  bool append(const std::string& message) override {
    std::cerr << message << std::endl;
    return true;
  }
};

class LogFileAppender : public Appender {
public:
  LogFileAppender(std::string fn)
    : filename_(std::move(fn)) {
  }

  bool append(const std::string& message) override {
    // Not super performant, but we'll start here and see how far this
    // gets us.
    if (message.empty()) {
      return true;
    }
    TextFile out(filename_, "a");
    if (!out.IsOpen()) {
      // We don't want to crash if we can't log, but what
      // should we do instead?
      return false;
    }
    return out.WriteLine(message) > 0;
  }

private:
  const std::string filename_;
};

static std::string FormatLogLevel(LoggerLevel l, int v) noexcept {
  if (l == LoggerLevel::verbose) {
    return StrCat("VER-", v);
  }
  static const std::unordered_map<LoggerLevel, std::string, stl::enum_hash> map = {
      {LoggerLevel::ignored, ""},
      {LoggerLevel::start, "START"},
      {LoggerLevel::debug, "DEBUG"},
      {LoggerLevel::verbose, "VER- "},
      {LoggerLevel::error, "ERROR"},
      {LoggerLevel::info, "INFO "},
      {LoggerLevel::warning, "WARN "},
      {LoggerLevel::fatal, "FATAL"},
  };
  return map.at(l);
}

std::string Logger::FormatLogMessage(LoggerLevel level, int verbosity,
                                     const std::string& msg) const noexcept {
  return StrCat(config_.timestamp_fn_(), FormatLogLevel(level, verbosity), " ", msg);
}

Logger::Logger(LoggerLevel level, int verbosity) noexcept
  : level_(level), verbosity_(verbosity) {
}

Logger::~Logger() noexcept {
  if (level_ == LoggerLevel::verbose) {
    if (!vlog_is_on(verbosity_)) {
      return;
    }
  }
  const auto msg = FormatLogMessage(level_, verbosity_, ss_.str());
  const auto& appenders = config_.log_to[level_];
  if (appenders.empty()) {
    // probably should never happen.
    if (console_appender) {
      console_appender->append(StrCat("No appenders specified; : ", msg));
    }
  }
  for (auto a : appenders) {
    a->append(msg);
  }
  if (level_ == LoggerLevel::fatal) {
    abort();
  }
}

// static
void Logger::set_cmdline_verbosity(int cmdline_verbosity) {
  config_.cmdline_verbosity = cmdline_verbosity;
}

// static
bool Logger::vlog_is_on(int level) { return level <= config_.cmdline_verbosity; }

// static
void Logger::StartupLog(int argc, char* argv[]) {
  const auto dt = DateTime::now();
  LOG(STARTUP) << config_.exit_filename << " version " << wwiv_version << beta_version << " ("
      << wwiv_date << ")";
  LOG(STARTUP) << config_.exit_filename << " starting at " << dt.to_string();
  if (argc > 1) {
    string cmdline;
    for (auto i = 1; i < argc; i++) {
      cmdline += argv[i];
      cmdline += " ";
    }
    LOG(STARTUP) << "command line: " << cmdline;
  }
}

// static
void Logger::ExitLogger() {
  const auto dt = DateTime::now();
  LOG(STARTUP) << config_.exit_filename << " exiting at " << dt.to_string();
}

// static
void Logger::Init(int argc, char** argv, LoggerConfig& c) {
  config_ = c;
  config_.cmdline_verbosity = 0;
  CommandLine cmdline(argc, argv, "");
  cmdline.AddStandardArgs();
  cmdline.set_no_args_allowed(true);
  cmdline.set_unknown_args_allowed(true);
  cmdline.Parse();

  const auto l = cmdline.arg("logdir");
  auto logdir = cmdline.logdir();
  if (l.is_default() && c.logdir_fn_) {
    const auto logdir_from_fn = c.logdir_fn_(cmdline.bbsdir());
    if (!logdir_from_fn.empty()) {
      logdir = logdir_from_fn;
    }
  }

  // Set --v from commandline
  config_.cmdline_verbosity = cmdline.iarg("v");

  string filename(argv[0]);
  if (ends_with(filename, ".exe") || ends_with(filename, ".EXE")) {
    filename = filename.substr(0, filename.size() - 4);
  }
  const auto last_slash = filename.rfind(File::pathSeparatorChar);
  if (last_slash != string::npos) {
    filename = filename.substr(last_slash + 1);
  }
  config_.log_filename = FilePath(logdir, StrCat(filename, ".log"));
  config_.exit_filename = filename;

  // Setup the default appenders.
  console_appender.reset(new ConsoleAppender{});
  logfile_appender.reset(new LogFileAppender{config_.log_filename});

  if (config_.register_console_destinations) {
    config_.add_appender(LoggerLevel::error, console_appender);
    config_.add_appender(LoggerLevel::fatal, console_appender);
    config_.add_appender(LoggerLevel::warning, console_appender);
    config_.add_appender(LoggerLevel::info, console_appender);
    config_.add_appender(LoggerLevel::verbose, console_appender);
  }
  if (config_.register_file_destinations) {
    config_.add_appender(LoggerLevel::error, logfile_appender);
    config_.add_appender(LoggerLevel::fatal, logfile_appender);
    config_.add_appender(LoggerLevel::warning, logfile_appender);
    config_.add_appender(LoggerLevel::info, logfile_appender);
    config_.add_appender(LoggerLevel::verbose, logfile_appender);
    config_.add_appender(LoggerLevel::start, logfile_appender);
  }
  if (config_.log_startup || cmdline.barg("log_startup")) {
    StartupLog(argc, argv);
  }
}

static std::string DefaultTimestamp() {
  const auto dt = DateTime::now();
  const auto nowc = std::chrono::system_clock::now();
  const auto duration = nowc.time_since_epoch();
  const auto millis = static_cast<int>(
    std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000);
  return fmt::sprintf("%s,%03d ", dt.to_string(log_date_format), millis);
}


LoggerConfig::LoggerConfig()
  : timestamp_fn_(DefaultTimestamp) {
}

LoggerConfig::LoggerConfig(logdir_fn l)
  : LoggerConfig(l, DefaultTimestamp) {
}

LoggerConfig::LoggerConfig(logdir_fn l, timestamp_fn t)
  : logdir_fn_(l), timestamp_fn_(t) {
}


void LoggerConfig::add_appender(LoggerLevel level, std::shared_ptr<Appender> appender) {
  log_to[level].emplace(appender);
}

void LoggerConfig::reset() {
  timestamp_fn_ = DefaultTimestamp;
}

} // namespace wwiv
