/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*             Copyright (C)2014-2015 WWIV Software Services              */
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
#include <map>
#include <string>
#include <sstream>

#define LOG wwiv::core::Logger()

namespace wwiv {
namespace core {

/**
 * Logger class for WWIV.
 * Usage:
 *   - Once near your main() method, invoke Logger::Init(argc, argv) to initialize the logger.
 *     This will initialize the logger for (I)nfo to your executable's name with .LOG appended.
 *     You should also invoke ExitLogger when exiting your binary,
 * 
 * Example:
 *
 *   Logger::Init(argc, argv);
 *   wwiv::core::ScopeExit at_exit(Logger::ExitLogger);
 *
 * In code, just use "LOG << messages" and it will end up in the information logs.
 */
class Logger {
public:
  Logger();
  explicit Logger(const std::string& kind);
  ~Logger();

  template <typename T> Logger & operator<<(T const & value) {
    stream_ << value;
    return *this;
  }

  /** Initializes the WWIV Loggers.  Must be invoked once per binary. */
  static void Init(int argc, char** argv);
  /** Sets the filename for the logger kind (i.e. 'I' for info) */
  static void set_filename(const std::string& kind, const std::string& filename) { fn_map_[kind] = filename; }
  static std::string date_time();
  static void ExitLogger();
  static void DefaultDisplay(const std::string& s);

private:
  const std::string kind_;
  std::ostringstream stream_;
  static std::map<std::string, std::string> fn_map_;
  std::function<void(const std::string&)> display_fn_;
};

}
}

#endif  // __INCLUDED_CORE_LOG_H__
