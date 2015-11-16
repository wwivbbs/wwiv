/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*                    Copyright (C)2015 WWIV Software Services            */
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

#include <unistd.h>

#include "core/strings.h"
#include "core/file.h"

using std::chrono::milliseconds;
using std::string;
using namespace wwiv::strings;

namespace wwiv {
namespace os {

void sleep_for(milliseconds d) {
  // usleep is nanoseconds.
  usleep (d.count() * 1000);
}

void sound(uint32_t frequency, std::chrono::milliseconds d) {
  // NOP
}

std::string os_version_string() {
#if defined ( __linux__ )
  File info("/proc/sys/kernel", "osrelease");
  if (info.Exists()) {
    FILE *kernel_file;
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
    info.Close();
    return StringPrintf("Linux %s", osrelease);
  }
  return std::string("Linux");
#elif defined ( __APPLE__ )
  return string(""); // StringPrintf("%s %s", GetOSNameString(), GetMacVersionString());
#elif defined(_WIN64)
  return string("Win64");
#elif defined(_WIN32)
  return string("Win32");
#elif defined(__OS2__)
  return string("OS/2");
#elif defined(__MSDOS__)
  return string("DOS");
#elif defined(__linux__)
  return string("Linux");
#elif defined(__FreeBSD__)
  return string("FreeBSD");
#elif defined(__OpenBSD__)
  return string("OpenBSD");
#elif defined(__NetBSD__)
  return string("NetBSD");
#elif defined(__APPLE__) && defined(__MACH__) && defined(__POWERPC__)
  return string("MacOSX");
#elif defined(BSD)
  return string("BSD");
#elif defined(__solaris__)
  return string("Solaris");
#elif defined(__sun__)
  return string("SunOS");
#elif defined(__gnu__)
  return string("GNU/Hurd");
#elif defined(__QNX__)
  return string("QNX");
#elif defined(__unix__)
  // TODO Add Linux version information code here..
  return string("UNIX");
#elif defined(__HAIKU__)
  return string("Haiku");
#endif
  return string("UNKNOWN OS");
}

bool set_environment_variable(const std::string& variable_name, const std::string value) {
  return setenv(variable_name.c_str(), value.c_str(), 1) == 0;
}

string stacktrace() { return string(); }


}  // namespace os
}  // namespace wwiv
