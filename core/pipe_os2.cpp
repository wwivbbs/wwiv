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

#define INCL_DOSERRORS
#define INCL_DOSPROCESS
#include <os2.h>

#include "core/log.h"
#include "fmt/format.h"

// Values from: The Art Of OS/2 Warp Programming. Chapter 5
static constexpr ULONG PIPE_OPEN_FLAG = OPEN_ACTION_OPEN_IF_EXISTS;
static constexpr ULONG PIPE_OPEN_MODE = (OPEN_FLAGS_WRITE_THROUGH | OPEN_FLAGS_FAIL_ON_ERROR | 
                                         OPEN_FLAGS_RANDOM | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE);

namespace wwiv::core {

Pipe::PIPE_HANDLE create_pipe(const std::string& name) {
  Pipe::PIPE_HANDLE hPipe;
  auto rc = DosCreateNPipe((const unsigned char*)name.c_str(), &hPipe, NP_ACCESS_DUPLEX,
                           NP_NOWAIT | NP_TYPE_BYTE | NP_READMODE_BYTE | 0xFF, 
			   Pipe::PIPE_BUFFER_SIZE,
                           Pipe::PIPE_BUFFER_SIZE,
                           0); // 0 == No Wait.
  VLOG(2) << "create_pipe(" << name << "); handle: " << hPipe;
  return hPipe;
}

bool Pipe::WaitForClient(std::chrono::duration<double> timeout) {
  auto end = std::chrono::system_clock::now() + timeout;
  do {
    if (DosConnectNPipe(handle_) == NO_ERROR) {
      return true;
    }
    DosSleep(200);
  } while (std::chrono::system_clock::now() <= end);
  return false;
}

std::string pipe_name(const std::string_view part) { 
  return fmt::format("\\PIPE\\{}", part); 
}

bool close_pipe(Pipe::PIPE_HANDLE h, bool server) {
  VLOG(2) << "close_pipe(" << h << ")";
  return DosDisConnectNPipe(h) == NO_ERROR;
}


/** returns the number of bytes written on success */
std::optional<int> Pipe::write(const char* data, int size) {
  unsigned long num_written;
  auto rc = DosWrite(handle_, data, size, &num_written);
  VLOG(4) << "Wrote bytes to pipe: " << num_written;
  if (rc == NO_ERROR) {
    return {num_written};
  }
  DosSleep(1);
  return std::nullopt;
}

/** returns the number of bytes read on success */
std::optional<int> Pipe::read(char* data, int size) {
  unsigned long num_read;
  if (auto rc = DosRead(handle_, data, size, &num_read); rc == NO_ERROR && num_read > 0) {
    VLOG(4) << "Read bytes from pipe: " << num_read;
    return {num_read};
  } else if (rc != 232 && rc != 0) {
    last_error_ = rc;
    return std::nullopt;
  } else {
    return {0};
  }
}

bool Pipe::Open() {
  VLOG(2) << "Pipe::Open: " << pipe_name_;
  for (int i=0; i<5*10; i++) { // 10s
    HFILE h;
    ULONG ulAction;
    auto rc = DosOpen((const unsigned char*)pipe_name_.c_str(), 
		      &h, 
		      &ulAction, 
		      0, 
		      FILE_NORMAL, 
		      PIPE_OPEN_FLAG,
		      PIPE_OPEN_MODE,
		      0L);
    if (rc == NO_ERROR) {
      VLOG(3) << "Pipe::Open: exiting: " << pipe_name_;
      handle_ = h;
      return true;
    }
    LOG(WARNING) << "Could not open pipe. Error: " << rc;
    DosSleep(200);
  }
  LOG(WARNING) << "Pipe::Open: failed to open: " << pipe_name_;
  return false;
}

std::optional<char> Pipe::peek() {
  char ch;
  ULONG num_read, bytes_avail, pipe_state;
  PAVAILDATA num_avail;

  if (PeekNamedPipe(handle_, &ch, 1, &num_read, &num_avail, &bytes_avail, &pipe_state)) {
    // TODO check if state is 4 (NP_STATE_CLOSING)?
    return {ch};
  }
  return std::nullopt;
}




}
