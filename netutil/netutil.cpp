/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
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
using namespace wwiv::net;
using namespace wwiv::sdk;

int main(int argc, char** argv) {
  try {
    CommandLine cmdline(argc, argv, "network_number");
    cmdline.add(BooleanCommandLineArgument("help", '?', "Displays Help", false));
    cmdline.add({"bbsdir", "Main BBS Directory containing CONFIG.DAT", File::current_directory()});

    class DumpCommand final: public CommandLineCommand {
    public:
      DumpCommand(): CommandLineCommand("dump", "Dumps contents of a network packet") {}
      virtual int Execute() override final {
        return dump(this);
      }
    };
    class DumpCalloutCommand final: public CommandLineCommand {
    public:
      DumpCalloutCommand()
        : CommandLineCommand("dump_callout", "Dumps parsed representation of CALLOUT.NET") {}
      virtual int Execute() override final {

        Config config(bbsdir_);
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
        return dump_callout(callouts, this);
      }
      void set_bbsdir(const std::string& bbsdir) { bbsdir_ = bbsdir; }
    private:
      std::string bbsdir_;
    };
    class DumpContactCommand final: public CommandLineCommand {
    public:
      DumpContactCommand()
          : CommandLineCommand("dump_contact", "Dumps parsed representation of CONTACT.NET") {}
      virtual int Execute() override final {

        Config config(bbsdir_);
        if (!config.IsInitialized()) {
          clog << "Unable to load CONFIG.DAT.";
          return 1;
        }
        Networks networks(config);
        if (!networks.IsInitialized()) {
          clog << "Unable to load networks.";
          return 1;
        }

        map<const string, Contact> contacts;
        for (const auto net : networks.networks()) {
          string lower_case_network_name(net.name);
          StringLowerCase(&lower_case_network_name);
          contacts.emplace(lower_case_network_name, Contact(net.dir, false));
        }
        return dump_contact(contacts, this);
      }
      void set_bbsdir(const std::string& bbsdir) { bbsdir_ = bbsdir; }
    private:
      std::string bbsdir_;
    };

    try {
      cmdline.add(new DumpCommand());
      DumpContactCommand* dump_contact = new DumpContactCommand();
      cmdline.add(dump_contact);
      DumpCalloutCommand* dump_callout = new DumpCalloutCommand();
      cmdline.add(dump_callout);

      int parse_result = cmdline.Parse();
      if (parse_result != 0) {
        return parse_result;
      }
      const string bbsdir = cmdline.arg("bbsdir").as_string();
      dump_contact->set_bbsdir(bbsdir);
      dump_callout->set_bbsdir(bbsdir);

      return cmdline.Execute();
    } catch (const unknown_argument_error& e) {
      clog << "Unable to parse command line." << endl;
      clog << e.what() << endl;
    }
  } catch (const std::exception& e) {
    clog << e.what() << endl;
  }
  return 1;
}
