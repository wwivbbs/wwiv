/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*                Copyright (C)2015 WWIV Software Services                */
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
#include "wwivutil/dump_callout.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "core/strings.h"
#include "sdk/callout.h"
#include "sdk/config.h"

using std::clog;
using std::cout;
using std::endl;
using std::map;
using std::string;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv {
namespace wwivutil {

static void dump_callout_usage() {
  cout << "Usage:   dump_callout" << endl;
  cout << "Example: dump_callout" << endl;
}

int DumpCalloutCommand::Execute() {

  Config config(config()->bbsdir());
  if (!config.IsInitialized()) {
    clog << "Unable to load CONFIG.DAT.";
    return 1;
  }
  Networks networks(config);
  if (!networks.IsInitialized()) {
    clog << "Unable to load networks.";
    return 1;
  }

  map<const string, Callout> callouts;
  for (const auto net : networks.networks()) {
    string lower_case_network_name(net.name);
    StringLowerCase(&lower_case_network_name);
    callouts.emplace(lower_case_network_name, Callout(net.dir));
  }

  for (const auto& c : callouts) {
    cout << "CALLOUT.NET information: : " << c.first << endl;
    cout << "===========================================================" << endl;
    cout << c.second.ToString() << endl;
  }

  return 0;
}


}
}
