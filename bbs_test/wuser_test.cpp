/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)2007, WWIV Software Services                  */
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
#include "gtest/gtest.h"

#include <cstring>
#include <type_traits>

#include "bbs/wuser.h"

using std::cout;
using std::endl;
using std::is_standard_layout;
using std::string;

TEST(WUserTest, IsStandardLayout) {
  ASSERT_TRUE(is_standard_layout<WUser>::value);
  EXPECT_EQ(sizeof(userrec), sizeof(WUser));
}

TEST(WUserTest, CanCopyToWUser) {
  userrec u;
  strcpy(reinterpret_cast<char*>(u.name), "Test User");

  WUser user;
  memcpy(&user, &u, sizeof(userrec));

  EXPECT_STREQ(reinterpret_cast<char*>(u.name), user.GetName());
}