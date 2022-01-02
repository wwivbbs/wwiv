/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "bbs/inetmsg.h"

#include "bbs/bbs.h"
#include "bbs/connect1.h"
#include "bbs/email.h"
#include "common/input.h"
#include "bbs/instmsg.h"
#include "bbs/misccmd.h"
#include "common/quote.h"
#include "bbs/utility.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "sdk/filenames.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;


bool check_inet_addr(const std::string& a) {
  return !a.empty() && (contains(a, '@') && contains(a, '.'));
}

std::string read_inet_addr(int user_number) {
  if (!user_number) {
    return {};
  }

  if (user_number == a()->user()->usernum()) {
    if (check_inet_addr(a()->user()->email_address())) {
      return  a()->user()->email_address();
    }
  } else if (auto user = a()->users()->readuser(user_number)) {
    return user->email_address();
  }
  return {};
}

