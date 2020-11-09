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

#include "core/log.h"
#include "core/strings.h"
#include "core/parser/ast.h"
#include "core/parser/lexer.h"
#include "sdk/user.h"
#include "sdk/acs/eval.h"
#include "sdk/acs/uservalueprovider.h"
#include <string>
#include <iostream>

using std::string;
using namespace wwiv::core;
using namespace wwiv::core::parser;
using namespace wwiv::sdk::acs;

class AcsTest : public ::testing::Test {
public:
  AcsTest() = default;

  void createEval(const std::string& expr) { 
    eval = std::make_unique<Eval>(expr);
    eval->add("user", std::make_unique<UserValueProvider>(&user_, user_.GetSl(), sl_));

  }
  std::unique_ptr<Eval> eval;
  wwiv::sdk::User user_{};
  slrec sl_{};
};

TEST_F(AcsTest, SL_GT_Pass) {
  user_.SetSl(201);
  createEval("user.sl>200");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, SL_GT_False) {
  user_.SetSl(10);
  createEval("user.sl>200");
  EXPECT_FALSE(eval->eval());
}

TEST_F(AcsTest, DummySL_LT) {
  user_.SetSl(10);
  createEval("user.sl<200");

  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, DummySL_LT_False) {
  user_.SetSl(25);
  createEval("user.sl<20");

  EXPECT_FALSE(eval->eval());
}

TEST_F(AcsTest, Or) {
  user_.SetSl(201);
  createEval("user.sl>200 || user.dsl > 200 || user.name == \"Rushfan\"");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Multiple_Group_SL) {
  user_.SetSl(201);
  user_.SetDsl(100);
  user_.set_name("SYSOP");
  createEval("(user.sl>200 || user.dsl > 200) || user.name == \"Rushfan\"");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Multiple_Group_DSL) {
  user_.SetSl(10);
  user_.SetDsl(201);
  user_.set_name("SYSOP");
  createEval("(user.sl>200 || user.dsl > 200) || user.name == \"Rushfan\"");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Multiple_Group_None) {
  user_.SetSl(22);
  user_.SetDsl(12);
  user_.set_name("SYSOP");
  createEval("(user.sl>200 || user.dsl > 200) || user.name == \"Rushfan\"");
  EXPECT_FALSE(eval->eval());
  LOG(INFO) << wwiv::strings::JoinStrings(eval->debug_info(), "\r\n");
}

TEST_F(AcsTest, Ar_Pass) {
  user_.SetAr(2);  // B
  createEval("user.ar == 'B'");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Ar_Fail) {
  user_.SetAr(5); // a || c
  createEval("user.ar == 'B'");
  EXPECT_FALSE(eval->eval());
}

TEST_F(AcsTest, BadAttrOnUser) {
  createEval("user.foo<20");

  EXPECT_FALSE(eval->eval());
  EXPECT_EQ(eval->error_text(), "No user attribute named 'user.foo' exists.");
  LOG(INFO) << wwiv::strings::JoinStrings(eval->debug_info(), "\r\n");
}

TEST_F(AcsTest, BadExpression) { 
  createEval("foo == ~ foo");
  EXPECT_FALSE(eval->eval());
  LOG(INFO) << wwiv::strings::JoinStrings(eval->debug_info(), "\r\n");
}

TEST_F(AcsTest, Sysop_Pass) {
  user_.SetSl(255);
  createEval("user.sysop == \"true\"");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Sysop_Pass_Literal) {
  user_.SetSl(255);
  createEval("user.sysop == true");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Sysop_Fail) {
  user_.SetSl(200);
  createEval("user.sysop == true");
  EXPECT_FALSE(eval->eval());
}

TEST_F(AcsTest, Sysop_Pass_Negated) {
  user_.SetSl(200);
  createEval("user.sysop == false");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Regnum_Pass) {
  user_.SetSl(12);
  user_.SetWWIVRegNumber(12345);
  createEval("user.regnum == true");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Regnum_Fail) {
  user_.SetSl(12);
  user_.SetWWIVRegNumber(0);
  createEval("user.regnum == true");
  EXPECT_FALSE(eval->eval());
}

TEST_F(AcsTest, CoSysop_Pass) {
  user_.SetSl(200);
  sl_.ability |= ability_cosysop;
  createEval("user.cosysop == true");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, CoSysop_Fail) {
  user_.SetSl(200);
  sl_.ability &= ~ability_cosysop;
  createEval("user.cosysop == true");
  EXPECT_FALSE(eval->eval());
}
