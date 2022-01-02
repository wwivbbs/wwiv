/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2020-2022, WWIV Software Services             */
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
#ifndef INCLUDED_WWIVUTIL_ACS_ACS_H
#define INCLUDED_WWIVUTIL_ACS_ACS_H

#include "wwivutil/command.h"

namespace wwiv::wwivutil::acs {

class AcsCommand final : public UtilCommand {
public:
  AcsCommand();
  AcsCommand(const AcsCommand&) = delete;
  AcsCommand(AcsCommand&&) = delete;
  AcsCommand& operator=(const AcsCommand&) = delete;
  AcsCommand& operator=(AcsCommand&&) = delete;

  [[nodiscard]] std::string GetUsage() const override;
  ~AcsCommand() override = default;
  int Execute() override;
  bool AddSubCommands() override;
};


}  // namespace


#endif