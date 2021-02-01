/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*                  Copyright (C)2021, WWIV Software Services             */
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
#ifndef INCLUDED_WWIVUTIL_HELP_H
#define INCLUDED_WWIVUTIL_HELP_H

#include "wwivutil/command.h"

namespace wwiv::wwivutil {

class HelpCommand final : public UtilCommand {
public:
  HelpCommand();
  [[nodiscard]] std::string GetUsage() const override;
  bool AddSubCommands() override;
  int Execute() override;

private:
  bool ShowHelpForPath(std::vector<std::string> path, const core::CommandLineCommand* c);
  [[nodiscard]] std::string LongHelpForArg(const core::CommandLineArgument& c,
                                                     int max_len);

  [[nodiscard]] std::string HelpForCommand(const CommandLineCommand& uc,
                                           const std::string& command_base);
  [[nodiscard]] std::string FullHelpForCommand(const CommandLineCommand& uc,
                                           const std::string& cmdpath);
  [[nodiscard]] std::string ShortHelpForCommand(const core::CommandLineCommand& uc,
                                           const std::string& cmdpath);
};

}  // namespace


#endif
