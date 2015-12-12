/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#include <cstdlib>
#include <memory>
#include <string>

#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/wconstants.h"
#include "bbs/wuser.h"
#include "bbs/wsession.h"
#include "bbs/vars.h"
#include "bbs/wstatus.h"
#include "core/strings.h"
#include "core/file.h"
#include "sdk/filenames.h"

using std::string;
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
//  -2      = WWIVnet
//
int finduser(const string& searchString) {
  WUser user;

  guest_user = false;
  session()->users()->SetUserWritesAllowed(true);
  if (searchString == "NEW") {
    return -1;
  }
  if (searchString == "!-@NETWORK@-!") {
    return -2;
  }
  int nUserNumber = atoi(searchString.c_str());
  if (nUserNumber > 0) {
    session()->users()->ReadUser(&user, nUserNumber);
    if (user.IsUserDeleted()) {
      return 0;
    }
    return nUserNumber;
  }
  nUserNumber = session()->users()->FindUser(searchString);
  if (nUserNumber == 0L) {
    return 0;
  } 
  session()->users()->ReadUser(&user, nUserNumber);
  if (user.IsUserDeleted()) {
    return 0;
  }
  if (IsEqualsIgnoreCase(user.GetName(), "GUEST")) {
    guest_user = true;
    session()->users()->SetUserWritesAllowed(false);
  }
  return nUserNumber;
}


// Takes user name/handle as parameter, and returns user number, if found,
// else returns 0.
int finduser1(const string& searchString) {
  if (searchString.empty()) {
    return 0;
  }
  int nFindUserNum = finduser(searchString);
  if (nFindUserNum > 0) {
    return nFindUserNum;
  }

  string userNamePart = searchString;
  StringUpperCase(&userNamePart);
  for (int i1 = 0; i1 < session()->GetStatusManager()->GetUserCount(); i1++) {
    if (strstr(reinterpret_cast<char*>(smallist[i1].name), userNamePart.c_str()) != nullptr) {
      int nCurrentUserNum = smallist[i1].number;
      WUser user;
      session()->users()->ReadUser(&user, nCurrentUserNum);
      bout << "|#5Do you mean " << user.GetUserNameAndNumber(nCurrentUserNum) << " (Y/N/Q)? ";
      char ch = ynq();
      if (ch == 'Y') {
        return nCurrentUserNum;
      }
      if (ch == 'Q') {
        return 0;
      }
    }
  }
  return 0;
}
