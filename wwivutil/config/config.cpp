/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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
#include "wwivutil/config/config.h"

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/file.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/net.h"
#include "sdk/networks.h"

#include "wwivutil/net/dump_bbsdata.h"
#include "wwivutil/net/dump_callout.h"
#include "wwivutil/net/dump_connect.h"
#include "wwivutil/net/dump_contact.h"
#include "wwivutil/net/dump_packet.h"

using std::cerr;
using std::cout;
using std::endl;
using std::make_unique;
using std::setw;
using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::core::BooleanCommandLineArgument;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv {
namespace wwivutil {

static int show_version(const Config& config) {
  cout << "5.2 Versioned Config    : " << std::boolalpha << config.versioned_config_dat() << std::endl;
  cout << "Written By WWIV Version : " << config.written_by_wwiv_num_version() << std::endl;
  cout << "Config Revision #       : " << config.config_revision_number() << std::endl;

  return 0;
}

class ConfigVersionCommand : public UtilCommand {
public:
  ConfigVersionCommand(): UtilCommand("version", "Sets or Gets the config version") {}
  std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage: " << std::endl << std::endl;
    ss << "  get : Gets the config.dat version information." << std::endl << std::endl;
    return ss.str();
  }
  int Execute() override final {
    if (remaining().empty()) {
      std::cout << GetUsage() << GetHelp() << endl;
      return 2;
    }
    string set_or_get(remaining().front());
    StringLowerCase(&set_or_get);

    if (set_or_get == "get") {
      return show_version(*this->config()->config());
    }
    return 1;
  }
  bool AddSubCommands() override final {
    return true;
  }

};

bool ConfigCommand::AddSubCommands() {
  add(make_unique<ConfigVersionCommand>());
  return true;
}


}  // namespace wwivutil
}  // namespace wwiv
