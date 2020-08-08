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

#include "core/parser/lexer.h"
#include "core/parser/token.h"
#include <array>
#include <memory>
#include <optional>
#include <stack>
#include <stdexcept>
#include <string>
#include <vector>

namespace wwiv::core::parser {

class AstVisitor;

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
  AstNode(AstType t) : ast_type_(t) {}
  virtual ~AstNode() = default;
  AstType ast_type() const noexcept { return ast_type_; }
  virtual std::string ToString();
  virtual void accept(AstVisitor* visitor);

private:
  const AstType ast_type_;
};

enum class Operator { add, sub, mul, div, gt, ge, lt, le, eq, ne, or, and };

enum class FactorType { int_value, string_val, variable };

class Factor;

class Expression : public AstNode {
public:
  static int expression_id;
  Expression() : AstNode(AstType::EXPR), id_(++expression_id) {}

  Expression* left() { return left_.get(); }
  Expression* right() { return right_.get(); }
  Operator op() const noexcept { return op_; }
  int id() const noexcept { return id_; }
  virtual std::string ToString() override;
  virtual std::string ToString(bool include_children);
  virtual std::string ToString(bool include_children, int indent);
  virtual void accept(AstVisitor* visitor) override;

  // Used when constructing these.
  // TODO(rushfan): Move into constructor and construct these fully.
  Operator op_;
  std::unique_ptr<Expression> left_;
  std::unique_ptr<Expression> right_;
  int id_;

protected:
  // Constructor for subclasses of expressions to use to eval.
  Expression(AstType t) : AstNode(t), id_(++expression_id) {}
};

class Factor : public Expression {
protected:
  Factor(FactorType t, int v, const std::string& s);

public:
  virtual std::string ToString() override { return ToString(0); };
  virtual std::string ToString(int indent);
  FactorType factor_type() const noexcept { return factor_type_; }
  std::string value() const {
    return factor_type_ == FactorType::int_value ? std::to_string(val) : sval;
  }
  int int_value() const {
    return factor_type_ == FactorType::int_value ? val : -1;
  }
  virtual void accept(AstVisitor* visitor) override;

private:
  FactorType factor_type_;
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

struct parse_error : public std::runtime_error {
  parse_error(const std::string& m);
};

class AstVisitor {
public:
  virtual void visit(AstNode* n) = 0;
  virtual void visit(Expression* n) = 0;
  virtual void visit(Factor* n) = 0;
};

class Ast final {
public:
  Ast() = default;
  
  bool parse(const Lexer& l);
  AstNode* root();

private:
  std::unique_ptr<AstNode> parseExpression(std::vector<Token>::iterator& begin,
                                           const std::vector<Token>::iterator& end);
  std::unique_ptr<AstNode> parseGroup(std::vector<Token>::iterator& begin,
                                      const std::vector<Token>::iterator& end);
  std::unique_ptr<RootNode> parse(std::vector<Token>::iterator& begin,
                                  const std::vector<Token>::iterator& end);
  bool need_reduce(const std::stack<std::unique_ptr<AstNode>>& stack);
  void reduce(std::stack<std::unique_ptr<AstNode>>& stack);

  std::unique_ptr<RootNode> root_;
};

std::string to_string(Operator o);
std::string to_symbol(Operator o);

} // namespace wwiv::core

#endif
