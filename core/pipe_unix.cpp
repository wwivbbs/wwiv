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
  auto path = pipe_name(name);
  return -1;
}

bool connect_pipe(Pipe::PIPE_HANDLE h) {
  VLOG(2) << "Waiting for pipe to connect: [handle: " << h;
  return false;
}

std::string pipe_name(const std::string_view part) {
  return fmt::format("/tmp/wwiv-pipe-{}", part);
}

static bool close_pipe(Pipe::PIPE_HANDLE h) { 
  return close(h) >= 0;
}


}
