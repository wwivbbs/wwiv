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

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <DbgHelp.h>

#endif  // _WIN32

#include "core/strings.h"
#include "core/file.h"

#pragma comment (lib, "DbgHelp.lib")

using std::numeric_limits;
using std::chrono::milliseconds;
using std::string;
using std::stringstream;
using namespace wwiv::strings;

namespace wwiv {
namespace os {

void sleep_for(milliseconds d) {
  int64_t count = d.count();
  if (count > numeric_limits<uint32_t>::max()) {
    count = numeric_limits<uint32_t>::max();
  }
  ::Sleep(static_cast<uint32_t>(count));
}

void sound(uint32_t frequency, std::chrono::milliseconds d) {
  ::Beep(frequency, static_cast<uint32_t>(d.count()));
}

std::string os_version_string() {

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
  }
  return StringPrintf("WIN32 Compatable OS v%d%c%d", os.dwMajorVersion, '.', os.dwMinorVersion);
}

bool set_environment_variable(const std::string& variable_name, const std::string value) {
  return ::SetEnvironmentVariable(variable_name.c_str(), value.c_str()) ? true : false;
}

string stacktrace() {
  HANDLE process = GetCurrentProcess();
  SymInitialize(process, NULL, TRUE);
  void* stack[100];
  uint16_t frames = CaptureStackBackTrace(0, 100, stack, NULL);
  SYMBOL_INFO* symbol = (SYMBOL_INFO *) calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
  symbol->MaxNameLen = 255;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

  stringstream out;
  // start at one to skip this current frame.
  for(std::size_t i = 1; i < frames; i++) {
    if (SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol)) {
      out << frames - i - 1 << ": " << symbol->Name << " = " << std::hex << symbol->Address;
    }
    IMAGEHLP_LINE64 line;
    DWORD displacement;
    if (SymGetLineFromAddr64(process, (DWORD64)stack[i], &displacement, &line)) {
      out << " (" << line.FileName << ": " << std::dec << line.LineNumber << ") ";
    }
    out << std::endl;
  }
  free(symbol);
  return out.str();
}


}  // namespace os
}  // namespace wwiv
