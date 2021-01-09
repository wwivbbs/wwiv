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
#ifndef INCLUDED_WWIVUTIL_EMAIL_EMAIL_H
#define INCLUDED_WWIVUTIL_EMAIL_EMAIL_H

#include "sdk/subxtr.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/msgapi/email_wwiv.h"
#include "wwivutil/command.h"
#include <memory>

namespace wwiv::wwivutil {

class BaseEmailSubCommand : public UtilCommand {
public:
  BaseEmailSubCommand(const std::string& name, const std::string& descr)
      : UtilCommand(name, descr) {}
  ~BaseEmailSubCommand() override = default;
  bool CreateMessageApi();
  // N.B. if this doesn't exist this message will crash.
  sdk::msgapi::WWIVMessageApi& api() noexcept;

protected:
  std::unique_ptr<sdk::msgapi::WWIVMessageApi> api_;

private:
  std::string basename_;
};

class EmailCommand final : public UtilCommand {
public:
  EmailCommand(): UtilCommand("email", "WWIV email commands.") {}
  ~EmailCommand() override = default;
  bool AddSubCommands() override;
};

class EmailDumpCommand final: public BaseEmailSubCommand {
public:
  EmailDumpCommand();
  ~EmailDumpCommand() override = default;
  [[nodiscard]] std::string GetUsage() const override;
  int Execute() override;
  bool AddSubCommands() override;

protected:
  int ExecuteImpl(sdk::msgapi::WWIVEmail* area, int start, int end, bool all);
};

}  // namespace


#endif
