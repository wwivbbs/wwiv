/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014,WWIV Software Services             */
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
#include <random>
#include <string>

#include "wwiv.h"
#include "core/strings.h"
#include "core/wutil.h"
#include "core/wwivport.h"

#if defined(__APPLE__)
#include "macversioninfo.h"
#endif  // __APPLE__

using std::string;
using wwiv::strings::StringPrintf;

void WWIV_Sound(int nFreq, int nDly) {
#ifdef _WIN32
  ::Beep(nFreq, nDly);
#endif
}


int WWIV_GetRandomNumber(int nMaxValue) {
  static std::random_device rdev;
  static std::default_random_engine re(rdev());

  std::uniform_int_distribution<int> dist(0, nMaxValue - 1);
  return dist(re);
}

string WWIV_GetOSVersion() {
#if defined (_WIN32)
  OSVERSIONINFO os;
  os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  if (!GetVersionEx(&os)) {
    return string("WIN32");
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
	WFile info("/proc/sys/kernel", "osrelease");
	if(info.Exists()) {
         FILE *kernel_file;
         struct k_version {
               unsigned major,minor,update,iteration;};
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
	return string("Linux");
#elif defined ( __APPLE__ )
  return StringPrintf("%s %s", GetOSNameString(), GetMacVersionString());
#elif defined ( __unix__ )
  // TODO Add Linux version information code here..
  return string("UNIX");
#else
#error "What's the platform here???"
#endif
  return string("UNKNOWN OS");
}
