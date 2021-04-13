/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include "common/null_remote_io.h"
#include "common/remote_io.h"
#include "local_io/local_io.h"

namespace wwiv::common {

std::optional<ScreenPos> NullRemoteIO::screen_position() { 
  if (!local_io_) {
    return std::nullopt;  
  }
  auto x = local_io_->WhereX() + 1;
  auto y = local_io_->WhereY() + 1;
  return {ScreenPos{x, y}};
}

} // namespace wwiv::comon
