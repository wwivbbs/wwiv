/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*               Copyright (C)2014-2015 WWIV Software Services            */
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

#include <chrono>
#include <cstdlib>
#include <functional>
#include <limits>
#include <random>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#else  // _WIN32
#include <unistd.h>

#endif  // _WIN32

#include "core/strings.h"
#include "core/file.h"

using std::function;
using std::string;
using std::numeric_limits;
using std::chrono::milliseconds;
using namespace wwiv::strings;

namespace wwiv {
namespace os {

bool wait_for(function<bool()> predicate, milliseconds d) {
  auto now = std::chrono::steady_clock::now();
  auto end = now + d;
  while (!predicate() && now < end) {
    now = std::chrono::steady_clock::now();
    sleep_for(milliseconds(100));
  }
  return predicate();
}

void sleep_for(milliseconds d) {
#ifdef _WIN32
  int64_t count = d.count();
  if (count > numeric_limits<uint32_t>::max()) {
    count = numeric_limits<uint32_t>::max();
  }
  ::Sleep(static_cast<uint32_t>(count));

#else  // _WIN32
  usleep (d.count() * 1000);
#endif  // _WIN32
}

void yield() {
  // TODO(rushfan): use this_thread::yield once it's been
  // tested on MSVC and GCC (Maybe with MSVC 2015).
  // Then we'll call this:
  // std::this_thread::yield();

  // Also TODO is to investigate if we should use sleep(1) vs. sleep(0) so that we'll
  // yield to threads on other processors.
  // See http://stackoverflow.com/questions/1413630/switchtothread-thread-yield-vs-thread-sleep0-vs-thead-sleep1
  sleep_for(milliseconds(0));
}

std::string os_version_string() {
#if defined (_WIN32)
  OSVERSIONINFO os;
  os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  if (!GetVersionEx(&os)) {
    return std::string("WIN32");
  }
  switch (os.dwPlatformId) {
  case VER_PLATFORM_WIN32_NT:
    if (os.dwMajorVersion == 5) {
      switch (os.dwMinorVersion) {
      case 0:
        return StringPrintf("Windows 2000 %s", os.szCSDVersion);
      case 1:
        return StringPrintf("Windows XP %s", os.szCSDVersion);
      case 2:
        return StringPrintf("Windows Server 2003 %s", os.szCSDVersion);
      default:
        return StringPrintf("Windows NT %ld%c%ld %s",
                                           os.dwMajorVersion, '.', os.dwMinorVersion, os.szCSDVersion);
      }
    } else if (os.dwMajorVersion == 6) {
      switch (os.dwMinorVersion) {
      case 0:
        return StringPrintf("Windows Vista %s", os.szCSDVersion);
      case 1:
        return StringPrintf("Windows 7 %s", os.szCSDVersion);
      case 2:
        return StringPrintf("Windows 8 %s", os.szCSDVersion);
      case 3:
        return StringPrintf("Windows 8.1 %s", os.szCSDVersion);
      default:
        return StringPrintf("Windows NT %ld%c%ld %s",
                                           os.dwMajorVersion, '.', os.dwMinorVersion, os.szCSDVersion);
      }
    }
    break;
  default:
    return StringPrintf("WIN32 Compatable OS v%d%c%d", os.dwMajorVersion, '.', os.dwMinorVersion);
  }
#elif defined ( __linux__ )
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
#elif defined ( __unix__ )
  // TODO Add Linux version information code here..
  return string("UNIX");
#else
#error "What's the platform here???"
#endif
  return string("UNKNOWN OS");
}

void sound(uint32_t frequency, std::chrono::milliseconds d) {
#ifdef _WIN32
  ::Beep(frequency, static_cast<uint32_t>(d.count()));
#endif  // _WIN32
}

int random_number(int max_value) {
  static std::random_device rdev;
  static std::default_random_engine re(rdev());

  std::uniform_int_distribution<int> dist(0, max_value - 1);
  return dist(re);
}

std::string environment_variable(const std::string& variable_name) {
  const char* s = getenv(variable_name.c_str());
  if (s == nullptr) {
    return "";
  }
  return string(s);
}

bool set_environment_variable(const std::string& variable_name, const std::string value) {
#ifdef _WIN32
  return ::SetEnvironmentVariable(variable_name.c_str(), value.c_str()) ? true : false;
#else
  return setenv(variable_name.c_str(), value.c_str(), 1) == 0;
#endif  // _WIN32
}


}  // namespace os
}  // namespace wwiv
