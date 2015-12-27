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
#include "netutil/dump_callout.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "core/strings.h"
#include "networkb/callout.h"

using std::cout;
using std::endl;
using std::map;
using std::string;
using wwiv::net::Callout;
using namespace wwiv::strings;

void dump_callout_usage() {
  cout << "Usage:   dump_callout" << endl;
  cout << "Example: dump_callout" << endl;
}

int dump_callout(map<const string, Callout> callouts,
  const wwiv::core::CommandLineCommand* command) {
  for (const auto& c : callouts) {
    cout << "CALLOUT.NET information: : " << c.first << endl;
    cout << "===========================================================" << endl;
    cout << c.second.ToString() << endl;
  }

  return 0;
}
