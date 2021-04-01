/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2021, WWIV Software Services               */
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
#ifndef INCLUDED_NETWORK2_CONTEXT_H
#define INCLUDED_NETWORK2_CONTEXT_H

#include "net_core/netdat.h"
#include "sdk/config.h"
#include "sdk/ssm.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/net/net.h"
#include "sdk/subxtr.h"
#include "sdk/usermanager.h"
#include <map>
#include <vector>

namespace wwiv::net::network2 {
  
/** 
 * Context for data needed by network processing.
 */
class Context {

public:
  Context(const sdk::Config& c, const sdk::net::Network& n, sdk::UserManager& u,
          const std::vector<sdk::net::Network>& ns, NetDat& netdat);

  void set_api(int type, std::unique_ptr<sdk::msgapi::MessageApi>&& a);

  void set_email_api(std::unique_ptr<sdk::msgapi::WWIVMessageApi>&& a);

  [[nodiscard]] sdk::msgapi::MessageApi& api(int type);
  [[nodiscard]] sdk::msgapi::WWIVMessageApi& email_api() const;

  [[nodiscard]] const std::vector<sdk::net::Network>& networks() const noexcept { return networks_; }
  [[nodiscard]] NetDat& netdat() const { return netdat_; }

  const sdk::Config& config;
  const sdk::net::Network& net;
  sdk::UserManager& user_manager;
  std::map<int, std::unique_ptr<sdk::msgapi::MessageApi>> msgapis_;
  // This is the type-2 entry in msgapis_. It's owned there.
  std::unique_ptr<sdk::msgapi::WWIVMessageApi> email_api_;
  // network number like network 0 (.0) is the 1st network in WWIVconfig.
  int network_number{0};
  sdk::Subs subs;
  const std::vector<sdk::net::Network> networks_;
  NetDat& netdat_;
  bool verbose{false};
  bool subs_initialized{false};
  sdk::SSM ssm;
};

} // namespace wwiv::net::network2

#endif // INCLUDED_NETWORK2_CONTEXT_H
