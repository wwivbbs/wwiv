/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2020-2021, WWIV Software Services           */
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

#include "core/parser/ast.h"
#include "sdk/user.h"
#include "common/value/uservalueprovider.h"

using namespace wwiv::core;
using namespace wwiv::core::parser;
using namespace wwiv::sdk::value;

class ArTest : public ::testing::Test {
public:
  ArTest() = default;

};

TEST_F(ArTest, Eq) {
  const Ar a('A');
  const Ar user_abcd("ABCD", true);
  EXPECT_EQ(user_abcd, a);
  EXPECT_EQ(a, user_abcd);
}

TEST_F(ArTest, Ne) {
  const Ar a('A');
  const Ar user_cde("CDE", true);
  EXPECT_NE(user_cde, a);
  EXPECT_NE(a, user_cde);
}

TEST_F(ArTest, Eq_Conversion) {
  const Ar user_abcd("ABCD", true);

  EXPECT_EQ(user_abcd, 'A');
  EXPECT_EQ(user_abcd, "A");
  EXPECT_EQ("A", user_abcd);
  EXPECT_EQ("A", user_abcd);
}

TEST_F(ArTest, Ne_Conversion) {
  const Ar user_cde("CDE", true);
  EXPECT_NE(user_cde, 'A');
  EXPECT_NE(user_cde, "A");
  EXPECT_NE('A', user_cde);
  EXPECT_NE("A", user_cde);
}

TEST_F(ArTest, Eq_Userside_Empty) {
  const Ar e(0, true);
  EXPECT_NE(e, "A");
  EXPECT_NE("A", e);
}

TEST_F(ArTest, Eq_OtherSide_Empty) {
  const Ar user("ABC", true);
  EXPECT_EQ(user, Ar(0, false));
  EXPECT_EQ(Ar(0, false), user);
}

