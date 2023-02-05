/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2022, WWIV Software Services             */
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
#include "wwivutil/net/dump_connect.h"

#include "core/log.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/net/connect.h"
#include "sdk/net/networks.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::wwivutil {

std::string DumpConnectCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   connect" << std::endl;
  ss << "Example: connect" << std::endl;
  return ss.str();
}

int DumpConnectCommand::Execute() {
  const Networks networks(*config()->config());
  if (!networks.IsInitialized()) {
    LOG(ERROR) << "Unable to load networks.";
    return 1;
  }

  std::map<const std::string, Connect> connects;
  for (const auto& net : networks.networks()) {
    const auto lnn = ToStringLowerCase(net.name);
    connects.emplace(lnn, Connect(net.dir));
  }

  for (const auto& c : connects) {
    std::cout << "connect.net information: : " << c.first << std::endl;
    std::cout << "===========================================================" << std::endl;
    std::cout << c.second.ToString() << std::endl;
  }

  return 0;
}


}
