/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                 Copyright (C)2015-2021, WWIV Software Services         */
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

// Windows headers
#include <Windows.h>
#include <DbgHelp.h>
#include <process.h>

#pragma comment (lib, "DbgHelp.lib")

using namespace std::chrono;
using namespace wwiv::strings;

namespace wwiv::os {

void sleep_for(duration<double> d) {
  auto count = duration_cast<milliseconds>(d).count();
  if (count > std::numeric_limits<uint32_t>::max()) {
    count = std::numeric_limits<uint32_t>::max();
  }
  Sleep(static_cast<uint32_t>(count));
}

void sound(uint32_t frequency, duration<double> d) {
  const auto count = duration_cast<milliseconds>(d).count();
  Beep(frequency, static_cast<uint32_t>(count));
}

std::string os_version_string() {
  return "Windows";
}

bool set_environment_variable(const std::string& variable_name, const std::string& value) {
  return ::SetEnvironmentVariable(variable_name.c_str(), value.c_str()) ? true : false;
}

std::string environment_variable(const std::string& variable_name) {
  // See http://techunravel.blogspot.com/2011/08/win32-env-variable-pitfall-of.html
  // Use Win32 functions to get since we do to set...
  char buffer[4096];
  if (const auto size = GetEnvironmentVariable(variable_name.c_str(), buffer, 4096); size == 0) {
    // error or does not exits.
    return "";
  }
  return std::string(buffer);
}

std::string stacktrace() {
  // ReSharper disable once CppUseAuto
  // ReSharper disable once CppLocalVariableMayBeConst
  auto process = GetCurrentProcess();
  if (process == nullptr) {
    return "";
  }
  SymInitialize(process, nullptr, TRUE);
  void* stack[100];
  const auto frames = CaptureStackBackTrace(0, 100, stack, nullptr);
  auto* symbol = static_cast<SYMBOL_INFO *>(calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1));
  if (symbol == nullptr) {
    return "";
  }
  symbol->MaxNameLen = 255;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

  std::stringstream out;
  // start at one to skip this current frame.
  for (auto i = 1; i < frames; i++) {
    if (SymFromAddr(process, reinterpret_cast<DWORD64>(stack[i]), nullptr, symbol)) {
      out << frames - i - 1 << ": " << symbol->Name << " = " << std::hex << symbol->Address;
    }
    IMAGEHLP_LINE64 line;
    DWORD displacement;
    if (SymGetLineFromAddr64(process, reinterpret_cast<DWORD64>(stack[i]), &displacement, &line)) {
      out << " (" << line.FileName << ": " << std::dec << line.LineNumber << ") ";
    }
    out << std::endl;
  }
  free(symbol);
  return out.str();
}

pid_t get_pid() {
  return _getpid();
}


} // namespace wwiv
