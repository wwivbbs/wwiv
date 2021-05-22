/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services            */
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
#include "core/pipe.h"

// Always declare wwiv_windows.h first to avoid collisions on defines.
#include "core/wwiv_windows.h"

#include "core/log.h"
#include "fmt/format.h"

namespace wwiv::core {
 
Pipe::PIPE_HANDLE create_pipe(const char* name) {
  auto full_name = pipe_name(name);
  Pipe::PIPE_HANDLE hPipe =
      CreateNamedPipe(full_name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                      0xff, 4000, 4000, 0 /* default timeout */, NULL);
  VLOG(2) << "create_pipe(" << full_name << "); handle: " << hPipe;
  return hPipe;
}

bool connect_pipe(Pipe::PIPE_HANDLE h) {
  VLOG(2) << "Waiting for pipe to connect: [handle: " << h << "]";
  int i = 0;
  for (;;) {
    if (i++ > 1000) {
      LOG(ERROR) << "Failed to connect to pipe: " << h;
      // Failed to connect to pipe.
      return false;
    }
    if (ConnectNamedPipe(h, NULL)) {
      VLOG(1) << "connected to pipe: " << h;
      Sleep(100);
      return true;
    }
    Sleep(100);
    Sleep(0);
  }
}

std::string pipe_name(const std::string_view part) {
  return fmt::format("\\\\.\\PIPE\\{}", part);
}

bool close_pipe(Pipe::PIPE_HANDLE h) {
  VLOG(2) << "close_pipe(" << h << ")";
  if (h == INVALID_HANDLE_VALUE) {
    return true;
  }
  DisconnectNamedPipe(reinterpret_cast<HANDLE>(h));
  CloseHandle(reinterpret_cast<HANDLE>(h));
  return true;
}


}
