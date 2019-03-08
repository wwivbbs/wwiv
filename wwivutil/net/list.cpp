/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2018-2019, WWIV Software Services             */
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

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/log.h"
#include "core/strings.h"
#include "networkb/net_util.h"
#include "sdk/net/packets.h"
#include "sdk/bbslist.h"
#include "sdk/config.h"
#include "sdk/callout.h"
#include "sdk/config.h"
#include "core/datetime.h"
#include "sdk/networks.h"

using std::cout;
using std::endl;
using std::map;
using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

namespace wwiv {
namespace wwivutil {

std::string NetListCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   list " << endl;
  return ss.str();
}

bool NetListCommand::AddSubCommands() {
  return true;
}

int NetListCommand::Execute() {
  Networks networks(*config()->config());
  if (!networks.IsInitialized()) {
    LOG(ERROR) << "Unable to load networks.";
    return 1;
  }

  int nn = 0;
  for (const auto& net : networks.networks()) {
    cout << "." << pad_to(std::to_string(nn++), 2) << " " << pad_to(net.name, 17) << "(" << net.dir
         << ")" << std::endl;
  }
  return 0;
}

}
}
