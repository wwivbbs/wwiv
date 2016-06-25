/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2016 WWIV Software Services              */
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

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/bbslist.h"
#include "sdk/config.h"
#include "sdk/callout.h"
#include "sdk/config.h"
#include "sdk/networks.h"

using std::cout;
using std::endl;
using std::map;
using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv {
namespace wwivutil {

std::string DumpBbsDataCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   dump_bbsdata" << endl;
  ss << "Example: dump_bbsdata" << endl;
  return ss.str();
}

bool DumpBbsDataCommand::AddSubCommands() {
  add_argument(BooleanCommandLineArgument("bbslist", 'l', "Parse BBSList.NET vs. BBSDATA.NET", false));

  return true;
}

int DumpBbsDataCommand::Execute() {
  Networks networks(*config()->config());
  if (!networks.IsInitialized()) {
    LOG << "Unable to load networks.";
    return 1;
  }

  map<const string, BbsListNet> bbslists;
  for (const auto net : networks.networks()) {
    string lower_case_network_name(net.name);
    StringLowerCase(&lower_case_network_name);
    if (arg("bbslist").as_bool()) {
      LOG << "Parsing BBSLIST.NET";
      bbslists.emplace(lower_case_network_name, BbsListNet::ParseBbsListNet(net.dir));
    } else {
      LOG << "Reading BBSDATA.NET";
      bbslists.emplace(lower_case_network_name, BbsListNet::ReadBbsDataNet(net.dir));
    }
  }

  for (const auto& b : bbslists) {
    cout << "BBSDATA.NET information: : " << b.first << endl;
    cout << "===========================================================" << endl;
    cout << b.second.ToString() << endl;
  }

  return 0;
}


}
}
