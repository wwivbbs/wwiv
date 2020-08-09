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

#include "core/parser/lexer.h"
#include <string>
#include <iostream>

using std::string;
using namespace wwiv::core;
using namespace wwiv::core::parser;

#define EXPECT_TOKEN_EQ(tok, typ, m)                                                               \
  {                                                                                                \
    EXPECT_EQ((tok).type, typ);                                                                    \
    EXPECT_EQ((tok).lexmeme, m);                                                                    \
  }

class LexerTest : public ::testing::Test {
public:
  LexerTest() {}
};

TEST_F(LexerTest, Add) { 
  Lexer l("1+2"); 
  ASSERT_TRUE(l.ok());

  const auto& t = l.tokens();
  EXPECT_EQ(3u, t.size());
  EXPECT_TOKEN_EQ(t[0], TokenType::number, "1");
  EXPECT_TOKEN_EQ(t[1], TokenType::add, "");
  EXPECT_TOKEN_EQ(t[2], TokenType::number, "2");
}

TEST_F(LexerTest, Sub) {
  Lexer l("3-4");
  ASSERT_TRUE(l.ok());

  const auto& t = l.tokens();
  EXPECT_EQ(3u, t.size());
  EXPECT_TOKEN_EQ(t[0], TokenType::number, "3");
  EXPECT_TOKEN_EQ(t[1], TokenType::sub, "");
  EXPECT_TOKEN_EQ(t[2], TokenType::number, "4");
}

TEST_F(LexerTest, Parens) {
  Lexer l("(1+2)*3");
  ASSERT_TRUE(l.ok());

  const auto& t = l.tokens();
  EXPECT_EQ(7u, t.size());
  EXPECT_TOKEN_EQ(t[0], TokenType::lparen, "");
  EXPECT_TOKEN_EQ(t[1], TokenType::number, "1");
  EXPECT_TOKEN_EQ(t[2], TokenType::add, "");
  EXPECT_TOKEN_EQ(t[3], TokenType::number, "2");
  EXPECT_TOKEN_EQ(t[4], TokenType::rparen, "");
  EXPECT_TOKEN_EQ(t[5], TokenType::mul, "");
  EXPECT_TOKEN_EQ(t[6], TokenType::number, "3");
}

TEST_F(LexerTest, Eq) {
  Lexer l("user.sl == 10");
  ASSERT_TRUE(l.ok());

  const auto& t = l.tokens();
  EXPECT_EQ(3u, t.size());
  EXPECT_TOKEN_EQ(t[0], TokenType::identifier, "user.sl");
  EXPECT_TOKEN_EQ(t[1], TokenType::eq, "");
  EXPECT_TOKEN_EQ(t[2], TokenType::number, "10");
}

TEST_F(LexerTest, Or) {
  Lexer l("(user.sl>200) || user.ar == 'A'");
  ASSERT_TRUE(l.ok());

  const auto& t = l.tokens();
  EXPECT_EQ(9u, t.size());
  EXPECT_TOKEN_EQ(t[5], TokenType::logical_or, "");
}
