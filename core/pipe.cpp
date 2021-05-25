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

Pipe::Pipe(const std::string_view name) : pipe_name_(pipe_name(name)) {}

Pipe::Pipe(int node_number, bool control_pipe)
    : Pipe(fmt::format("WWIV{}{}", node_number, control_pipe ? "C" : "")) {}
  
Pipe::Pipe(int node_number) : Pipe(node_number, false) {}


bool Pipe::Create() {
  handle_ = create_pipe(pipe_name_);
  return handle_ != Pipe::PIPE_INVALID_HANDLE_VALUE;
}

bool Pipe::Close() {
  auto ret = close_pipe(handle_, server_);
  handle_ = Pipe::PIPE_INVALID_HANDLE_VALUE;
  return ret;
}


Pipe::~Pipe() {
  if (!IsOpen()) {
    return;
  }
  Close();
}

bool Pipe::IsOpen() const noexcept { 
  return handle_ != Pipe::PIPE_INVALID_HANDLE_VALUE;
}

std::string Pipe::name() const noexcept { 
  return pipe_name_;
}


// Implemented in pipe_PLATFORM.cpp
// std::optional<int> Pipe::write(const char* data, int size);
// std::optional<int> Pipe::read(char* data, int size)
// bool Pipe::Open() {}
// bool WaitForClient(std::chrono::duration<double> timeout);


}
