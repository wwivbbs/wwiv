/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*               Copyright (C)2015, WWIV Software Services                */
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
#include <cctype>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/stl.h"
#include "netutil/dump.h"
#include "netutil/dump_callout.h"
#include "netutil/dump_contact.h"
#include "networkb/callout.h"
#include "networkb/connection.h"
#include "sdk/config.h"
#include "sdk/net.h"
#include "sdk/networks.h"

using std::clog;
using std::cout;
using std::endl;
using std::map;
using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::strings;
using wwiv::net::Callout;
using wwiv::sdk::Config;
using wwiv::net::Contact;
using wwiv::sdk::Networks;
using wwiv::stl::contains;

int main(int argc, char** argv) {
  CommandLine cmdline(argc, argv, "network_number");
  cmdline.add(CommandLineArgument("bbsdir", "Main BBS Directory containing CONFIG.DAT", File::current_directory()));
  cmdline.add_command("dump");
  cmdline.add_command("dump_callout");
  cmdline.add_command("dump_contact");

  if (!cmdline.Parse()) {
    clog << "Unable to parse command line." << endl;
    return 1;
  }

  if (argc <= 1 || !cmdline.subcommand_selected()) {
    cout << "usage: netutil [--bbsdir] <command> [<args>]" << endl;
    cout << "commands:" << endl;
    cout << "\tdump           Dumps contents of a network packet" << endl;
    cout << "\tdump_callout   Dumps parsed representation of CALLOUT.NET" << endl;
    cout << "\tdump_contact   Dumps parsed representation of CONTACT.NET" << endl;
    return 0;
  }

  string bbsdir = cmdline.arg("bbsdir").as_string();
  Config config(bbsdir);
  if (!config.IsInitialized()) {
    clog << "Unable to load CONFIG.DAT.";
    return 1;
  }
  Networks networks(config);
  if (!networks.IsInitialized()) {
    clog << "Unable to load networks.";
    return 1;
  }

  const string command = cmdline.command()->name();

  if (command == "dump") {
    return dump(cmdline.command());
  } else if (command == "dump_callout") {
    map<const string, Callout> callouts;
    for (const auto net : networks.networks()) {
      string lower_case_network_name(net.name);
      StringLowerCase(&lower_case_network_name);
      callouts.emplace(lower_case_network_name, Callout(net.dir));
    }
    return dump_callout(callouts, cmdline.command());
  } else if (command == "dump_contact") {
    map<const string, Contact> contacts;
    for (const auto net : networks.networks()) {
      string lower_case_network_name(net.name);
      StringLowerCase(&lower_case_network_name);
      contacts.emplace(lower_case_network_name, Contact(net.dir, false));
    }
    return dump_contact(contacts, cmdline.command());
  }
  cout << "Invalid command: \"" << command << "\"." << endl;
  return 1;
}
