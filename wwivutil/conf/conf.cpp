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
#include "sdk/conf/conf.h"
#include "wwivutil/conf/conf.h"

#include "core/command_line.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "wwivutil/util.h"

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

constexpr char CD = 4;

namespace wwiv::wwivutil {

static void DisplayConferences(const std::set<conference_t>& c5, char key = 0) {
  for (const auto& item : c5) {
    if (key == '\0' || item.key == key) {
      std::cout << item.key << ") '" << item.conf_name << "' " << std::endl;
      std::cout << "  [ACS: " << item.acs << "]" << std::endl;
    }
  }
}

class ConfDumpCommand final : public UtilCommand {
public:
  ConfDumpCommand() : UtilCommand("dump", "Displays the info about a conference") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   dump --conf_type=[subs|dirs] [conf key]" << std::endl;
    ss << "Example: dump --conf_type=subs A" << std::endl << std::endl;
    ss << "         Note that an empty conference key dumps all converences" << std::endl;
    return ss.str();
  }

  int Execute() override {
    char key = 0;
    if (!remaining().empty()) {
      key = to_upper_case_char(remaining().front().front());
    }

    const auto datadir = config()->config()->datadir();
    Subs subs(datadir, config()->networks().networks());
    files::Dirs dirs(datadir, 0);
    if (!subs.Load()) {
      std::cout << "Unable to load subs.json. Aborting." << std::endl;
      return 3;
    }
    if (!dirs.Load()) {
      std::cout << "Unable to load subs.json. Aborting." << std::endl;
      return 3;
    }

    const Conferences confs(datadir, subs, dirs, 0);

    const auto kind_subs = iequals(sarg("conf_type"), "subs");

    auto& c = kind_subs ? confs.subs_conf() : confs.dirs_conf();

    DisplayConferences(c.confs(), key);

    return 0;
  }

  bool AddSubCommands() override {
    add_argument(BooleanCommandLineArgument("full", "Display full info about every command.", false));
    add_argument({"conf_type", "The conference type ('subs' or 'dirs')", "subs"});
    return true;
  }
};

class ConfConvertCommand final : public UtilCommand {
public:
  ConfConvertCommand() : UtilCommand("convert", "Converts 4.3x conferences to 5.6 format") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage:   convert " << std::endl;
    return ss.str();
  }

  int Execute() override {
    const auto datadir = config()->config()->datadir();
    Subs subs(datadir, config()->networks().networks());
    files::Dirs dirs(datadir, 0);
    if (!subs.Load()) {
      std::cout << "Unable to load subs.json. Aborting." << std::endl;
      return 3;
    }
    if (!dirs.Load()) {
      std::cout << "Unable to load subs.json. Aborting." << std::endl;
      return 3;
    }
    Conferences confs(datadir, subs, dirs, 0);

    const auto f = UpgradeConferences(*config()->config(), subs, dirs);
    if (!confs.LoadFromFile(f)) {
      std::cout << "Failed to upgrade conferences";
    }

    std::cout << "Subs: " << std::endl;
    DisplayConferences(confs.subs_conf().confs());
    std::cout << "Dirs: " << std::endl;
    DisplayConferences(confs.dirs_conf().confs());
    std::cout << std::endl << std::endl;

    // Save confs to new version.
    if (confs.Save()) {
      if (!subs.Save()) {
        LOG(ERROR) << "Saved new conference, but failed to save subs";
      }
      if (!dirs.Save()) {
        LOG(ERROR) << "Saved new conference, but failed to save dirs";
      }
    }

    return 0;
  }

  bool AddSubCommands() override final {
    add_argument({"menu_set", "The menuset to use", "wwiv"});
    add_argument({"conf_type", "The conference type ('subs' or 'dirs')", "subs"});
    return true;
  }
};

bool ConfCommand::AddSubCommands() {
  if (!add(std::make_unique<ConfDumpCommand>())) {
    return false;
  }
  if (!add(std::make_unique<ConfConvertCommand>())) {
    return false;
  }

  return true;
}

} // namespace wwiv
