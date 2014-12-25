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
#include "core/log.h"

#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

namespace wwiv {
namespace core {

// static
std::map<std::string, std::string> Logger::fn_map_;

Logger::Logger(const std::string& kind) : kind_(kind) { 
  stream_ << kind_ << date_time() << " "; 
}

Logger::~Logger() {
  const std::string filename = Logger::fn_map_[kind_];
  std::ofstream out(filename, std::ofstream::app);
  if (!out) {
    std::clog << "Unable to open log file: '" << filename << "'" << std::endl;
  } else {
    out << stream_.str() << std::endl;
  }
  std::clog << stream_.str() << std::endl;
}

// static
std::string Logger::date_time() {
  std::time_t t = std::time(NULL);
  char s[255];
  strftime(s, sizeof(s), "%Y%m%d:%H%M%S", std::localtime(&t));
  return std::string(s);
}

}
}
