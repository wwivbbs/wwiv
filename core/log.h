/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2022, WWIV Software Services             */
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
// ReSharper disable CppClangTidyCppcoreguidelinesMacroUsage
#ifndef INCLUDED_CORE_LOG_H
#define INCLUDED_CORE_LOG_H

#include "core/os.h"

#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#ifdef _MSC_VER
#include <sal.h>
#endif

typedef std::basic_ostream<char>&(ENDL_TYPE)(std::basic_ostream<char>&);

#if defined(_DEBUG) || !defined(NDEBUG)
#define WWIV_CORE_LOG_DEBUG
#endif

#ifdef _MSC_VER
#define WWIV_ANALYSIS_ASSUME(expr) _Analysis_assume_(expr)
#else
#define WWIV_ANALYSIS_ASSUME(expr)
#endif


#define LOG(LEVEL) LOG_##LEVEL
#define VLOG(LEVEL) LOG_VERBOSE(LEVEL)

#define LOG_IGNORED(x) wwiv::core::NullLogger()
#ifdef _MSC_VER
#define LOG_FATAL_IGNORED(x) __assume(x), wwiv::core::NullLogger()
#else
#define LOG_FATAL_IGNORED(x) wwiv::core::NullLogger()
#endif
#define LOG_STARTUP wwiv::core::Logger(wwiv::core::LoggerLevel::start, 0)
#define LOG_INFO wwiv::core::Logger(wwiv::core::LoggerLevel::info, 0)
#define LOG_WARNING wwiv::core::Logger(wwiv::core::LoggerLevel::warning, 0)
#define LOG_ERROR wwiv::core::Logger(wwiv::core::LoggerLevel::error, 0)
#define LOG_FATAL wwiv::core::Logger(wwiv::core::LoggerLevel::fatal, 0)
#define LOG_VERBOSE(verbosity) wwiv::core::Logger(wwiv::core::LoggerLevel::verbose, verbosity)

#define CHECK(x)                                                                                   \
  if (!(x))                                                                                        \
  wwiv::core::Logger(wwiv::core::LoggerLevel::fatal, 0)                                            \
      << "Failed Precondition: at " << __FILE__ << ":" << __LINE__ << " Condition: " #x " "        \
      << wwiv::os::stacktrace()

#define CHECK_LE(x, y) LOG_IF(!((x) <= (y)), FATAL)
#define CHECK_EQ(x, y) LOG_IF(!((x) == (y)), FATAL)
#define CHECK_NE(x, y) LOG_IF(!((x) != (y)), FATAL)
#define CHECK_GE(x, y) LOG_IF(!((x) >= (y)), FATAL)
#define CHECK_GT(x, y) LOG_IF(!((x) > (y)), FATAL)

#ifdef WWIV_CORE_LOG_DEBUG
#define DCHECK(x) CHECK(x)

#define DCHECK_LE(x, y) CHECK_LE(x, y)

#define DCHECK_EQ(x, y) CHECK_EQ(x, y)

#define DCHECK_NE(x, y) CHECK_NE(x, y)

#define DCHECK_GE(x, y) CHECK_GE(x, y)

#define DCHECK_GT(x, y) CHECK_GT(x, y)

#define DLOG(LEVEL) LOG_##LEVEL

#else
#define DCHECK(x) LOG_FATAL_IGNORED(x)
#define DCHECK_LE(x, y) LOG_FATAL_IGNORED(x)
#define DCHECK_EQ(x, y) LOG_FATAL_IGNORED(x)
#define DCHECK_NE(x, y) LOG_FATAL_IGNORED(x)
#define DCHECK_GE(x, y) LOG_FATAL_IGNORED(x)
#define DCHECK_GT(x, y) LOG_FATAL_IGNORED(x)
#define DLOG(LEVEL) LOG_IGNORED(LEVEL)
#endif

#define CHECK_NOTNULL(x) CHECK((x) != nullptr)
#define LOG_IF(condition, LEVEL)                                                                   \
  if (condition)                                                                                   \
  LOG(LEVEL)

#define VLOG_IS_ON(level) wwiv::core::Logger::vlog_is_on(level)

namespace wwiv::core {

enum class LoggerLevel { ignored, start, debug, verbose, info, warning, error, fatal };

class Appender {
public:
  virtual ~Appender() = default;

  Appender() = default;
  virtual bool append(const std::string& message) = 0;
};

typedef std::unordered_map<LoggerLevel, std::unordered_set<std::shared_ptr<Appender>>>
log_to_map_t;
typedef std::function<std::string()> timestamp_fn;
typedef std::function<std::string(std::string)> logdir_fn;


class LoggerConfig {
public:
  LoggerConfig();
  explicit LoggerConfig(logdir_fn f);
  LoggerConfig(logdir_fn l, timestamp_fn t);
  void add_appender(LoggerLevel level, const std::shared_ptr<Appender>& appender);
  void reset();

  // Change to true to enabled the startup log globally for all binaries.
  bool log_startup{false};
  std::string exit_filename;
  std::string log_filename;
  int cmdline_verbosity{0};
  bool register_file_destinations{true};
  bool register_console_destinations{true};
  log_to_map_t log_to;
  logdir_fn logdir_fn_;
  timestamp_fn timestamp_fn_;
};

class NullLogger {
public:
  NullLogger() noexcept = default;

  void operator&(std::ostream&) const noexcept {}

  template <class T> NullLogger& operator<<(const T&) noexcept { return *this; }
  NullLogger& operator<<(ENDL_TYPE*) noexcept { return *this; }
};

/**
 * Logger class for WWIV.
 * Usage:
 *
 * Once near your main() method, invoke Logger::Init(argc, argv) to initialize
 * the logger. This will initialize the logger filename to be your
 * executable's name with .log appended. You should also also invoke ExitLogger
 * when exiting your binary,
 *
 * Example:
 *
 *   LoggerConfig lc(LogDirFromConfig);
 *   Logger::Init(argc, argv, lc);
 *   auto at_exit = finally(Logger::ExitLogger);
 *
 * In code, just use "LOG(INFO) << messages" and it will end up in the information logs.
 */
class Logger {
public:
  Logger() noexcept
    : Logger(LoggerLevel::info, 0) {
  }

  explicit Logger(LoggerLevel level) noexcept
    : Logger(level, 0) {
  }

  Logger(LoggerLevel level, int verbosity) noexcept;
  ~Logger() noexcept;

  /** Initializes the WWIV Loggers.  Must be invoked once per binary. */
  static void Init(int argc, char** argv, LoggerConfig& config);
  static void ExitLogger();
  static bool vlog_is_on(int level);
  static LoggerConfig& config() noexcept { return config_; }
  static void set_cmdline_verbosity(int cmdline_verbosity);

  template <class T> Logger& operator<<(const T& msg) noexcept {
    try {
      ss_ << msg;
    } catch (...) {
      // NOOP
    }
    used_ = true;
    return *this;
  }

  Logger& operator<<(ENDL_TYPE* m) noexcept {
    used_ = true;
    try {
      ss_ << m;
    } catch (...) {
      // NOOP
    }
    return *this;
  }

private:
  static void StartupLog(int argc, char* argv[]);
  [[nodiscard]] std::string FormatLogMessage(LoggerLevel level, int verbosity,
                                             const std::string& msg) const noexcept;
  static LoggerConfig config_;
  LoggerLevel level_;
  int verbosity_;
  bool used_{false};
  std::ostringstream ss_;
};

} // namespace

#endif
