/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2018-2021, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_NET_CALLOUTS_H
#define INCLUDED_SDK_NET_CALLOUTS_H

#include "core/datetime.h"
#include "sdk/net/contact.h"
#include "sdk/net/net.h"

namespace wwiv::sdk::net {

bool allowed_to_call(const network_callout_config_t& con);
bool allowed_to_call(const net_call_out_rec& con, const wwiv::core::DateTime& dt);
bool should_call(const wwiv::sdk::NetworkContact& ncn, const network_callout_config_t& callout,
                 const wwiv::core::DateTime& dt);
bool should_call(const wwiv::sdk::NetworkContact& ncn, const net_call_out_rec& con,
                 const wwiv::core::DateTime& dt);

}

#endif
