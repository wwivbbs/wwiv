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

TEST(UserAsvTest, SL) {
  User u{};
  u.sl(10);

  valrec v{20, 0, 0, 0, 0};
  ASSERT_TRUE(u.asv(v));
  EXPECT_EQ(20, u.sl());
}

TEST(UserAsvTest, SL_Lower) {
  User u{};
  u.sl(50);

  valrec v{20, 0, 0, 0, 0};
  ASSERT_TRUE(u.asv(v));
  EXPECT_EQ(50, u.sl());
}

TEST(UserAsvTest, DSL) {
  User u{};
  u.dsl(30);

  valrec v{20, 50, 0, 0, 0};
  ASSERT_TRUE(u.asv(v));
  EXPECT_EQ(50, u.dsl());
}

TEST(UserAsvTest, DSL_Lower) {
  User u{};
  u.dsl(50);

  valrec v{20, 30, 0, 0, 0};
  ASSERT_TRUE(u.asv(v));
  EXPECT_EQ(50, u.dsl());
}

User CreateUser(int sl, int dsl, int ar, int dar, uint16_t r) {
  User u{};
  u.sl(sl);
  u.dsl(dsl);
  u.ar_int(ar);
  u.dar_int(dar);
  u.restriction(r);
  return u;
}

TEST(UserAsvTest, AR) {
  User u = CreateUser(10, 0, 1, 0, 0);

  valrec v{20, 0, 4, 0, 0};
  ASSERT_TRUE(u.asv(v));
  EXPECT_EQ(5, u.ar_int());
}

TEST(UserAsvTest, AR_Lower) {
  User u = CreateUser(10, 0, 1, 0, 0);

  valrec v{10, 0, 3, 0, 0};
  ASSERT_TRUE(u.asv(v));
  EXPECT_EQ(3, u.ar_int());
}

TEST(UserAsvTest, DAR) {
  User u = CreateUser(10, 0, 0, 1, 0);

  valrec v{20, 0, 0, 4, 0};
  ASSERT_TRUE(u.asv(v));
  EXPECT_EQ(5, u.dar_int());
}

TEST(UserAsvTest, DAR_Lower) {
  User u = CreateUser(10, 0, 0, 1, 0);

  valrec v{10, 0, 0, 3, 0};
  ASSERT_TRUE(u.asv(v));
  EXPECT_EQ(3, u.dar_int());
}

TEST(UserAsvTest, Restrict) {
  auto u = CreateUser(10, 0, 1, 0, 2);

  valrec v{20, 0, 1, 0, 4};
  ASSERT_TRUE(u.asv(v));
  EXPECT_EQ(0, u.restriction());
}

TEST(UserAsvTest, Restrict_Lower) {
  auto u = CreateUser(10, 0, 1, 0, 6);

  valrec v{10, 0, 3, 0, 4};
  ASSERT_TRUE(u.asv(v));
  EXPECT_EQ(4, u.restriction());
}