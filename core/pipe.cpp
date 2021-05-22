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

#ifdef __OS2__

#elif defined(_WIN32)



#elif defined(__unix__)

#else

#error "No OS defined for this type."

#endif

Pipe::Pipe(const std::string_view name) : pipe_name_(pipe_name(name)) {

}

Pipe::~Pipe() {
  close_pipe(handle_);
  handle_ = INVALID_HANDLE_VALUE;
}

}
