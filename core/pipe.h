/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services            */
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
#ifndef INCLUDED_CORE_PIPE_H
#define INCLUDED_CORE_PIPE_H

#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace wwiv::core {

class Pipe {
public:

#ifdef _WIN32
  typedef void* PIPE_HANDLE;
#elif defined(__OS2__)
  // HFILE is unsigned long
  typedef unsigned long HFILE;
  typedef HFILE PIPE_HANDLE;
#endif

  static constexpr int PIPE_BUFFER_SIZE = 4000;

  static constexpr PIPE_HANDLE PIPE_INVALID_HANDLE_VALUE = ((PIPE_HANDLE)-1);

  explicit Pipe(const std::string_view name);
  Pipe(int node_number, bool control_pipe);
  Pipe(int node_number);
  ~Pipe();

  /** Creates a new Named Pipe */
  bool Create();

  /** Opens an existing named pipe */
  bool Open();

  /** Waits for a client to connect up time timeout */
  bool WaitForClient(std::chrono::duration<double> timeout);

  /** returns the number of bytes written on success */
  std::optional<int> write(const char* data, int size);

  /** returns the number of bytes read on success */
  std::optional<int> read(char* data, int size);

  /** returns the last error code */
  int last_error() const noexcept { return last_error_ ; }

  /** Returns the optional character that will be returned next */
  std::optional<char> peek();

  /**
   * True while the pipe has a handle and the other end has not gone away.
   * Once read, write or peek sees that the other end closed the pipe this
   * returns false, but the handle is only released by Close() or the
   * destructor, so another thread never uses a handle that was closed
   * (and possibly reused) under it.
   */
  [[nodiscard]] bool IsOpen() const noexcept;

  /** Close the pipe */
  bool Close();

  [[nodiscard]] std::string name() const noexcept;

  /** returns true if this end is the server of the pipe */
  [[nodiscard]] bool IsServer() const noexcept { return server_; }
 
private:
  std::string pipe_name_;

  /** Records last_error and marks the pipe broken if the other end has gone away. */
  void set_last_error(int e) noexcept;

  PIPE_HANDLE handle_{PIPE_INVALID_HANDLE_VALUE};
  std::atomic<bool> broken_{false};
  int last_error_{0};
  bool server_{false};
};

// Platform specific bits
Pipe::PIPE_HANDLE create_pipe(const std::string& name);
bool connect_pipe(Pipe::PIPE_HANDLE h);
std::string pipe_name(const std::string_view part);
bool close_pipe(Pipe::PIPE_HANDLE h, bool server);

}

#endif  // INCLUDED_CORE_PIPE_H
