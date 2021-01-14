/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2007-2021, WWIV Software Services               */
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

#include "core/fake_clock.h"
#include "sdk/user.h"
#include <cstring>
#include <type_traits>

using std::cout;
using std::endl;
using std::is_standard_layout;
using std::string;

using namespace wwiv::sdk;

TEST(UserTest, IsStandardLayout) {
  ASSERT_TRUE(is_standard_layout<User>::value);
  EXPECT_EQ(sizeof(userrec), sizeof(User));
}

TEST(UserTest, CanCopyToUser) {
  userrec u{};
  strcpy(reinterpret_cast<char*>(u.name), "Test User");

  User user{};
  memcpy(&user, &u, sizeof(userrec));

  EXPECT_STREQ(reinterpret_cast<char*>(u.name), user.GetName());
}

TEST(UserTest, Birthday) {
  User u;
  u.birthday_mdy(2, 15, 1980);

  EXPECT_EQ("02/15/80", u.birthday_mmddyy());
}

TEST(UserTest, YearsOld) {
  tm t{};
  t.tm_mon = 9;
  t.tm_mday = 25;
  t.tm_year = 120;
  const auto now = wwiv::core::DateTime::from_tm(&t);
  wwiv::core::FakeClock clock(now);
  User u{};
  u.birthday_mdy(10, 24, 1970);
  EXPECT_EQ(50, years_old(&u, clock));
}
