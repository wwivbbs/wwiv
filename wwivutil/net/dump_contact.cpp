/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2020, WWIV Software Services             */
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
#include "wwivutil/net/dump_contact.h"

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/contact.h"
#include "sdk/networks.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>

using std::clog;
using std::cout;
using std::endl;
using std::map;
using std::string;
using wwiv::sdk::Contact;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::wwivutil {

std::string DumpContactCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   contact" << endl;
  ss << "Example: contact" << endl;
  return ss.str();
}

bool DumpContactCommand::AddSubCommands() {
  add_argument(BooleanCommandLineArgument{"save", "Save/cleanup contact.net in all subs", false});
  add_argument(BooleanCommandLineArgument{"backup", "make a backup of the contact.net files", false});
  return true;
}

int DumpContactCommand::Execute() {
  const Networks networks(*config()->config());
  if (!networks.IsInitialized()) {
    LOG(ERROR) << "Unable to load networks.";
    return 1;
  }

  map<const string, Contact> contacts;
  for (const auto& net : networks.networks()) {
    const auto lower_case_network_name = ToStringLowerCase(net.name);
    contacts.emplace(lower_case_network_name, Contact(net, false));
  }

  for (const auto& c : contacts) {
    cout << "contact.net information: : " << c.first << endl;
    cout << "===========================================================" << endl;
    cout << c.second.ToString() << endl;
  }

  if (barg("save")) {
    for (auto& c : contacts) {
      if (barg("backup")) {
        backup_file(c.second.path());
      }
      c.second.Save();
    }
  }

  return 0;
}

}
