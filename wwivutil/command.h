/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
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
#ifndef __INCLUDED_WWIVUTIL_COMMAND_H__
#define __INCLUDED_WWIVUTIL_COMMAND_H__

#include <memory>
#include <string>

#include "core/command_line.h"
#include "sdk/config.h"
#include "sdk/networks.h"

namespace wwiv {
namespace wwivutil {

class Configuration {
public:
  Configuration(const std::string bbsdir, wwiv::sdk::Config* config) 
      : bbsdir_(bbsdir), config_(config), networks_(*config) {
    if (!networks_.IsInitialized()) {
      initialized_ = false;
    }


  }
  ~Configuration() {}

  const wwiv::sdk::Config* config() const { return config_; }
  const std::string bbsdir() const { return bbsdir_;  }
  bool initialized() const { return initialized_; }
  wwiv::sdk::Networks networks() const { return networks_; }

private:
  const std::string bbsdir_;
  wwiv::sdk::Config* config_;
  wwiv::sdk::Networks networks_;
  bool initialized_ = true;
};

/** WWIVUTIL Command */
class UtilCommand: public wwiv::core::CommandLineCommand {
public:
  UtilCommand(const std::string& name, const std::string& description);
  virtual ~UtilCommand();
  // Override to add all commands.
  virtual bool AddSubCommands() = 0;
  bool add(std::unique_ptr<CommandLineCommand> cmd) override;

  Configuration* config() const { return config_; }
  bool set_config(Configuration* config);

private:
  Configuration* config_;
  std::vector<UtilCommand*> subcommands_;
};

}  // namespace wwivutil
}  // namespace wwiv


#endif  // __INCLUDED_WWIVUTIL_COMMAND_H__
