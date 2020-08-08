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
#include "core/parser/ast.h"
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

class AstTest : public ::testing::Test {
public:
  AstTest() {}

  ::testing::AssertionResult HasOp(Expression *e, Operator op) {
    if (e->op == op)
      return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure() << to_string(e->op) << " was not: " << to_string(op);
  }
  ::testing::AssertionResult HasLeftVariable(Expression* e, std::string v) {
    if (!e->left_factor)
      return ::testing::AssertionFailure() << "missing left variable: " << v;
    auto val = e->left_factor->value();
    if (val == v) {
      return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << v << " != " << val;
  }
};

TEST_F(AstTest, Expr_Add) { 
  Lexer l("1+2"); 

  auto tokens = l.tokens();
  ASSERT_TRUE(l.ok());
  auto it = std::begin(tokens);
  Ast ast{};
  auto root = ast.parse(it, std::end(tokens));
  LOG(INFO) << root->ToString();
}

TEST_F(AstTest, Expr_Eq) {
  Lexer l("user.sl>200");

  auto tokens = l.tokens();
  ASSERT_TRUE(l.ok());
  auto it = std::begin(tokens);
  Ast ast;
  auto root = ast.parse(it, std::end(tokens));
  LOG(INFO) << root->ToString();
}

TEST_F(AstTest, Expr_Parens) {
  Lexer l("(user.sl>200) || user.ar == 'A'");

  auto tokens = l.tokens();
  ASSERT_TRUE(l.ok());
  auto it = std::begin(tokens);
  Ast ast;
  auto root = ast.parse(it, std::end(tokens));
  LOG(INFO) << root->ToString();
  ASSERT_EQ(AstType::EXPR, root->node->type_);

  auto expr = dynamic_cast<Expression*>(root->node.get());
  EXPECT_TRUE(HasOp(expr, Operator::or));
  ASSERT_NE(nullptr, expr);

  auto left = dynamic_cast<Expression*>(expr->left_expression.get());
  LOG(INFO) << "Left: " << left->ToString();
  EXPECT_TRUE(HasOp(left, Operator:: gt));
  EXPECT_TRUE(HasLeftVariable(left, "user.sl"));
  auto right = dynamic_cast<Expression*>(expr->right_expression.get());
  LOG(INFO) << "Right: " << right->ToString();
  EXPECT_TRUE(HasOp(right, Operator::eq));
}
