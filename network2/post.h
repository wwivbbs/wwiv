/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2022, WWIV Software Services               */
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
#include "sdk/net/packets.h"
#include <set>

namespace wwiv {
namespace net {
namespace network2 {

/** What handle_inbound_post did with a post. */
enum class inbound_post_t {
  /** The post couldn't be handled. */
  error,
  /** The post was saved; send it on to the other subscribers. */
  posted,
  /** The post was saved, held for the sysop to validate before it goes on. */
  held,
  /** The post wasn't saved (a duplicate, or written to dead.net); don't send it on. */
  not_posted,
};

/**
 * Handles receiving a NetPacket with a post and writing it to the
 * local database.
 */
inbound_post_t handle_inbound_post(Context& context, wwiv::sdk::net::NetPacket& packet);
/**
 * Handles a main_type_new_post packet: saves it here, then sends it on to the
 * other subscribers unless it wasn't saved or is held for validation.
 */
bool handle_new_post(Context& context, wwiv::sdk::net::NetPacket& packet);

/**
 * Send a network post out to the other subscribers when you are the host off
 * a sub or gating a sub.
 */
bool send_post_to_subscribers(Context& context, wwiv::sdk::net::NetPacket& packet,
                              const std::set<uint16_t>& subscribers_to_skip);

} // namespace network2
} // namespace net
} // namespace wwiv

#endif // __INCLUDED_NETWORK2_POST_H__
