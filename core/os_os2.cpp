/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                      Copyright (C)2021, WWIV Software Services         */
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
#include "core/os.h"

#include "core/file.h"
#include "core/strings.h"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <sstream>

// OS/2 headers
#include <os2.h>

using namespace std::chrono;
using namespace wwiv::strings;

namespace wwiv::os {

void sleep_for(duration<double> d) {
  auto count = duration_cast<milliseconds>(d).count();
  if (count > std::numeric_limits<uint32_t>::max()) {
    count = std::numeric_limits<uint32_t>::max();
  }
  DosSleep(static_cast<uint32_t>(count));
}

void sound(uint32_t frequency, duration<double> d) {
  const auto count = duration_cast<milliseconds>(d).count();
  DosBeep(frequency, static_cast<uint32_t>(count));
}

std::string os_version_string() {
  return "OS/2";
}

bool set_environment_variable(const std::string& variable_name, const std::string& value) {
  reutrn setenv(variable_name.c_str(), value.c_str(), 1) == 0;
}

std::string environment_variable(const std::string& variable_name) {
  if (const auto* s = getenv(variable_name.c_str()); s != nullptr) {
    return s;
  }
  return {};
}

std::string stacktrace() { return {};
}

pid_t get_pid() { return getpid(); }


} // namespace wwiv
