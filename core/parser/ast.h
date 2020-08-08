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
#ifndef __INCLUDED_WWIV_CORE_AST_H__
#define __INCLUDED_WWIV_CORE_AST_H__

#include "core/parser/token.h"
#include <array>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <vector>

namespace wwiv::core::parser {

enum class AstType {
  LOGICAL_OP,
  BINOP,
  UNOP,
  PAREN,
  EXPR,
  FACTOR, 
  TAUTOLOGY,
  ERROR,
  ROOT
};


class AstNode {
public:
  AstNode(AstType t) : type_(t) {}
  virtual ~AstNode() = default;

  virtual std::string ToString();
  const AstType type_;
};

enum class Operator { add, sub, mul, div, gt, ge, lt, le, eq, ne, or, and };

enum class FactorType { int_value, string_val, variable };

class Factor : public AstNode {
protected:
  Factor(FactorType t, int v, const std::string& s);

public:
  virtual std::string ToString() override;

  FactorType type;
  int val;
  std::string sval;
};

class Number : public Factor {
public:
  Number(int v) : Factor(FactorType::int_value, v, "") {}
};

class String : public Factor {
public:
  String(std::string s) : Factor(FactorType::string_val, -1, std::move(s)) {}
};

class Variable : public Factor {
public:
  Variable(std::string s) : Factor(FactorType::variable, -1, std::move(s)) {}
};

class Expression : public AstNode {
public:
  Expression() : AstNode(AstType::EXPR) {}

  virtual std::string ToString() override;
  Operator op;
  std::unique_ptr<Factor> left_factor;
  std::unique_ptr<Expression> left_expression;
  std::unique_ptr<Factor> right_factor;
  std::unique_ptr<Expression> right_expression;
};

class RootNode : public AstNode {
public:
  RootNode(std::unique_ptr<AstNode>&& n) : AstNode(AstType::ROOT), node(std::move(n)) {}
  virtual std::string ToString() override;

  std::unique_ptr<AstNode> node;
};

class TautologyNode : public AstNode {
public:
  TautologyNode() : AstNode(AstType::TAUTOLOGY) {}
};

class ErrorNode : public AstNode {
public:
  ErrorNode(const std::string& m) : AstNode(AstType::ERROR), message(m) {}
  std::string message;
};

class BinaryOperatorNode : public AstNode {
public:
  BinaryOperatorNode(Operator o) : AstNode(AstType::BINOP), oper(o) {}
  virtual std::string ToString() override;
  Operator oper;
};

class LogicalOperatorNode : public AstNode {
public:
  LogicalOperatorNode(Operator o) : AstNode(AstType::LOGICAL_OP), oper(o) {}
  virtual std::string ToString() override;
  Operator oper;
};

class Ast final {
public:
  Ast() = default;

  std::unique_ptr<AstNode> parse(std::vector<Token>::iterator& begin,
                                        const std::vector<Token>::iterator& end);
  //const std::stack<std::unique_ptr<AstNode>>& stk() { return stack; }

private:
  std::unique_ptr<AstNode> parseExpression(std::vector<Token>::iterator& begin,
                                           const std::vector<Token>::iterator& end);
  std::unique_ptr<AstNode> parseGroup(std::vector<Token>::iterator& begin,
                                      const std::vector<Token>::iterator& end);
  bool need_reduce(const std::stack<std::unique_ptr<AstNode>>& stack);
  void reduce(std::stack<std::unique_ptr<AstNode>>& stack);
};

} // namespace wwiv::core

#endif
