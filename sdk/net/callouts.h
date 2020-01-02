/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2018-2020, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_NET_CALLOUTS_H__
#define __INCLUDED_SDK_NET_CALLOUTS_H__

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/datetime.h"
#include "sdk/contact.h"
#include "sdk/net.h"
#include "sdk/networks.h"

namespace wwiv {
namespace sdk {
namespace net {

bool allowed_to_call(const network_callout_config_t& con, const wwiv::core::DateTime& dt);
bool allowed_to_call(const net_call_out_rec& con, const wwiv::core::DateTime& dt);
bool should_call(const wwiv::sdk::NetworkContact& ncn, const network_callout_config_t& callout,
                 const wwiv::core::DateTime& dt);
bool should_call(const wwiv::sdk::NetworkContact& ncn, const net_call_out_rec& con,
                 const wwiv::core::DateTime& dt);

} // namespace net
} // namespace sdk
} // namespace wwiv

#endif // __INCLUDED_SDK_NET_CALLOUTS_H__
