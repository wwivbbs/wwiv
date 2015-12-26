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
#ifndef __INCLUDED_WWIVUTIL_COMMAND_H__
#define __INCLUDED_WWIVUTIL_COMMAND_H__

#include "core/command_line.h"
#include "sdk/config.h"
#include "sdk/networks.h"

namespace wwiv {
namespace wwivutil {

class Configuration {
public:
  explicit Configuration(wwiv::sdk::Config* config) : config_(config) {}
  ~Configuration() {}
  wwiv::sdk::Config* config() const { return config_; }
private:
  wwiv::sdk::Config* config_;
};

/** WWIVUTIL Command */
class UtilCommand: public wwiv::core::CommandLineCommand {
public:
  UtilCommand(const std::string& name, const std::string& description);
  virtual ~UtilCommand();
  // Override to add all commands.
  virtual bool AddSubCommands() = 0;

  Configuration* config() const { return config_; }
  bool set_config(Configuration* config);

private:
  Configuration* config_;
  std::vector<UtilCommand*> subcommands_;
};

// Calls AddStandardAtgs and AddSubCommands.
// Note: This must to called after you add it to the parent.
UtilCommand* AddCommandsAndArgs(UtilCommand* cmd);


}  // namespace wwivutil
}  // namespace wwiv


#endif  // __INCLUDED_WWIVUTIL_COMMAND_H__
