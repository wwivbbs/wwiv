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

#include "core/parser/lexer.h"
#include <string>

using namespace wwiv::core;
using namespace wwiv::core::parser;

#define EXPECT_TOKEN_EQ(tok, typ, m)                                                               \
  {                                                                                                \
    EXPECT_EQ((tok).type, typ);                                                                    \
    EXPECT_EQ((tok).lexeme, m);                                                                    \
  }

#define EXPECT_ID_OP_NUM(t, m, o, n)                                                               \
  {                                                                                                \
    EXPECT_EQ(3u, t.size());                                                                       \
    EXPECT_TOKEN_EQ((t[0]), TokenType::identifier, m);                                           \
    EXPECT_TOKEN_EQ((t[1]), o, "");                                                                    \
    EXPECT_TOKEN_EQ((t[2]), TokenType::number, n);                                           \
  }

#define EXPECT_LEXER_ID_OP_NUM(l, m, o, n)                                                         \
  {                                                                                                \
    ASSERT_TRUE(l.ok());                                                                             \
    const auto t = l.tokens();                                                                     \
    EXPECT_EQ(3u, t.size());                                                                       \
    EXPECT_TOKEN_EQ((t[0]), TokenType::identifier, m);                                             \
    EXPECT_TOKEN_EQ((t[1]), o, "");                                                                \
    EXPECT_TOKEN_EQ((t[2]), TokenType::number, n);                                                 \
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
  EXPECT_LEXER_ID_OP_NUM(l, "user.sl", TokenType::eq, "10");
}

TEST_F(LexerTest, NotEq) {
  Lexer l("user.sl != 11");
  EXPECT_LEXER_ID_OP_NUM(l, "user.sl", TokenType::ne, "11");
}

TEST_F(LexerTest, UserSlGt) {
  Lexer l("user.sl>200");
  EXPECT_LEXER_ID_OP_NUM(l, "user.sl", TokenType::gt, "200");
}

TEST_F(LexerTest, UserDSlGe) {
  Lexer l("user.dsl>=201");
  EXPECT_LEXER_ID_OP_NUM(l, "user.dsl", TokenType::ge, "201");
}

TEST_F(LexerTest, UserSlLt) {
  Lexer l("user.sl<255");
  EXPECT_LEXER_ID_OP_NUM(l, "user.sl", TokenType::lt, "255");
}

TEST_F(LexerTest, UserSlLe) {
  Lexer l("user.sl<=255");
  EXPECT_LEXER_ID_OP_NUM(l, "user.sl", TokenType::le, "255");
}

TEST_F(LexerTest, Or) {
  Lexer l("(user.sl>200) || user.ar == 'A'");
  ASSERT_TRUE(l.ok());

  const auto& t = l.tokens();
  EXPECT_EQ(9u, t.size());
  EXPECT_TOKEN_EQ(t[5], TokenType::logical_or, "");
}

TEST_F(LexerTest, And) {
  Lexer l("(user.sl>20) && user.ar == 'A'");
  ASSERT_TRUE(l.ok());

  const auto& t = l.tokens();
  EXPECT_EQ(9u, t.size());
  EXPECT_TOKEN_EQ(t[5], TokenType::logical_and, "");
}

TEST_F(LexerTest, Smoke_Error) {
  Lexer l("user.sl&10");
  EXPECT_FALSE(l.ok());
  const auto& t = l.tokens();
  ASSERT_GE(t.size(), 2u) << l;
  EXPECT_EQ(t[1].type, TokenType::error) << l;
}

TEST_F(LexerTest, Underscore) { 
  Lexer l("user.fs_reader"); 
  ASSERT_TRUE(l.ok());

  const auto& t = l.tokens();
  EXPECT_EQ(1u, t.size());
  EXPECT_TOKEN_EQ(t[0], TokenType::identifier, "user.fs_reader");
}

