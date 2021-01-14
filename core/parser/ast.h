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
#ifndef INCLUDED_WWIV_CORE_AST_H
#define INCLUDED_WWIV_CORE_AST_H

#include "core/parser/lexer.h"
#include "core/parser/token.h"
#include <memory>
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
  virtual std::string ToString() const;
  virtual void accept(AstVisitor* visitor);

private:
  const AstType ast_type_;
};

enum class Operator { add, sub, mul, div, gt, ge, lt, le, eq, ne, logical_or, logical_and, UNKNOWN };

enum class FactorType { int_value, string_val, variable };

class Factor;

//[[DebuggerDisplay("op={op_} l={left_} r={right_}")]]
class Expression : public AstNode {
public:
  static int expression_id;
  //Expression() : AstNode(AstType::EXPR), id_(++expression_id) {}
  Expression(std::unique_ptr<Expression>&& left, Operator op, std::unique_ptr<Expression>&& right);

  [[nodiscard]] Expression* left() const { return left_.get(); }
  [[nodiscard]] Expression* right() const { return right_.get(); }
  [[nodiscard]] Operator op() const noexcept { return op_; }
  [[nodiscard]] int id() const noexcept { return id_; }
  [[nodiscard]] std::string ToString() const override;
  [[nodiscard]] virtual std::string ToString(bool include_children) const;
  [[nodiscard]] virtual std::string ToString(bool include_children, int indent) const;
  void accept(AstVisitor* visitor) override;

  // Used when constructing these.
  // TODO(rushfan): Move into constructor and construct these fully.
  int id_;
  std::unique_ptr<Expression> left_;
  Operator op_;
  std::unique_ptr<Expression> right_;

protected:
  // Constructor for subclasses of expressions to use to eval.
  explicit Expression(AstType t) : AstNode(t), id_(++expression_id) {}
};

class Factor : public Expression {
protected:
  Factor(FactorType t, int v, const std::string& s);

public:
  [[nodiscard]] std::string ToString() const override { return ToString(0); };

  [[nodiscard]] virtual std::string ToString(int indent) const;

  [[nodiscard]] FactorType factor_type() const noexcept { return factor_type_; }

  [[nodiscard]] std::string value() const {
    return factor_type_ == FactorType::int_value ? std::to_string(val) : sval;
  }

  [[nodiscard]] int int_value() const {
    return factor_type_ == FactorType::int_value ? val : -1;
  }

  void accept(AstVisitor* visitor) override;

private:
  FactorType factor_type_;
  int val;
  std::string sval;
};

class Number final : public Factor {
public:
  explicit Number(int v) : Factor(FactorType::int_value, v, "") {}
};

class String final : public Factor {
public:
  explicit String(std::string s) : Factor(FactorType::string_val, -1, s) {}
};

class Variable final : public Factor {
public:
  explicit Variable(std::string s);
};

class RootNode final : public AstNode {
public:
  explicit RootNode(std::unique_ptr<AstNode>&& n) : AstNode(AstType::ROOT), node(std::move(n)) {}
  [[nodiscard]] std::string ToString() const override;

  std::unique_ptr<AstNode> node;
};

class TautologyNode final : public AstNode {
public:
  TautologyNode() : AstNode(AstType::TAUTOLOGY) {}
};

class ErrorNode final : public AstNode {
public:
  explicit ErrorNode(const std::string& m) : AstNode(AstType::ERROR), message(m) {}
  std::string message;
};

class OperatorNode : public AstNode {
protected:
  OperatorNode(Operator o, AstType t) : AstNode(t), oper(o) {}

public:
  [[nodiscard]] std::string ToString() const override = 0;
  const Operator oper;
};

class BinaryOperatorNode final : public OperatorNode {
public:
  explicit BinaryOperatorNode(Operator o) : OperatorNode(o, AstType::BINOP) {}
  std::string ToString() const override;
};

class LogicalOperatorNode final : public OperatorNode {
public:
  explicit LogicalOperatorNode(Operator o) : OperatorNode(o, AstType::LOGICAL_OP) {}
  std::string ToString() const override;
};

struct parse_error : std::runtime_error {
  explicit parse_error(const std::string& m);
};

class AstVisitor {
public:
  virtual ~AstVisitor() = default;
  virtual void visit(AstNode* n) = 0;
  virtual void visit(Expression* n) = 0;
  virtual void visit(Factor* n) = 0;
};

class Ast final {
public:
  Ast() = default;
  
  bool parse(const Lexer& l);
  AstNode* root() const;

private:
  [[nodiscard]] std::unique_ptr<AstNode> parseExpression(std::vector<Token>::iterator& begin,
                                                         const std::vector<Token>::iterator& end);
  [[nodiscard]] std::unique_ptr<AstNode> parseGroup(std::vector<Token>::iterator& begin,
                                                    const std::vector<Token>::iterator& end);
  [[nodiscard]] std::unique_ptr<RootNode> parse(std::vector<Token>::iterator& begin,
                                                const std::vector<Token>::iterator& end);
  bool need_reduce(const std::stack<std::unique_ptr<AstNode>>& stack, bool allow_logical);
  bool reduce(std::stack<std::unique_ptr<AstNode>>& stack);

  std::unique_ptr<RootNode> root_;
};

std::string to_string(Operator o);
std::string to_symbol(Operator o);
std::string to_string(const AstNode& n);

} // namespace wwiv::core

#endif
