/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5.x                            */
/*          Copyright (C)2020-2022, WWIV Software Services                */
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
/**************************************************************************/
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "common/pipe_expr.h"
#include "common/common_helper.h"

using namespace wwiv::common;
using namespace testing;

TEST(PipeExprTest, Smoke) {
  CommonHelper helper;
  helper.user()->set_name("Rushfan");
  PipeEval eval(helper.context());
  EXPECT_EQ("Rushfan", eval.eval("{user.name}"));
  EXPECT_EQ("{unknown}", eval.eval("{unknown}"));
}

TEST(PipeExprTest, If) {
  CommonHelper helper;
  helper.user()->set_name("Rushfan");
  helper.user()->sl(200);
  helper.sess().effective_sl(200);
  PipeEval eval(helper.context());
  EXPECT_EQ("Senior Level", eval.eval(R"({if "user.sl >= 200", "Senior Level", "Junior Level"})"));
  helper.user()->sl(100);
  helper.sess().effective_sl(100);
  EXPECT_EQ("Junior Level", eval.eval(R"({if "user.sl >= 200", "Senior Level", "Junior Level"})"));
}

TEST(PipeExprTest, Fmt_LeftPad) {
  CommonHelper helper;
  helper.user()->set_name("rf");
  PipeEval eval(helper.context());
  EXPECT_EQ("rf  ", eval.eval(R"({user.name, "<4"})"));
}

TEST(PipeExprTest, Fmt_RightPad) {
  CommonHelper helper;
  helper.user()->set_name("rf");
  PipeEval eval(helper.context());
  EXPECT_EQ("  rf", eval.eval(R"({user.name, ">4"})"));
}

TEST(PipeExprTest, Fmt_MidPad) {
  CommonHelper helper;
  helper.user()->set_name("rf");
  PipeEval eval(helper.context());
  EXPECT_EQ("  rf ", eval.eval(R"({user.name, "^5"})"));
}

TEST(PipeExpr_ParseFmtMaskTest, Smoke) {
  const auto m = parse_mask("<20");
  EXPECT_EQ(m.align, pipe_fmt_align_t::left);
  EXPECT_EQ(20, m.len);
  EXPECT_EQ(' ', m.pad);
}

TEST(PipeExpr_ParseFmtMaskTest, PadChar) {
  const auto m = parse_mask("X<10");
  EXPECT_EQ(m.align, pipe_fmt_align_t::left);
  EXPECT_EQ(10, m.len);
  EXPECT_EQ('X', m.pad);
}

TEST(PipeExprTest, Fmt_LeftPadCustomChar) {
  CommonHelper helper;
  helper.user()->set_name("rf");
  PipeEval eval(helper.context());
  EXPECT_EQ("rf**", eval.eval(R"({user.name, "*<4"})"));
}

TEST(PipeExprTest, Fmt_MidPadCustomChar) {
  CommonHelper helper;
  helper.user()->set_name("rf");
  PipeEval eval(helper.context());
  EXPECT_EQ("***rf**", eval.eval(R"({user.name, "*^7"})"));
}

TEST(PipeExpr_ParseFmtMaskTest, Right) {
  const auto m = parse_mask(">10");
  EXPECT_EQ(m.align, pipe_fmt_align_t::right);
  EXPECT_EQ(10, m.len);
}

TEST(PipeExpr_ParseFmtMaskTest, OnlyLength) {
  const auto m = parse_mask("10");
  EXPECT_EQ(m.align, pipe_fmt_align_t::left);
  EXPECT_EQ(10, m.len);
  EXPECT_EQ(' ', m.pad);
}

