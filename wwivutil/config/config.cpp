/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2022, WWIV Software Services             */
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

#include "sdk/config.h"
#include "core/command_line.h"
#include "core/datafile.h"
#include "core/strings.h"
#include "sdk/config430.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

using wwiv::core::BooleanCommandLineArgument;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::wwivutil {

static int show_version(const Config& config) {
  std::cout << "5.2 Versioned Config    : " << std::boolalpha << config.versioned_config_dat()
       << std::endl;
  std::cout << "Written By WWIV Version : " << config.written_by_wwiv_num_version() << std::endl;
  std::cout << "Config Revision #       : " << config.config_revision_number() << std::endl;

  return 0;
}

static bool set_version(Config& config, uint16_t wwiv_ver, uint32_t revision) {
  if (!config.versioned_config_dat()) {
    std::cout << "Can only set the wwiv_version and config revision on a 5.1 or higher versioned "
            "config.dat"
         << std::endl;
    return false;
  }

  auto& h = config.header();
  if (wwiv_ver >= 500) {
    std::cout << "setting wwiv_ver to " << wwiv_ver << std::endl;
    h.written_by_wwiv_num_version = wwiv_ver;
  }
  if (revision > 0) {
    std::cout << "setting revision to " << revision << std::endl;
    h.config_revision_number = revision;
  }

  if (wwiv_ver >= 500 || revision > 0) {
    std::cout << "Wrote Config.dat" << std::endl;
    config.Save();
  }
  std::cout << "Nothing changed; Nothing to do." << std::endl << std::endl;
  return false;
}

class ConfigVersionCommand final : public UtilCommand {
public:
  ConfigVersionCommand() : UtilCommand("version", "Sets or Gets the config version") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage: " << std::endl << std::endl;
    ss << "  get : Gets the config.dat version information." << std::endl;
    ss << "  set : Sets the config.dat version information." << std::endl << std::endl;
    return ss.str();
  }
  int Execute() override {
    if (remaining().empty()) {
      std::cout << GetUsage() << GetHelp() << std::endl;
      return 2;
    }
    const auto set_or_get = ToStringLowerCase(remaining().front());

    if (set_or_get == "get") {
      return show_version(*this->config()->config());
    }
    if (set_or_get == "set") {
      if (!set_version(*this->config()->config(), static_cast<uint16_t>(iarg("wwiv_version")),
                       iarg("revision"))) {
        std::cout << GetUsage() << GetHelp() << std::endl;
        return 1;
      }
      return 0;
    }
    return 1;
  }

  bool AddSubCommands() override {
    add_argument({"wwiv_version", "WWIV Version that created this config.dat", ""});
    add_argument({"revision", "Configuration revision number", ""});
    return true;
  }
};

class ConfigConvertCommand final : public UtilCommand {
public:
  ConfigConvertCommand() : UtilCommand("convert", "converts the config to either JSON or DAT format") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage: " << std::endl << std::endl;
    ss << "  json : Writes a config.json." << std::endl;
    ss << "  dat : Writes a config.dat." << std::endl << std::endl;
    return ss.str();
  }
  int Execute() override {
    if (remaining().empty()) {
      std::cout << GetUsage() << GetHelp() << std::endl;
      return 2;
    }
    const auto out_format = ToStringLowerCase(remaining().front());

    if (out_format == "dat") {
      Config430 c430(config()->config()->to_config_t());
      c430.Save();
      return show_version(*this->config()->config());
    }
    if (out_format == "json") {
      if (!set_version(*this->config()->config(), static_cast<uint16_t>(iarg("wwiv_version")),
                       iarg("revision"))) {
        std::cout << GetUsage() << GetHelp() << std::endl;
        return 1;
      }
      return 0;
    }
    return 1;
  }

  bool AddSubCommands() override {
    add_argument({"wwiv_version", "WWIV Version that created this config.dat", ""});
    add_argument({"revision", "Configuration revision number", ""});
    return true;
  }
};

bool ConfigCommand::AddSubCommands() {
  add(std::make_unique<ConfigVersionCommand>());
  add(std::make_unique<ConfigConvertCommand>());
  return true;
}

} // namespace