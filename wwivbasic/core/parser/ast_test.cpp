/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2020-2022, WWIV Software Services           */
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

using namespace wwiv::core;
using namespace wwiv::core::parser;

#define EXPECT_TOKEN_EQ(tok, typ, m)                                                               \
  {                                                                                                \
    EXPECT_EQ((tok).type, typ);                                                                    \
    EXPECT_EQ((tok).lexeme, m);                                                                    \
  }

class AstTest : public ::testing::Test {
public:
  AstTest() {}

  ::testing::AssertionResult HasOp(Expression *e, Operator op) {
    if (e->op() == op)
      return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure()
           << to_string(e->op()) << " was not: " << to_string(op) << "; Expr: " << e->ToString();
  }

  ::testing::AssertionResult HasFactor(Expression* e, std::string expected, const std::string& lr) {
    auto* f = dynamic_cast<Factor*>(e);
    if (!f)
      return ::testing::AssertionFailure() << "missing " << lr << " factor: " << expected;
    auto val = f->value();
    if (val == expected) {
      return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure()
           << expected << " != " << val << "; Expr: " << e->ToString();
  }
  ::testing::AssertionResult HasLeftFactor(Expression* e, std::string expected) {
    return HasFactor(e->left(), expected, "left");
  }
  ::testing::AssertionResult HasRightFactor(Expression* e, std::string expected) {
    return HasFactor(e->right(), expected, "right");
  }
  ::testing::AssertionResult HasExpression(Expression* e, std::string l, Operator op,
                                           std::string r) {
    if (auto has_op = HasOp(e, op); !has_op) {
      return has_op;
    }
    if (auto has_left = HasLeftFactor(e, l); !has_left) {
      return has_left;
    }
    if (auto has_right = HasRightFactor(e, r); !has_right) {
      return has_right;
    }
    return ::testing::AssertionSuccess();
  }
};

TEST_F(AstTest, Expr_Add) {
  const Lexer l("1+2"); 

  Ast ast;
  ASSERT_TRUE(ast.parse(l));
  auto* root = dynamic_cast<Expression*>(ast.root());
  VLOG(1) << root->ToString();

  EXPECT_TRUE(HasLeftFactor(root, "1"));
  EXPECT_TRUE(HasOp(root, Operator::add));
  EXPECT_TRUE(HasRightFactor(root, "2"));
}

TEST_F(AstTest, Expr_Eq) {
  Lexer l("user.sl>200");

  Ast ast;
  ASSERT_TRUE(ast.parse(l));
  auto* root = dynamic_cast<Expression*>(ast.root());
  VLOG(1) << root->ToString();

  EXPECT_TRUE(HasLeftFactor(root, "user.sl"));
  EXPECT_TRUE(HasOp(root, Operator::gt));
  EXPECT_TRUE(HasRightFactor(root, "200"));

  EXPECT_TRUE(HasExpression(root, "user.sl", Operator::gt, "200"));
}

TEST_F(AstTest, Expr_Parens) {
  Lexer l("(user.sl>200) || user.ar == 'A'");

  Ast ast;
  ASSERT_TRUE(ast.parse(l));
  auto* root = ast.root();
  VLOG(1) << root->ToString();

  auto* expr = dynamic_cast<Expression*>(root);
  CHECK_NOTNULL(expr);
  EXPECT_TRUE(HasOp(expr, Operator::logical_or)) << root->ToString();
  ASSERT_NE(nullptr, expr);

  auto* left = expr->left();
  VLOG(1) << "Left: " << left->ToString();
  EXPECT_TRUE(HasExpression(left, "user.sl", Operator::gt, "200"));
  auto* right = expr->right();
  VLOG(1) << "Right: " << right->ToString();
  EXPECT_TRUE(HasExpression(right, "user.ar", Operator::eq, "A"));
}


TEST_F(AstTest, Visitor) {
  Lexer l("((user.sl>200) || user.ar == 'A') || user.sl == 255");

  class PrintVisitor : public AstVisitor {
  public:
    void visit(AstNode* n) override {
      LOG(INFO) << "Visit AstNode: " << n->ToString();
    };

    void visit(Expression* n) override {
      LOG(INFO) << "Visit Expression: " << n->ToString(false);
      ++count;
    };

    void visit(Factor* n) override {
      LOG(INFO) << "Visit Factor: " << n->ToString();
      ++count;
    };
    int count{0};
  };

  Ast ast;
  ASSERT_TRUE(ast.parse(l));
  auto* root = ast.root();
  PrintVisitor v{};
  root->accept(&v);

  EXPECT_EQ(11, v.count);

  LOG(INFO) << "======================================================================";
  LOG(INFO) << root->ToString();
}
