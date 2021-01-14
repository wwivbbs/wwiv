/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#include "wwivutil/net/dump_bbsdata.h"

#include "core/command_line.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/bbslist.h"
#include "sdk/config.h"
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
using namespace wwiv::strings;

namespace wwiv::wwivutil {

std::string DumpBbsDataCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   dump_bbsdata" << endl;
  ss << "Example: dump_bbsdata" << endl;
  return ss.str();
}

bool DumpBbsDataCommand::AddSubCommands() {
  add_argument(BooleanCommandLineArgument("bbslist", 'l', "Parse bbslist.net vs. bbsdata.net", false));

  return true;
}

int DumpBbsDataCommand::Execute() {
  const Networks networks(*config()->config());
  if (!networks.IsInitialized()) {
    LOG(ERROR) << "Unable to load networks.";
    return 1;
  }

  map<const string, BbsListNet> bbslists;
  for (const auto& net : networks.networks()) {
    string lower_case_network_name(net.name);
    StringLowerCase(&lower_case_network_name);
    if (barg("bbslist")) {
      LOG(INFO) << "Parsing bbslist.net";
      bbslists.emplace(lower_case_network_name, BbsListNet::ParseBbsListNet(net.sysnum, net.dir));
    } else {
      LOG(INFO) << "Reading bbsdata.net";
      bbslists.emplace(lower_case_network_name, BbsListNet::ReadBbsDataNet(net.dir));
    }
  }

  for (const auto& [net_name, b] : bbslists) {
    cout << "bbsdata.net information: : " << net_name << endl;
    cout << "===========================================================" << endl;
    cout << b.ToString() << endl;
  }

  return 0;
}


}
