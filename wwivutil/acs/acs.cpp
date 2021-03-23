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
#include "wwivutil/acs/acs.h"

#include "common/value/uservalueprovider.h"
#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/acs/eval.h"

#include <iostream>
#include <string>

using wwiv::core::BooleanCommandLineArgument;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::acs;
using namespace wwiv::strings;

namespace wwiv::wwivutil::acs {

AcsCommand::AcsCommand() : UtilCommand("acs", "Evaluates an ACS expression for a user.") {}

std::string AcsCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage: " << std::endl << std::endl;
  ss << "  acs : Evaluates an ACS expression for a user." << std::endl;
  return ss.str();
}

int AcsCommand::Execute() {
  if (remaining().empty()) {
    std::cout << GetUsage() << GetHelp() << std::endl;
    return 2;
  }
  const auto expr = remaining().front();
  const auto user_number = iarg("user");

  const UserManager um(*config()->config());
  const auto user = um.readuser_nocache(user_number);
  if (!user) {
    LOG(ERROR) << "Failed to load user number " << user_number;
    return 1;
  }

  Eval eval(expr);
  const auto sl = user->sl();
  const auto& slr = config()->config()->sl(sl);
  common::value::UserValueProvider u(*config()->config(), user.value(), sl, slr);
  eval.add(&u);

  const auto result = eval.eval();
  std::cout << "Evaluate: '" << expr << "' ";
  if (!result) {
    std::cout << "evaluated to FALSE" << std::endl;
  } else {
    std::cout << "evaluated to TRUE" << std::endl;
  }
  std::cout << std::endl;

  if (eval.error()) {
    std::cout << "Expression has the following error: " << eval.error_text()
              << std::endl;
    return 1;
  }

  if (barg("debug")) {
    std::cout << "Execution Trace: " << std::endl;
    for (const auto& l : eval.debug_info()) {
      std::cout << "       + " << l << std::endl;
    }
  }

  return 0;
}

bool AcsCommand::AddSubCommands() {
  add_argument(BooleanCommandLineArgument{"debug", "Display expression evaluation at end of execution.", true});
  add_argument({"user", "user number to use while evaluating the expression", "1"});
  return true;
}

} // namespace wwiv
