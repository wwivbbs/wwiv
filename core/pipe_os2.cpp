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

#include "core/log.h"
#include "fmt/format.h"

namespace wwiv::core {

Pipe::PIPE_HANDLE create_pipe(const char* name) {
  auto full_name = pipe_name(name);
  PIPE_HANDLE hPipe;
  auto rc = DosCreateNPipe((const unsigned char*)full_name.c_str(), &hPipe, NP_ACCESS_DUPLEX,
                           NP_NOWAIT | NP_TYPE_BYTE | NP_READMODE_BYTE | 0xFF, PIPE_BUFFER_SIZE,
                           PIPE_BUFFER_SIZE,
                           0); // 0 == No Wait.
  VLOG(2) << "create_pipe(" << full_name << "); handle: " << hPipe;
  return hPipe;
}

bool connect_pipe(Pipe::PIPE_HANDLE h) {
  VLOG(2) << "Waiting for pipe to connect: [handle: " << h;
  int i = 0;
  auto rc = NO_ERROR;
  do {
    if (i++ > 1000) {
      LOG(ERROR) << "Failed to connect to pipe: " << h;
      // Failed to connect to pipe.
      return false;
    }
    rc = DosConnectNPipe(h);
    if (rc != 0) {
      // Sleep for 100ms
      DosSleep(100);
      os_yield();
    }
  } while (rc != NO_ERROR);
  VLOG(1) << "connected to pipe: " << h;
  DosSleep(100);
  return true;
}

std::string pipe_name(const std::string_view part) { 
  return fmt::format("\\PIPE\\{}", part); 
}

bool close_pipe(Pipe::PIPE_HANDLE h) {
  VLOG(2) << "close_pipe(" << h << ")";
  return DosDisConnectNPipe(h) == NO_ERROR;
}


}
