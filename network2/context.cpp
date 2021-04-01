/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2021, WWIV Software Services             */
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
#include "network2/context.h"


namespace wwiv::net::network2 {

using namespace wwiv::sdk::net;

Context::Context(const sdk::Config& c, const Network& n, sdk::UserManager& u,
                 const std::vector<Network>& ns, NetDat& netdat)
  : config(c), net(n), user_manager(u), subs(c.datadir(), ns), networks_(ns), netdat_(netdat), ssm(c, u) {
  subs_initialized = subs.Load();
}

void Context::set_api(int type, std::unique_ptr<sdk::msgapi::MessageApi>&& a) {
  msgapis_[type] = std::move(a);
}

void Context::set_email_api(std::unique_ptr<sdk::msgapi::WWIVMessageApi>&& a) {
  email_api_ = std::move(a);
}

sdk::msgapi::MessageApi& Context::api(int type) {
  if (type == 0) { type = 2; }
  return *stl::at(msgapis_, type);
}

sdk::msgapi::WWIVMessageApi& Context::email_api() const { return *email_api_; }


}
