/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                    Copyright (C)2020, WWIV Software Services           */
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
#include "core/strings.h"
#include "sdk/acs/expr.h"
#include <string>

using std::string;
using namespace wwiv::core;
using namespace parser;
using namespace wwiv::sdk::acs;

class AcsExprTest : public testing::Test {
public:
  AcsExpr ae;
};

TEST_F(AcsExprTest, MultipleLines) {
  ae.max_age(18);
  EXPECT_EQ(ae.get(), "user.age <= 18");
}

TEST_F(AcsExprTest, Age_Min) { EXPECT_EQ(ae.min_age(18).get(), "user.age >= 18"); }
TEST_F(AcsExprTest, Age_Max) { EXPECT_EQ(ae.max_age(21).get(), "user.age <= 21"); }
TEST_F(AcsExprTest, Age_Min_Max) {
  EXPECT_EQ(ae.min_age(18).max_age(21).get(), "user.age >= 18 && user.age <= 21");
}

TEST_F(AcsExprTest, Ar) { EXPECT_EQ(ae.ar('C').get(), "user.ar == 'C'"); }
TEST_F(AcsExprTest, Ar_Bad) { EXPECT_EQ(ae.ar('Z').get(), ""); }
TEST_F(AcsExprTest, SL_Min) { EXPECT_EQ(ae.min_sl(18).get(), "user.sl >= 18"); }
TEST_F(AcsExprTest, SL_Min_255) { EXPECT_EQ(ae.min_sl(255).get(), "user.sl >= 255"); }
TEST_F(AcsExprTest, SL_Max) { EXPECT_EQ(ae.max_sl(21).get(), "user.sl <= 21"); }
TEST_F(AcsExprTest, SL_Min_Max) {
  EXPECT_EQ(ae.min_sl(18).max_sl(21).get(), "user.sl >= 18 && user.sl <= 21");
}

TEST_F(AcsExprTest, Ar_Int) { EXPECT_EQ(ae.ar_int(1).get(), "user.ar == 'A'"); }

TEST_F(AcsExprTest, Dar) { EXPECT_EQ(ae.dar('C').get(), "user.dar == 'C'"); }
TEST_F(AcsExprTest, Dar_Bad) { EXPECT_EQ(ae.dar('Z').get(), ""); }
TEST_F(AcsExprTest, Dar_Int) { EXPECT_EQ(ae.dar_int(4).get(), "user.dar == 'C'"); }


TEST_F(AcsExprTest, DSL_Min) { EXPECT_EQ(ae.min_dsl(18).get(), "user.dsl >= 18"); }
TEST_F(AcsExprTest, DSL_Max) { EXPECT_EQ(ae.max_dsl(21).get(), "user.dsl <= 21"); }
TEST_F(AcsExprTest, DSL_Min_Max) {
  EXPECT_EQ(ae.min_dsl(18).max_dsl(21).get(), "user.dsl >= 18 && user.dsl <= 21");
}

TEST_F(AcsExprTest, DSL_Min_255) { EXPECT_EQ(ae.min_dsl(255).get(), "user.dsl >= 255"); }

TEST_F(AcsExprTest, RegNum) { EXPECT_EQ(ae.regnum(true).get(), "user.regnum == true"); }

TEST_F(AcsExprTest, MultipleConditions) {
  EXPECT_EQ(ae.min_dsl(18).max_dsl(21).dar('A').regnum(true).get(),
            "user.dar == 'A' && user.dsl >= 18 && user.dsl <= 21 && user.regnum == true");
}
