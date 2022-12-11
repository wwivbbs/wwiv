/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
// Always declare wwiv_windows.h first to avoid collisions on defines.
#include "core/wwiv_windows.h"

#include "common/remote_io.h"
#include "core/scope_exit.h"
#include "fmt/format.h"
#include <string>

#ifndef _WIN32
// for strerror_r
#include <string.h>
#endif  // !_WIN32

using wwiv::core::finally;

namespace wwiv::common {

// static
std::string RemoteIO::error_text_;

void RemoteIO::set_binary_mode(bool b) {
  binary_mode_ = b;
}

std::string RemoteIO::GetLastErrorText() {
#if defined ( _WIN32 )
  char* error_text;
  
  FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS,
    nullptr,
    GetLastError(),
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
    LPTSTR(&error_text),
    0,
    nullptr
  );
  error_text_.assign(error_text);
  LocalFree(error_text);
#else
  // int strerror_r(int errnum, char *buf, size_t buflen);
  char buf[500];
  if (strerror_r(errno, buf, sizeof(buf)) != 0) {
    buf[0] = '\0';
  }
  return fmt::format("errno: {} (num: {})", buf, errno);
#endif
  return error_text_;
}

std::optional<ScreenPos> RemoteIO::screen_position() { 
  return ScreenPos{0, 0};
}

} // namespace wwiv::common
