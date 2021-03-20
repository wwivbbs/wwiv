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
#include "wwivutil/users/users.h"

#include "core/command_line.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/instance.h"
#include "sdk/usermanager.h"

#include <iostream>
#include <memory>
#include <string>

using wwiv::core::BooleanCommandLineArgument;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::wwivutil {

class UsersAsvCommand final : public UtilCommand {
public:
  UsersAsvCommand(): UtilCommand("asv", "Execute automatic sysop validation on a user.") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage: " << std::endl << std::endl;
    ss << "  asv --user=<num> --asv=1 : Autoval user." << std::endl << std::endl;
    return ss.str();
  }

  int Execute() override {
    if (const auto asv_string = sarg("asv"); asv_string.empty()) {
      std::cerr << "--asv must be specified." << std::endl;
      std::cerr << GetUsage();
      return 1;
    }    
    if (const auto user_string = sarg("user"); user_string.empty()) {
      std::cerr << "--user must be specified." << std::endl;
      std::cerr << GetUsage();
      return 1;
    }

    const auto user_number = iarg("user");
    UserManager um(*config()->config());
    if (user_number < 1 || user_number > um.num_user_records()) {
      std::cerr << "invalid user number for --user specified: " << user_number << std::endl;
      std::cerr << GetUsage();
      return 1;
    }
    const auto asv_num = iarg("asv");
    if (asv_num < 1 || asv_num > 10) {
      std::cerr << "invalid user number for --asv specified: " << asv_num << std::endl;
      std::cerr << GetUsage();
      return 1;
    }

    if (auto o = um.readuser(user_number)) {
      auto& user = o.value();
      if (const auto& v = config()->config()->auto_val(asv_num); !user.asv(v)) {
        std::cout << "Failed to apply ASV to user number: " << user_number;
        return 1;
      }
      if (!um.writeuser(&user, user_number)) {
        std::cout << "Unable to save user number: " << user_number;
        return 1;
      }
      std::cout << "Done!";
    } else {
      std::cout << "Unable to find user number: " << user_number;
      return 1;
    }
    return 0;
  }

  bool AddSubCommands() override {
    add_argument({"user", "user number to use while evaluating the expression", ""});
    add_argument({"asv", "user number to use while evaluating the expression", ""});
    return true;
  }

};

bool UsersCommand::AddSubCommands() {
  add(std::make_unique<UsersAsvCommand>());
  return true;
}


} 
