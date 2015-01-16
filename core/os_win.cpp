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

using std::string;
using std::stringstream;
using namespace wwiv::strings;

namespace wwiv {
namespace os {

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
    SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
    out << frames - i - 1 << ": " << symbol->Name << " = " << std::hex << symbol->Address << std::endl;
  }
  free(symbol);
  return out.str();
}


}  // namespace os
}  // namespace wwiv
