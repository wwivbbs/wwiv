/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include "sdk/user.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <memory>
#include "core/strings.h"
#include "core/file.h"
#include "sdk/names.h"
#include "sdk/filenames.h"

using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

User::User() {
  ZeroUserData();
}

User::~User() {}

User::User(const User& w) {
  memcpy(&data, &w.data, sizeof(userrec));
}

User& User::operator=(const User& rhs) {
  if (this == &rhs) {
    return *this;
  }
  memcpy(&data, &rhs.data, sizeof(userrec));
  return *this;
}

void User::FixUp() {
  data.name[sizeof(data.name) - 1]      = '\0';
  data.realname[sizeof(data.realname) - 1]  = '\0';
  data.callsign[sizeof(data.callsign) - 1]  = '\0';
  data.phone[sizeof(data.phone) - 1]      = '\0';
  data.dataphone[sizeof(data.dataphone) - 1]  = '\0';
  data.street[sizeof(data.street) - 1]    = '\0';
  data.city[sizeof(data.city) - 1]      = '\0';
  data.state[sizeof(data.state) - 1]      = '\0';
  data.country[sizeof(data.country) - 1]    = '\0';
  data.zipcode[sizeof(data.zipcode) - 1]    = '\0';
  data.pw[sizeof(data.pw) - 1]        = '\0';
  data.laston[sizeof(data.laston) - 1]    = '\0';
  data.firston[sizeof(data.firston) - 1]    = '\0';
  data.firston[2]                 = '/';
  data.firston[5]                 = '/';
  data.note[sizeof(data.note) - 1]      = '\0';
  data.macros[0][sizeof(data.macros[0]) - 1]  = '\0';
  data.macros[1][sizeof(data.macros[1]) - 1]  = '\0';
  data.macros[2][sizeof(data.macros[2]) - 1]  = '\0';
}

void User::ZeroUserData() {
  memset(&data, 0, sizeof(userrec));
}

}  // namespace sdk
}  // namespace wwiv
