/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*              Copyright (C)2016 WWIV Software Services                  */
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
#ifndef __INCLUDED_NETWORK2_SUBS_H__
#define __INCLUDED_NETWORK2_SUBS_H__

#include <set>
#include "network2/context.h"
#include "sdk/networks.h"
#include "sdk/net.h"
#include "sdk/subxtr.h"
#include "sdk/usermanager.h"
#include "sdk/vardec.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/msgapi/message_api_wwiv.h"

namespace wwiv {
namespace net {
namespace network2 {

bool handle_sub_add_req(Context& context, const net_header_rec& nh, const std::string& text);
bool handle_sub_drop_req(Context& context, const net_header_rec& nh, const std::string& text);
bool handle_sub_add_drop_resp(Context& context, const net_header_rec& nh, const std::string& add_or_drop, const std::string& text);
bool handle_sub_list_info_request(Context& context, const net_header_rec& nh);
bool handle_sub_list_info_response(Context& context, const net_header_rec& nh, const std::string& text);

}  // namespace network2
}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORK2_SUBS_H__
