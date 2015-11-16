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
using namespace wwiv::strings;
using wwiv::net::Callout;
using wwiv::sdk::Config;
using wwiv::net::Contact;
using wwiv::sdk::Networks;
using wwiv::stl::contains;

int main(int argc, char** argv) {
  map<string, string> args;
  int i = 1;
  string command;
  for (; i < argc; i++) {
    const string s(argv[i]);
    if (starts_with(s, "--")) {
      const vector<string> delims = SplitString(s, "=");
      const string value = (delims.size() > 1) ? delims[1] : "";
      args.emplace(delims[0].substr(2), value);
    }
    else if (starts_with(s, "/")) {
      char letter = std::toupper(s[1]);
      if (letter == '?') {
        args.emplace("help", "");
      }
      else {
        const string key(1, letter);
        const string value = s.substr(2);
        args.emplace(key, value);
      }
    }
    else if (starts_with(s, ".")) {
      const string key = "network_number";
      const string value = s.substr(1);
      args.emplace(key, value);
    } else {
      command = s;
      break;
    }
  }
    
  if (argc <= 1) {
    cout << "usage: netutil [--bbsdir] <command> [<args>]" << endl;
    cout << "commands are:" << endl;
    cout << "\tdump           Dumps contents of a network packet" << endl;
    cout << "\tdump_callout   Dumps parsed representation of CALLOUT.NET" << endl;
    cout << "\tdump_contact   Dumps parsed representation of CONTACT.NET" << endl;
    return 0;
  }
  argc -= i;
  argv += i;

  string bbsdir = File::current_directory();
  if (contains(args, "bbsdir")) {
    bbsdir = args["bbsdir"];
  }
  Config config(bbsdir);
  if (!config.IsInitialized()) {
    clog << "Unable to load config.dat.";
    return 1;
  }
  Networks networks(config);
  if (!networks.IsInitialized()) {
    clog << "Unable to load networks.";
    return 1;
  }

  if (command == "dump") {
    return dump(argc, argv);
  } else if (command == "dump_callout") {
    map<const string, Callout> callouts;
    for (const auto net : networks.networks()) {
      string lower_case_network_name(net.name);
      StringLowerCase(&lower_case_network_name);
      callouts.emplace(lower_case_network_name, Callout(net.dir));
    }
    return dump_callout(callouts, argc, argv);
  } else if (command == "dump_contact") {
    map<const string, Contact> contacts;
    for (const auto net : networks.networks()) {
      string lower_case_network_name(net.name);
      StringLowerCase(&lower_case_network_name);
      contacts.emplace(lower_case_network_name, Contact(net.dir, false));
    }
    return dump_contact(contacts, argc, argv);
  }
  cout << "Invalid command: \"" << command << "\"." << endl;
  return 1;
}
