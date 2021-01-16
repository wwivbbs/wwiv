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
#include "wwivutil/instance/instance.h"

#include "sdk/instance.h"
#include "core/command_line.h"
#include "core/datetime.h"
#include "core/stl.h"
#include "core/strings.h"
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

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

namespace wwiv::wwivutil {

/* Instance status flags */
constexpr int INST_FLAGS_NONE = 0x0000;  // No flags at all
constexpr int INST_FLAGS_ONLINE = 0x0001;  // User online
constexpr int INST_FLAGS_MSG_AVAIL = 0x0002;  // Available for inst msgs
constexpr int INST_FLAGS_INVIS = 0x0004;  // For invisibility


class InstanceDumpCommand : public UtilCommand {
public:
  InstanceDumpCommand(): UtilCommand("dump", "Displays WWIV instance information.") {}

  [[nodiscard]] std::string GetUsage() const override final {
    std::ostringstream ss;
    ss << "Usage: " << std::endl << std::endl;
    ss << "  dump : Displays instance information." << std::endl << std::endl;
    return ss.str();
  }

  [[nodiscard]] std::string flags_to_string(uint16_t flags) const {
    std::ostringstream ss;
    if (flags & INST_FLAGS_ONLINE) {
      ss << "[online] ";
    }
    if (flags & INST_FLAGS_MSG_AVAIL) {
      ss << "[msg avail] ";
    }
    if (flags & INST_FLAGS_INVIS) {
      ss << "[invisible] ";
    }
    return ss.str();
  }

  int Execute() override final {
    Instances instances(*config()->config());
    if (!instances) {
      std::cout << "Unable to read Instance information.";
      return 1;
    }
    const auto num = instances.size();
    std::cout << "num instances:  " << num << std::endl;
    for (auto i = 1; i <= num; ++i) {
      const auto instance = instances.at(i);
      std::cout << "=======================================================================" << std::endl;
      std::cout << "Instance    : #" << instance.node_number() << std::endl;
      std::cout << "User        : #" << instance.user_number() << std::endl;
      std::cout << "Location    : " << instance.location_description() << std::endl;
      std::cout << "SubLoc      : " << instance.subloc_code() << std::endl;
      std::cout << "Flags       : " << flags_to_string(instance.ir().flags) << std::endl;
      std::cout << "Modem Speed : " << instance.modem_speed() << std::endl;
      std::cout << "Started     : " << instance.started().to_string() << std::endl;
      std::cout << "Updated     : " << instance.updated().to_string() << std::endl;
    }
    std::cout << "=======================================================================" << std::endl;
    return 0;
  }

  bool AddSubCommands() override final {
    return true;
  }

};

bool InstanceCommand::AddSubCommands() {
  add(make_unique<InstanceDumpCommand>());
  return true;
}


}  // namespace wwivutil::wwiv
