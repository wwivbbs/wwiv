/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2017, WWIV Software Services               */
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
#ifndef __INCLUDED_NETWORK2_POST_H__
#define __INCLUDED_NETWORK2_POST_H__

#include "network2/context.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/net.h"
#include "sdk/net/packets.h"
#include "sdk/networks.h"
#include "sdk/subxtr.h"
#include "sdk/usermanager.h"
#include "sdk/vardec.h"
#include "sdk/net/packets.h"
#include <set>
#include <vector>

namespace wwiv {
namespace net {
namespace network2 {

/**
 * Handles receiving a Packet with a post and writing it to the
 * local database.
 */
bool handle_inbound_post(Context& context, wwiv::sdk::net::Packet& packet);
/**
 * Send a network post out to the other subscribers when you are the host off
 * a sub or gating a sub.
 */
bool send_post_to_subscribers(Context& context, wwiv::sdk::net::Packet& packet,
     std::set<uint16_t> subscribers_to_skip);

} // namespace network2
} // namespace net
} // namespace wwiv

#endif // __INCLUDED_NETWORK2_POST_H__
