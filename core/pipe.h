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
#ifndef INCLUDED_CORE_PIPE_H
#define INCLUDED_CORE_PIPE_H

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

  Pipe(const std::string_view name);
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

  [[nodiscard]] bool IsOpen() const noexcept;

  /** Close the pipe */
  bool Close();

  [[nodiscard]] std::string name() const noexcept;

private:
  std::string pipe_name_;

  PIPE_HANDLE handle_{PIPE_INVALID_HANDLE_VALUE};
  int last_error_{0};
};

// Platform specific bits
Pipe::PIPE_HANDLE create_pipe(const std::string& name);
bool connect_pipe(Pipe::PIPE_HANDLE h);
std::string pipe_name(const std::string_view part);
bool close_pipe(Pipe::PIPE_HANDLE h);

}

#endif  // INCLUDED_CORE_PIPE_H
