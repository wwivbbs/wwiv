/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2021, WWIV Software Services             */
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
#include "sdk/net/networks.h"

#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;


namespace wwiv::sdk::net {

bool Network::try_load_nodelist() {
  if (nodelist && nodelist->initialized() && !nodelist->entries().empty()) {
    return true;
  }

  const auto nl_path = fido::Nodelist::FindLatestNodelist(dir, fido.nodelist_base);
  const auto domain = fido::domain_from_address(fido.fido_address);
  nodelist = std::make_shared<fido::Nodelist>(core::FilePath(dir, nl_path), domain);
  return nodelist->initialized();
}


} // namespace wwiv::sdk
