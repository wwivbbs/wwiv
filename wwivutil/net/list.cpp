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
#include "wwivutil/net/list.h"

#include "core/log.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/net/packets.h"
#include "sdk/net/networks.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using std::map;
using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

namespace wwiv::wwivutil {

std::string NetListCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   list " << endl;
  return ss.str();
}

bool NetListCommand::AddSubCommands() {
  return true;
}

int NetListCommand::Execute() {
  const Networks networks(*config()->config());
  if (!networks.IsInitialized()) {
    LOG(ERROR) << "Unable to load networks.";
    return 1;
  }

  auto nn = 0;
  for (const auto& net : networks.networks()) {
    const auto s = fmt::format(".{:<2} {:<17}({})", nn++, net.name, net.dir);
    cout << s << std::endl;
  }
  return 0;
}

}
