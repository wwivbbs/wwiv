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

Pipe::PIPE_HANDLE create_pipe(const std::string& name) {
  PIPE_HANDLE hPipe;
  auto rc = DosCreateNPipe((const unsigned char*)name.c_str(), &hPipe, NP_ACCESS_DUPLEX,
                           NP_NOWAIT | NP_TYPE_BYTE | NP_READMODE_BYTE | 0xFF, PIPE_BUFFER_SIZE,
                           PIPE_BUFFER_SIZE,
                           0); // 0 == No Wait.
  VLOG(2) << "create_pipe(" << name << "); handle: " << hPipe;
  return hPipe;
}

bool Pipe::WaitForClient(std::chrono::duration<double> timeout) {
  auto end = std::chrono::system_clock::now() + timeout;
  do {
    if (DosConnectNPipe(h) == NO_ERROR) {
      return true;
    }
    DosSleep(200);
  } while (std::chrono::system_clock::now() <= end);
  return false;
}

std::string pipe_name(const std::string_view part) { 
  return fmt::format("\\PIPE\\{}", part); 
}

bool close_pipe(Pipe::PIPE_HANDLE h) {
  VLOG(2) << "close_pipe(" << h << ")";
  return DosDisConnectNPipe(h) == NO_ERROR;
}


/** returns the number of bytes written on success */
std::optional<int> Pipe::write(const char* data, int size) {
  unsigned long num_written;
  auto rc = DosWrite(h, buffer, num_read, &num_written);
  VLOG(4) << "Wrote bytes to pipe: " << num_written;
  if (rc == NO_ERROR) {
    return {num_wrtten};
  }
  DosSleep(1);
  return std::nullopt;
}

/** returns the number of bytes read on success */
std::optional<int> Pipe::read(char* data, int size) {
  if (auto rc = DosRead(h, &ch, 1, &num_read); rc == NO_ERROR && num_read > 0) {
    VLOG(4) << "Read bytes from pipe: " << num_read;
    return {num_read};
  } else if (rc != 232 && rc != 0) {
    last_error_ = rc;
    return std::nullopt;
  } else {
    return {0};
  }
}


}
