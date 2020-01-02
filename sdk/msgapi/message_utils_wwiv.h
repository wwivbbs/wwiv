/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2015-2020, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_MESSAGE_UTILS_WWIV_H__
#define __INCLUDED_SDK_MESSAGE_UTILS_WWIV_H__

#include <cstdint>
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

template<typename M>
uint8_t network_number_from(const M* m) {
  if (!(m->status & status_new_net)) {
    // Some bits in wwiv, like forwarded mail will not have the network
    // status set on the original message.  It's also not clear if this
    // is right, however returning 255 here (no net found), seems to
    // break things.
    return 0;
  }
  if (m->status & status_source_verified) {
    return m->network.src_verified_msg.net_number;
  }
  return m->network.network_msg.net_number;
}

template<typename M>
uint16_t source_verfied_type(const M* m) {
  if ((m->status & status_new_net) && (m->status & status_source_verified)) {
    return m->network.src_verified_msg.source_verified_type;
  }
  return 0;
}


}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv


#endif  // __INCLUDED_SDK_MESSAGE_UTILS_WWIV_H__
