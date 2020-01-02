/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2020, WWIV Software Services               */
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

#include "sdk/config.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/net.h"
#include "sdk/networks.h"
#include "sdk/subxtr.h"
#include "sdk/usermanager.h"
#include "sdk/vardec.h"
#include <map>
#include <vector>

namespace wwiv {
namespace net {
namespace network2 {
  
/** 
 * Context for data needed by network processing.
 */
class Context {

public:
  Context(const wwiv::sdk::Config& c, const net_networks_rec& n, wwiv::sdk::UserManager& u,
          const std::vector<net_networks_rec>& ns)
      : config(c), net(n), user_manager(u), subs(c.datadir(), ns), networks_(ns) {
    subs_initialized = subs.Load();
  }

  void set_api(int type, std::unique_ptr<wwiv::sdk::msgapi::MessageApi>&& a) {
    msgapis_[type] = std::move(a);
  }

  void set_email_api(std::unique_ptr<wwiv::sdk::msgapi::WWIVMessageApi>&& a) { 
    email_api_ = std::move(a);
  }

  wwiv::sdk::msgapi::MessageApi& api(int type) { return *msgapis_.at(type).get(); }
  wwiv::sdk::msgapi::WWIVMessageApi& email_api() { return *email_api_.get(); }

  const std::vector<net_networks_rec>& networks() const noexcept { return networks_; }

  const wwiv::sdk::Config& config;
  const net_networks_rec& net;
  wwiv::sdk::UserManager& user_manager;
  std::map<int, std::unique_ptr<wwiv::sdk::msgapi::MessageApi>> msgapis_;
  // This is the type-2 entry in msgapis_. It's owned there.
  std::unique_ptr<wwiv::sdk::msgapi::WWIVMessageApi> email_api_;
  // network number like network 0 (.0) is the 1st network in wwivconfig.
  int network_number{0};
  wwiv::sdk::Subs subs;
  const std::vector<net_networks_rec> networks_;
  bool verbose{false};
  bool subs_initialized{false};
};

} // namespace network2
} // namespace net
} // namespace wwiv

#endif // __INCLUDED_NETWORK2_CONTEXT_H__
