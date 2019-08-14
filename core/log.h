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
#ifndef __INCLUDED_CORE_LOG_H__
#define __INCLUDED_CORE_LOG_H__

#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

typedef std::basic_ostream<char>&(ENDL_TYPE)(std::basic_ostream<char>&);

#if defined(_DEBUG) || !defined(NDEBUG)
#define WWIV_CORE_LOG_DEBUG
#endif

#define LOG(LEVEL) LOG_##LEVEL
#define VLOG(LEVEL) LOG_VERBOSE(LEVEL)

#define LOG_IGNORED(x) wwiv::core::NullLogger()
#define LOG_STARTUP wwiv::core::Logger(wwiv::core::LoggerLevel::start, 0)
#define LOG_INFO wwiv::core::Logger(wwiv::core::LoggerLevel::info, 0)
#define LOG_WARNING wwiv::core::Logger(wwiv::core::LoggerLevel::warning, 0)
#define LOG_ERROR wwiv::core::Logger(wwiv::core::LoggerLevel::error, 0)
#define LOG_FATAL wwiv::core::Logger(wwiv::core::LoggerLevel::fatal, 0)
#define LOG_VERBOSE(verbosity) wwiv::core::Logger(wwiv::core::LoggerLevel::verbose, verbosity)
#define LOG_FATAL wwiv::core::Logger(wwiv::core::LoggerLevel::fatal, 0)

#define CHECK(x) LOG_IF(!(x), FATAL)
#define CHECK_LE(x, y) LOG_IF(!(x <= y), FATAL)
#define CHECK_EQ(x, y) LOG_IF(!(x == y), FATAL)
#define CHECK_NE(x, y) LOG_IF(!(x != y), FATAL)
#define CHECK_GE(x, y) LOG_IF(!(x >= y), FATAL)
#define CHECK_GT(x, y) LOG_IF(!(x > y), FATAL)
#ifdef WWIV_CORE_LOG_DEBUG
#define DCHECK_LE(x, y) CHECK_LE(x, y)

#define DCHECK_EQ(x, y) CHECK_EQ(x, y)

#define DCHECK_NE(x, y) CHECK_NE(x, y)

#define DCHECK_GE(x, y) CHECK_GE(x, y)

#define DCHECK_GT(x, y) CHECK_GT(x, y)

#define DLOG(LEVEL) LOG_##LEVEL

#else
#define DCHECK_LE(x, y) LOG_IGNORED(x)
#define DCHECK_EQ(x, y) LOG_IGNORED(x)
#define DCHECK_NE(x, y) LOG_IGNORED(x)
#define DCHECK_GE(x, y) LOG_IGNORED(x)
#define DCHECK_GT(x, y) LOG_IGNORED(x)
#define DLOG(LEVEL) LOG_IGNORED(LEVEL)
#endif

#define CHECK_NOTNULL(x) CHECK(x != nullptr)
#define LOG_IF(condition, LEVEL)                                                                   \
  if (condition)                                                                                   \
  LOG(LEVEL)

#define VLOG_IS_ON(level) wwiv::core::Logger::vlog_is_on(level)

namespace wwiv {
namespace core {

struct enum_hash {
  template <typename T>
  inline typename std::enable_if<std::is_enum<T>::value, std::size_t>::type
  operator()(T const value) const {
    return static_cast<std::size_t>(value);
  }
};

enum class LoggerLevel { ignored, start, debug, verbose, info, warning, error, fatal };

class Appender {
public:
  Appender(){};
  virtual bool append(const std::string& message) const = 0;
};

typedef std::unordered_map<LoggerLevel, std::unordered_set<std::shared_ptr<Appender>>, enum_hash>
    log_to_map_t;
typedef std::function<std::string()> timestamp_fn;
typedef std::function<std::string(std::string)> logdir_fn;


class LoggerConfig {
public:
  LoggerConfig();
  void add_appender(LoggerLevel level, std::shared_ptr<Appender> appender);
  void reset();

  bool log_startup{false};
  std::string exit_filename;
  std::string log_filename;
  int cmdline_verbosity{0};
  bool register_file_destinations{true};
  bool register_console_destinations{true};
  log_to_map_t log_to;
  timestamp_fn timestamp_fn_;
  logdir_fn logdir_fn_;
};

class NullLogger {
public:
  NullLogger() {}
  void operator&(std::ostream&) {}
  template <class T> NullLogger& operator<<(const T& msg) { return *this; }
  inline NullLogger& operator<<(ENDL_TYPE* m) { return *this; }
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
 *   Logger::Init(argc, argv);
 *   wwiv::core::ScopeExit at_exit(Logger::ExitLogger);
 *
 * In code, just use "LOG(INFO) << messages" and it will end up in the information logs.
 */
class Logger {
public:
  Logger() : Logger(LoggerLevel::info, 0) {}
  Logger(LoggerLevel level) : Logger(level, 0) {}
  Logger(LoggerLevel level, int verbosity);
  ~Logger();

  /** Initializes the WWIV Loggers.  Must be invoked once per binary. */
  static void Init(int argc, char** argv, LoggerConfig& config);
  static void Init(int argc, char** argv);
  static void ExitLogger();
  static bool vlog_is_on(int level);
  static LoggerConfig& config() noexcept { return config_; }
  static void set_cmdline_verbosity(int cmdline_verbosity);

  template <class T> Logger& operator<<(const T& msg) {
    ss_ << msg;
    used_ = true;
    return *this;
  }
  inline Logger& operator<<(ENDL_TYPE* m) {
    used_ = true;
    ss_ << m;
    return *this;
  }

private:
  static void StartupLog(int argc, char* argv[]);
  std::string FormatLogMessage(LoggerLevel level, int verbosity, const std::string& msg);
  static LoggerConfig config_;
  LoggerLevel level_;
  int verbosity_;
  bool used_{false};
  std::ostringstream ss_;
};

} // namespace core
} // namespace wwiv

#endif // __INCLUDED_CORE_LOG_H__
