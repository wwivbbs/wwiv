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

#include <string>
#include <string_view>

namespace wwiv::core {

class Pipe {
public:

// TODO(rushfan): These seem generally useful, maybe wwivport.h?
// and just call them native OS file handles?
#ifdef _WIN32
  typedef void* PIPE_HANDLE;
#elif defined(__OS2__)
  typedef HFILE PIPE_HANDLE;
#else
  typedef int PIPE_HANDLE;
#endif
  static constexpr PIPE_HANDLE PIPE_INVALID_HANDLE_VALUE = ((PIPE_HANDLE)-1);

  Pipe(const std::string_view name);
  ~Pipe();

private:
  std::string pipe_name_;

  PIPE_HANDLE handle_{PIPE_INVALID_HANDLE_VALUE};
};

// Platform specific bits
Pipe::PIPE_HANDLE create_pipe(const char* name);
bool connect_pipe(Pipe::PIPE_HANDLE h);
std::string pipe_name(const std::string_view part);
bool close_pipe(Pipe::PIPE_HANDLE h);

}

#endif  // INCLUDED_CORE_PIPE_H