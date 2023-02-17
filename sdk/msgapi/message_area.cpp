/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2022, WWIV Software Services             */
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
/**************************************************************************/
#include "sdk/msgapi/message_area_wwiv.h"

#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/msgapi/message_api.h"

#include <string>

namespace wwiv::sdk::msgapi {

[[nodiscard]] uint32_t MessageAreaLastRead::last_read(int user_number) {
  if (message_area_number_ < 0) {
    return 0;
  }
  return api_->last_read(message_area_number_);
}

bool MessageAreaLastRead::set_last_read(int user_number, uint32_t last_read, uint32_t highest_read) {
  if (message_area_number_ >= 0) {
    api_->set_last_read(message_area_number_, std::max<uint32_t>(last_read, highest_read));
  }
  return true;
}

bool MessageAreaLastRead::Close() { return true; }

} // namespace wwiv::sdk::msgapi
