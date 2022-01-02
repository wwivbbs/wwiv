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
#ifndef __INCLUDED_NETWORK2_EMAIL_H__
#define __INCLUDED_NETWORK2_EMAIL_H__

#include "sdk/net/packets.h"
#include "network2/context.h"

namespace wwiv {
namespace net {
namespace network2 {

 // Expects the text in packet is of the form:
 // TITLE<nul>SENDER_NAME<cr/lf>DATE_STRING<cr/lf>MESSAGE_TEXT.
bool handle_email(Context& context, uint16_t to_user, wwiv::sdk::net::Packet& packet);

// Expects the text in packet is of the form:
// TO<nul>TITLE<nul>SENDER_NAME<cr/lf>DATE_STRING<cr/lf>MESSAGE_TEXT.
bool handle_email_byname(Context& context, wwiv::sdk::net::Packet& packet);

}  // namespace network2
}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORK2_EMAIL_H__
