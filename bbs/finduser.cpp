/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
/*                                                                        */
/**************************************************************************/
#include "bbs/finduser.h"
#include "bbs/application.h"
#include "bbs/bbs.h"
#include "common/com.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/names.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include <string>

using std::string;
using namespace wwiv::sdk;
using namespace wwiv::strings;

//
// Returns user number
//      or 0 if user not found
//      or special value
//
// This function will remove the special characters from arround the searchString that are
// used by the network and remote, etc.
//
// Special values:
//
//  -1      = NEW USER
//
int finduser(const string& searchString) {
  User user;

  a()->context().guest_user(false);
  a()->users()->set_user_writes_allowed(true);
  if (searchString == "NEW") {
    return -1;
  }
  auto user_number = to_number<int>(searchString);
  if (user_number > 0) {
    a()->users()->readuser(&user, user_number);
    if (user.IsUserDeleted()) {
      return 0;
    }
    return user_number;
  }
  user_number = a()->names()->FindUser(searchString);
  if (user_number == 0) {
    return 0;
  } 
  a()->users()->readuser(&user, user_number);
  if (user.IsUserDeleted()) {
    return 0;
  }
  if (iequals(user.GetName(), "GUEST")) {
    a()->context().guest_user(true);
    a()->users()->set_user_writes_allowed(false);
  }
  return user_number;
}


// Takes user name/handle as parameter, and returns user number, if found,
// else returns 0.
int finduser1(const string& searchString) {
  if (searchString.empty()) {
    return 0;
  }
  const auto un = finduser(searchString);
  if (un > 0) {
    return un;
  }

  const auto name_part = ToStringUpperCase(searchString);
  for (const auto& n : a()->names()->names_vector()) {
    if (strstr(reinterpret_cast<const char*>(n.name), name_part.c_str()) == nullptr) {
      continue;
    }

    bout << "|#5Do you mean " << a()->names()->UserName(n.number) << " (Y/N/Q)? ";
    const auto ch = ynq();
    if (ch == 'Y') {
      return n.number;
    }
    if (ch == 'Q') {
      return 0;
    }
  }
  return 0;
}
