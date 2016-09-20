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
#ifndef __INCLUDED_NETWORK2_CONTEXT_H__
#define __INCLUDED_NETWORK2_CONTEXT_H__

#include <vector>
#include "sdk/config.h"
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

class Context {
public:
  Context(
    const wwiv::sdk::Config& c,
    const net_networks_rec& n,
    wwiv::sdk::UserManager& u,
    wwiv::sdk::msgapi::WWIVMessageApi& a,
    const std::vector<net_networks_rec>& networks)
        : config(c), net(n), user_manager(u), api(a),
          subs(c.datadir(), networks)
  {
    subs_initialized = subs.Load();
  }

  const wwiv::sdk::Config& config;
  const net_networks_rec& net;
  wwiv::sdk::UserManager& user_manager;
  wwiv::sdk::msgapi::WWIVMessageApi& api;
  int network_number = 0;
  wwiv::sdk::Subs subs;
  bool verbose = false;
  bool subs_initialized = false;
};


}  // namespace network2
}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORK2_CONTEXT_H__
