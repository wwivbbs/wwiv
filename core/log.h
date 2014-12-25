/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*               Copyright (C)2014, WWIV Software Services                */
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

#include <map>
#include <string>
#include <sstream>

namespace wwiv {
namespace core {

class Logger {
public:
  Logger();
  explicit Logger(const std::string& kind);
  ~Logger();

  template <typename T> Logger & operator<<(T const & value) {
    stream_ << value;
    return *this;
  }

  static void Init(int argc, char** argv);
  static void set_filename(const std::string& kind, const std::string& filename) { fn_map_[kind] = filename; }
  static std::string date_time();

private:
  const std::string kind_;
  std::ostringstream stream_;
  static std::map<std::string, std::string> fn_map_;
};

}
}

#endif  // __INCLUDED_CORE_LOG_H__
