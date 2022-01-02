/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
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
#ifndef INCLUDED_WWIVUTIL_COMMAND_H
#define INCLUDED_WWIVUTIL_COMMAND_H

#include "core/command_line.h"
#include "sdk/config.h"
#include "sdk/net/networks.h"

#include <memory>
#include <memory>
#include <string>

namespace wwiv::wwivutil {
class UtilCommand;

class Configuration final {
public:
  explicit Configuration(std::unique_ptr<sdk::Config>&& config)
      : config_(std::move(config)), networks_(*config_) {
    if (!networks_.IsInitialized()) {
      initialized_ = false;
    }
  }
  ~Configuration() = default;

  Configuration() = delete;
  Configuration(const Configuration&) = delete;
  Configuration(Configuration&&) = delete;
  Configuration& operator=(const Configuration&) = delete;
  Configuration& operator=(Configuration&&) = delete;

  [[nodiscard]] sdk::Config* config() const { return config_.get(); }
  [[nodiscard]] bool initialized() const { return initialized_; }
  [[nodiscard]] const sdk::Networks& networks() const { return networks_; }
  [[nodiscard]] const std::vector<UtilCommand*>& subcommands() const { return subcommands_; }
  void set_subcommands(std::vector<UtilCommand*>& scs) { subcommands_ = scs; }

private:
  const std::string bbsdir_;
  std::unique_ptr<sdk::Config> config_;
  sdk::Networks networks_;
  bool initialized_{true};
  std::vector<UtilCommand*> subcommands_;
};

/** WWIVutil Command */
class UtilCommand : public core::CommandLineCommand {
public:
  UtilCommand(const std::string& name, const std::string& description);
  ~UtilCommand() override;

  UtilCommand() = delete;
  UtilCommand(const UtilCommand&) = delete;
  UtilCommand(UtilCommand&&) = delete;
  UtilCommand& operator=(const UtilCommand&) = delete;
  UtilCommand& operator=(UtilCommand&&) = delete;

  // Override to add all commands.
  virtual bool AddSubCommands() = 0;
  bool add(std::shared_ptr<UtilCommand> cmd);

  [[nodiscard]] Configuration* config() const;
  bool set_config(const std::shared_ptr<Configuration>& config);

private:
  std::shared_ptr<Configuration> config_;
  std::vector<std::shared_ptr<UtilCommand>> subcommands_;
};

} // namespace

#endif
