/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                  Copyright (C)2015-2020 WWIV Software Services         */
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

#include "core/strings.h"
#include "core/file.h"
#include "fmt/format.h"
#include <string>

#include <signal.h>
#include <unistd.h>

using namespace std::chrono;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::os {

void sleep_for(duration<double> d) {
  // usleep is microseconds.
  const auto ns = duration_cast<microseconds>(d);
  usleep(static_cast<__useconds_t>(ns.count()));
}

void sound(uint32_t frequency, duration<double> d) {
  // NOP
}

std::string os_version_string() {
#if defined ( __linux__ )
  const auto fn = FilePath("/proc/sys/kernel", "osrelease").string();
  if (File::Exists(fn)) {
    File info(fn);
    FILE* kernel_file;
    struct k_version { unsigned major, minor, update, iteration; };
    struct k_version k_version;
    kernel_file = fopen("/proc/sys/kernel/osrelease","r");
    fscanf(kernel_file,"%u%*c%u%*c%u%*c%u",
	   &k_version.major,
	   &k_version.minor,
	   &k_version.update,
	   &k_version.iteration);
    fclose(kernel_file);
    char osrelease[100];
    sprintf(osrelease,"%u.%u.%u-%u", k_version.major,
	    k_version.minor,
	    k_version.update,
	    k_version.iteration);
    return fmt::format("Linux {}", osrelease);
  }
  return "Linux";
#elif defined ( __APPLE__ )
  return "MacOSX"; // fmt::format("{} {}", GetOSNameString(), GetMacVersionString());
#elif defined(__OS2__)
  return "OS/2";
#elif defined(__FreeBSD__)
  return "FreeBSD";
#elif defined(__OpenBSD__)
  return "OpenBSD";
#elif defined(__NetBSD__)
  return "NetBSD";
#elif defined(BSD)
  return "BSD";
#elif defined(__solaris__)
  return "Solaris";
#elif defined(__sun__)
  return "SunOS";
#elif defined(__gnu__)
  return "GNU/Hurd";
#elif defined(__QNX__)
  return "QNX";
#elif defined(__unix__)
  return "UNIX";
#elif defined(__HAIKU__)
  return "Haiku";
#endif
  return "UNKNOWN OS";
}

bool set_environment_variable(const std::string& variable_name, const std::string& value) {
  return setenv(variable_name.c_str(), value.c_str(), 1) == 0;
}

std::string environment_variable(const std::string& variable_name) {
  if (const auto* s = getenv(variable_name.c_str()); s != nullptr) {
    return s;
  }
  return {};
}

std::string stacktrace() { 
  return {};
}

pid_t get_pid() {
  return getpid();
}

bool terminate_process(pid_t pid) {
  kill(pid, SIGKILL);
  return true;
}


} // namespace wwiv
