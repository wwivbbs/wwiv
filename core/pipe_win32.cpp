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

static std::string ErrorAsString(DWORD last_error) {
  LPSTR messageBuffer{nullptr};
  const auto size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                       FORMAT_MESSAGE_IGNORE_INSERTS,
                                   nullptr, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                   reinterpret_cast<LPSTR>(&messageBuffer), 0, nullptr);

  std::string message{messageBuffer, size};

  // Free the buffer.
  LocalFree(messageBuffer);
  return fmt::format("({}): {}", last_error, message);
}

 
Pipe::PIPE_HANDLE create_pipe(const std::string& name) {
  VLOG(2) << "create_pipe: " << name;
  Pipe::PIPE_HANDLE hPipe =
      CreateNamedPipe(name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                      0xff, Pipe::PIPE_BUFFER_SIZE, Pipe::PIPE_BUFFER_SIZE, 0 /* default timeout */, NULL);
  VLOG(4) << "create_pipe(" << name << "); handle: " << hPipe;
  return hPipe;
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

bool Pipe::Open() {
  VLOG(2) << "Pipe::Open: " << pipe_name_;
  for (int i=0; i<30 * 5; i++) {
    HANDLE h = CreateFile(pipe_name_.c_str(), GENERIC_READ | GENERIC_WRITE,
                          0,             // no sharing
                          NULL,          // default security attributes
                          OPEN_EXISTING, // opens existing pipe
                          0,             // default attributes
                          NULL);         // no template file
    if (h != INVALID_HANDLE_VALUE) {
      handle_ = h;
      VLOG(3) << "Pipe::Open: sucess opened: " << pipe_name_;
      return true;
    }
    // Exit if an error other than ERROR_PIPE_BUSY occurs.
    const auto e = GetLastError();
    if (e != ERROR_PIPE_BUSY && e != ERROR_FILE_NOT_FOUND) {
      LOG(WARNING) << "Could not open pipe. Error: " << ErrorAsString(e);
      return false;
    }
    Sleep(200);
  }
  VLOG(3) << "Pipe::Open: exiting: " << pipe_name_;
  return false;
}

bool Pipe::WaitForClient(std::chrono::duration<double> timeout) { 
  auto end = std::chrono::system_clock::now() + timeout;
  do {
    if (ConnectNamedPipe(handle_, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) {
      return true;
    }
    LOG(WARNING) << "WaitForClient: " << ErrorAsString(GetLastError());
    Sleep(250);
  } while (std::chrono::system_clock::now() <= end);
  return false;
}


/** returns the number of bytes written on success */
std::optional<int> Pipe::write(const char* data, int size) { 
  DWORD cbBytesWritten;
  if (WriteFile(handle_, data, size, &cbBytesWritten, NULL)) {
    return {cbBytesWritten};
  }
  return std::nullopt; 
}

/** returns the number of bytes read on success */
std::optional<int> Pipe::read(char* data, int size) { 
  DWORD cbBytesRead;
  if (ReadFile(handle_, data, size, &cbBytesRead, NULL)) {
    return { cbBytesRead };
  }
  return std::nullopt;
}

std::optional<char> Pipe::peek() {
  char ch;
  DWORD num_read, num_avail, num_leftmsg;
  if (PeekNamedPipe(handle_, &ch, 1, &num_read, &num_avail, &num_leftmsg) && num_read>0) {
    return {ch};
  }
  return std::nullopt;
}

}
