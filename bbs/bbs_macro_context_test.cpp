/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2022, WWIV Software Services               */
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
#include "bbs/bbs.h"
#include "bbs/interpret.h"
#include "bbs/bbs_helper.h"
#include "common/context.h"
#include "common/macro_context.h"
#include "common/pipe_expr.h"

#include "gtest/gtest.h"
#include <string>

using namespace wwiv::common;

class BbsMacroContextTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    pipe_eval = std::make_unique<PipeEval>(a()->context());
    bc = std::make_unique<BbsMacroContext>(&a()->context(), *pipe_eval);
  }

  BbsHelper helper{};
  std::unique_ptr<BbsMacroContext> bc;
  std::unique_ptr<PipeEval> pipe_eval;
};

TEST_F(BbsMacroContextTest, Move_Up2) {
  std::string s1 = "[2A";
  auto it = std::cbegin(s1);

  const auto i = bc->interpret(it, std::end(s1));
  EXPECT_EQ(i.cmd, interpreted_cmd_t::movement);
  EXPECT_EQ(i.up, 2);
}

TEST_F(BbsMacroContextTest, Move_Up) {
  std::string s1 = "[A";
  auto it = std::cbegin(s1);

  const auto i = bc->interpret(it, std::end(s1));
  EXPECT_EQ(i.cmd, interpreted_cmd_t::movement);
  EXPECT_EQ(i.up, 1);
}

TEST_F(BbsMacroContextTest, Move_Down) {
  std::string s1 = "[2B";
  auto it = std::cbegin(s1);
  const auto i = bc->interpret(it, std::end(s1));
  EXPECT_EQ(i.cmd, interpreted_cmd_t::movement);
  EXPECT_EQ(i.up, 0);
  EXPECT_EQ(i.down, 2);
}

TEST_F(BbsMacroContextTest, Move_Left) {
  std::string s1 = "[2D";
  auto it = std::cbegin(s1);

  const auto i = bc->interpret(it, std::end(s1));
  EXPECT_EQ(i.cmd, interpreted_cmd_t::movement);
  EXPECT_EQ(i.left, 2);
}

TEST_F(BbsMacroContextTest, Move_Right) {
  std::string s1 = "[2C";
  auto it = std::cbegin(s1);

  const auto i = bc->interpret(it, std::end(s1));
  EXPECT_EQ(i.cmd, interpreted_cmd_t::movement);
  EXPECT_EQ(i.right, 2);
}

TEST_F(BbsMacroContextTest, Move_XY) {
  std::string s1 = "[10;20H";
  auto it = std::cbegin(s1);

  const auto i = bc->interpret(it, std::end(s1));
  EXPECT_EQ(i.cmd, interpreted_cmd_t::movement);
  EXPECT_EQ(i.x, 10);
  EXPECT_EQ(i.y, 20);
}

TEST_F(BbsMacroContextTest, Expr_Ending) {
  std::string s1 = "{hello}";
  auto it = std::cbegin(s1);
  const auto i = bc->interpret(it, std::end(s1));
  EXPECT_EQ(i.cmd, interpreted_cmd_t::text);
  EXPECT_EQ(i.text, "{hello}");
}

TEST_F(BbsMacroContextTest, Expr_WithAdditional) {
  std::string s1 = "{hello}Hello";
  auto it = std::cbegin(s1);
  const auto i = bc->interpret(it, std::end(s1));
  EXPECT_EQ(i.cmd, interpreted_cmd_t::text);
  EXPECT_EQ(i.text, "{hello}");
  EXPECT_EQ("Hello", std::string(it, std::cend(s1)));
}

TEST_F(BbsMacroContextTest, JustText) {
  const std::string s1 = "hello";
  auto it = std::cbegin(s1);
  const auto end = std::cend(s1);
  const auto i = bc->interpret(it, end);
  EXPECT_EQ(i.cmd, interpreted_cmd_t::text);
  EXPECT_EQ(i.text, "h");
  EXPECT_EQ('e', *it);
  EXPECT_EQ(it, std::cbegin(s1) + 1);
}

TEST_F(BbsMacroContextTest, Variable) {
  const std::string s1 = "{user.name}";
  auto it = std::cbegin(s1);
  helper.user()->set_name("Rushfan");
  helper.user()->sl(200);
  helper.sess().effective_sl(200);
  const auto i = bc->interpret(it, std::cend(s1));
  EXPECT_EQ(i.text, "Rushfan");
}

TEST_F(BbsMacroContextTest, If) {
  const std::string s1 = R"({if "user.sl >= 200", "over", "under"})";
  helper.user()->set_name("Rushfan");
  helper.user()->real_name("RealName");
  {
    helper.user()->sl(200);
    helper.sess().effective_sl(200);
    auto it = std::cbegin(s1);
    const auto i = bc->interpret(it, std::cend(s1));
    EXPECT_EQ(i.text, "over");
  }
  {
    helper.user()->sl(199);
    helper.sess().effective_sl(199);
    auto it = std::cbegin(s1);
    const auto i = bc->interpret(it, std::cend(s1));
    EXPECT_EQ(i.text, "under");
  }
}
