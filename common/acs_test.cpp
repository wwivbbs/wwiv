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
#include "common/value/uservalueprovider.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/user.h"
#include "sdk/acs/eval.h"

#include "gtest/gtest.h"
#include <string>

using namespace wwiv::common::value;
using namespace wwiv::core;
using namespace wwiv::sdk::acs;
using namespace wwiv::sdk::value;

class AcsTest : public ::testing::Test {
public:
  AcsTest() : config_("", wwiv::sdk::config_t{}) {
    config_.set_initialized_for_test(true);
    config_.newuser_sl(10);
    config_.validated_sl(20);
  }

  void createEval(const std::string& expr) { 
    eval = std::make_unique<Eval>(expr);
    eval->add(std::make_unique<UserValueProvider>(config_, user_, user_.sl(), sl_));

  }
  std::unique_ptr<Eval> eval;
  wwiv::sdk::Config config_;
  wwiv::sdk::User user_{};
  slrec sl_{};
};

TEST_F(AcsTest, SL_GT_Pass) {
  user_.sl(201);
  createEval("user.sl>200");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, SL_GT_False) {
  user_.sl(10);
  createEval("user.sl>200");
  EXPECT_FALSE(eval->eval());
}

TEST_F(AcsTest, DummySL_LT) {
  user_.sl(10);
  createEval("user.sl<200");

  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, DummySL_LT_False) {
  user_.sl(25);
  createEval("user.sl<20");

  EXPECT_FALSE(eval->eval());
}

TEST_F(AcsTest, Or) {
  user_.sl(201);
  createEval("user.sl>200 || user.dsl > 200 || user.name == \"Rushfan\"");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Multiple_Group_SL) {
  user_.sl(201);
  user_.dsl(100);
  user_.set_name("SYSOP");
  createEval("(user.sl>200 || user.dsl > 200) || user.name == \"Rushfan\"");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Multiple_Group_DSL) {
  user_.sl(10);
  user_.dsl(201);
  user_.set_name("SYSOP");
  createEval("(user.sl>200 || user.dsl > 200) || user.name == \"Rushfan\"");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Multiple_Group_None) {
  user_.sl(22);
  user_.dsl(12);
  user_.set_name("SYSOP");
  createEval("(user.sl>200 || user.dsl > 200) || user.name == \"Rushfan\"");
  EXPECT_FALSE(eval->eval());
  LOG(INFO) << wwiv::strings::JoinStrings(eval->debug_info(), "\r\n");
}

TEST_F(AcsTest, Ar_Pass) {
  user_.ar_int(2);  // B
  createEval("user.ar == 'B'");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Ar_Fail) {
  user_.ar_int(5); // a || c
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
  user_.sl(255);
  createEval("user.sysop == \"true\"");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Sysop_Pass_Literal) {
  user_.sl(255);
  createEval("user.sysop == true");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Sysop_Fail) {
  user_.sl(200);
  createEval("user.sysop == true");
  EXPECT_FALSE(eval->eval());
}

TEST_F(AcsTest, Sysop_Pass_Negated) {
  user_.sl(200);
  createEval("user.sysop == false");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Regnum_Pass) {
  user_.sl(12);
  user_.wwiv_regnum(12345);
  createEval("user.registered == true");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, Regnum_Fail) {
  user_.sl(12);
  user_.wwiv_regnum(0);
  createEval("user.registered == true");
  EXPECT_FALSE(eval->eval());
}

TEST_F(AcsTest, CoSysop_Pass) {
  user_.sl(200);
  sl_.ability |= ability_cosysop;
  createEval("user.cosysop == true");
  EXPECT_TRUE(eval->eval());
}

TEST_F(AcsTest, CoSysop_Fail) {
  user_.sl(200);
  sl_.ability &= ~ability_cosysop;
  createEval("user.cosysop == true");
  EXPECT_FALSE(eval->eval());
}
