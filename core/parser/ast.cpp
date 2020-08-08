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
#include "core/parser/ast.h"

#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include <array>
#include <iomanip>
#include <ios>
#include <memory>
#include <random>
#include <sstream>
#include <stack>

using namespace wwiv::strings;

namespace wwiv::core::parser {

static std::unique_ptr<Factor> createFactor(const Token& token) {
  const auto& l = token.lexmeme;

  if (token.type == TokenType::string || token.type == TokenType::character) {
    return std::make_unique<String>(l);
  } else if (isdigit(l.front())) {
    return std::make_unique<Number>(to_number<int>(l));
  } else {
    return std::make_unique<Variable>(l);
  }
}

static std::unique_ptr<LogicalOperatorNode> createLogicalOperator(const Token& token) {
  switch (token.type) {
  case TokenType::or:
    return std::make_unique<LogicalOperatorNode>(Operator::or);
  case TokenType::and:
    return std::make_unique<LogicalOperatorNode>(Operator::and);
  }
  LOG(ERROR) << "Should never happen: " << static_cast<int>(token.type) << ": " << token.lexmeme;
  return {};
}

    static std::unique_ptr<BinaryOperatorNode> createBinaryOperator(const Token& token) {
  switch (token.type) {
  case TokenType::add:
    return std::make_unique<BinaryOperatorNode>(Operator::add);
  case TokenType::assign:
    LOG(FATAL) << "return std::make_unique<BinaryOperatorNode>(Operator::ass);";
  case TokenType::div:
    return std::make_unique<BinaryOperatorNode>(Operator::div);
  case TokenType::eq:
    return std::make_unique<BinaryOperatorNode>(Operator::eq);
  case TokenType::ge:
    return std::make_unique<BinaryOperatorNode>(Operator::ge);
  case TokenType::gt:
    return std::make_unique<BinaryOperatorNode>(Operator::gt);
  case TokenType::le:
    return std::make_unique<BinaryOperatorNode>(Operator::le);
  case TokenType::lt:
    return std::make_unique<BinaryOperatorNode>(Operator::lt);
  case TokenType::mul:
    return std::make_unique<BinaryOperatorNode>(Operator::mul);
  case TokenType::ne:
    return std::make_unique<BinaryOperatorNode>(Operator::ne);
  case TokenType::sub:
    return std::make_unique<BinaryOperatorNode>(Operator::sub);
  }
  LOG(ERROR) << "Should never happen: " << static_cast<int>(token.type) << ": " << token.lexmeme;
  return {};
}

void Ast::reduce(std::stack<std::unique_ptr<AstNode>>& stack) {
  auto expr = std::make_unique<Expression>();
  auto right = std::move(stack.top());
  stack.pop();
  auto op = std::move(stack.top());
  stack.pop();
  auto left = std::move(stack.top());
  stack.pop();

  // Set op
  if (op->type_ == AstType::BINOP) {
    auto oper = dynamic_cast<BinaryOperatorNode*>(op.get());
    expr->op = oper->oper;
  } else if (op->type_ == AstType::LOGICAL_OP) {
    auto oper = dynamic_cast<LogicalOperatorNode*>(op.get());
    expr->op = oper->oper;
  } else {
    stack.push(
        std::make_unique<ErrorNode>(StrCat("Expected BINOP at: ", static_cast<int>(op->type_))));
    return;
  }
  // Set left
  if (left->type_ == AstType::FACTOR) {
    auto factor = dynamic_cast<Factor*>(left.release());
    expr->left_factor = std::unique_ptr<Factor>(factor);
  } else if (left->type_ == AstType::EXPR) {
    auto factor = dynamic_cast<Expression*>(left.release());
    expr->left_expression = std::unique_ptr<Expression>(factor);
  }
  // Set right
  if (right->type_ == AstType::FACTOR) {
    auto factor = dynamic_cast<Factor*>(right.release());
    expr->right_factor = std::unique_ptr<Factor>(factor);
  } else if (right->type_ == AstType::EXPR) {
    auto factor = dynamic_cast<Expression*>(right.release());
    expr->right_expression = std::unique_ptr<Expression>(factor);
  }
  stack.push(std::move(expr));
}

std::unique_ptr<AstNode> Ast::parseExpression(std::vector<Token>::iterator& it,
                                              const std::vector<Token>::iterator& end) {
  std::stack<std::unique_ptr<AstNode>> stack;
  for (; it != end; it++) {
    switch (it->type) { 
    case TokenType::rparen: {
      // If we have a right parens, we are no longer in an expression.
      // exit now before advancing it;
      auto ret = std::move(stack.top());
      stack.pop();
      return ret;
    }
    case TokenType::comment:
      // skip
      LOG(INFO) << "comment: " << it->lexmeme;
      break;
    case TokenType::string:
    case TokenType::character:
    case TokenType::number:
    case TokenType::identifier: {
      const auto l = it->lexmeme;
      if (l.empty()) {
        return std::make_unique<ErrorNode>(
            StrCat("Unable to parse expression starting at: ", it->lexmeme));
      }
      const auto nr = need_reduce(stack);
      stack.push(createFactor(*it));
      if (nr) {
        // One identifier and one operator
        reduce(stack);
      }
    } break;
    case TokenType::add:
    case TokenType::assign:
    case TokenType::div:
    case TokenType::eq:
    case TokenType::ge:
    case TokenType::gt:
    case TokenType::le:
    case TokenType::lt:
    case TokenType::mul:
    case TokenType::ne:
    case TokenType::sub: {
      stack.push(createBinaryOperator(*it));
    } break;
    case TokenType::or: 
    case TokenType::and: {
      stack.push(createLogicalOperator(*it));
    } break;
    }
  }
  auto ret = std::move(stack.top());
  stack.pop();
  return ret;
}

std::unique_ptr<AstNode> Ast::parseGroup(std::vector<Token>::iterator& it,
                                         const std::vector<Token>::iterator& end) {
  if ((it+1) == end) {
    return std::make_unique<ErrorNode>(
        StrCat("Unable to parse expression starting at: ", it->lexmeme));
  }
  // Skip lparen
  ++it;
  auto expr = parseExpression(it, end);
  if (!expr) {
    return std::make_unique<ErrorNode>(
        StrCat("Unable to parse expression starting at: ", it->lexmeme));
  }
  if (it == end || it->type != TokenType::rparen) {
    LOG(ERROR) << "Missing right parens";
    auto pos = it == end ? "end" : it->lexmeme;
    return std::make_unique<ErrorNode>(StrCat("Unable to parse expression starting at: ", pos));
  }
  return expr;
}

bool Ast::need_reduce(const std::stack<std::unique_ptr<AstNode>>& stack) {
  if (stack.empty()) {
    return false;
  }
  const auto t = stack.top()->type_;
  return t == AstType::BINOP || t == AstType::LOGICAL_OP;
}  

std::unique_ptr<RootNode> Ast::parse(
    std::vector<Token>::iterator& begin, const std::vector<Token>::iterator& end) {
  std::stack<std::unique_ptr<AstNode>> stack;
  for (auto it = begin; it != end;) {
    const auto& t = *it;
    switch (t.type) { 
    case TokenType::lparen: {
      auto expr = parseGroup(it, end);
      if (!expr || it == end) {
        return std::make_unique<RootNode>(std::make_unique<ErrorNode>(
            StrCat("Unable to parse expression starting at: ", it->lexmeme)));
      }
      stack.push(std::move(expr));
      } break;

    case TokenType::and:
    case TokenType::or: {
      stack.push(createLogicalOperator(*it));
    } break;

    default: {
      // Try to reduce.
      bool nr = need_reduce(stack);
      auto expr = parseExpression(it, end);
      if (!expr) {
        return std::make_unique<RootNode>(std::make_unique<ErrorNode>(
            StrCat("Unable to parse expression starting at: ", it->lexmeme)));
      }
      stack.push(std::move(expr));
      if (nr) {
        reduce(stack);
      }
    } break;
    }

    if (it != end) {
      ++it;
    }
  }
  auto root = std::make_unique<RootNode>(std::move(stack.top()));
  stack.pop();
  return root;
}

Factor::Factor(FactorType t, int v, const std::string& s)
    : AstNode(AstType::FACTOR), type(t), val(v), sval(s) {}

std::string to_string(FactorType t) {
  switch (t) { 
  case FactorType::int_value:
    return "FactorType::int_value";
  case FactorType::string_val:
    return "FactorType::string_val";
  case FactorType::variable:
    return "FactorType::variable";
  }
  return "FactorType::UNKNOWN";
}

std::string to_string(Operator o) {
  switch (o) {
  case Operator::add:
    return "Operator::add";
  case Operator::div:
    return "Operator::div";
  case Operator::eq:
    return "Operator::eq";
  case Operator::ge:
    return "Operator::ge";
  case Operator::gt:
    return "Operator::gt";
  case Operator::le:
    return "Operator::le";
  case Operator::mul:
    return "Operator::mul";
  case Operator::ne:
    return "Operator::ne";
  case Operator::sub:
    return "Operator::sub";
  case Operator::or:
    return "Operator::or";
  case Operator::and:
    return "Operator::and";
  default:
    return "Operator::UNKNOWN";
  }
}


std::string Factor::ToString() { 
  return fmt::format("Factor: {} {}", to_string(type),
                      type == FactorType::int_value ? std::to_string(val) : sval);
}

std::string to_string(AstType t) {
  switch (t) {
  case AstType::LOGICAL_OP:
    return "AstType::LOGICAL_OP";
  case AstType::BINOP:
    return "AstType: binop";
  case AstType::UNOP:
    return "AstType: unop";
  case AstType::PAREN:
    return "AstType: paren";
  case AstType::EXPR:
    return "AstType: expr";
  case AstType::FACTOR:
    return "AstType: factor";
  case AstType::TAUTOLOGY:
    return "AstType: tautology";
  case AstType::ERROR:
    return "AstType: error";
  case AstType::ROOT:
    return "AstType: root";
  }
  return "AstType::unknown";
}


std::string AstNode::ToString() { 
  return fmt::format("AstNode: {}", to_string(type_));
}

std::string Expression::ToString() { 
  std::ostringstream ss;
  ss << AstNode::ToString() << " [";
  if (left_factor) {
    ss << "Expression: \n";
    ss << "LF: " << left_factor->ToString() << "\n";
  }
  if (left_expression) {
    ss << "LE: " << left_expression->ToString() << "\n";
  }
  ss << "OP: " << to_string(op) << "\n";
  if (right_factor) {
    ss << "RF: " << right_factor->ToString() << "\n";
  }
  if (right_expression) {
    ss << "RE: " << right_expression->ToString() << "\n";
  }
  return ss.str();
}

std::string RootNode::ToString() { return fmt::format("ROOT: {}\n", node->ToString()); }


std::string BinaryOperatorNode::ToString() { 
  return fmt::format("BinaryOperatorNode: ", to_string(oper));
}

std::string LogicalOperatorNode::ToString() {
  return fmt::format("LogicalOperatorNode: ", to_string(oper));
}

} // namespace wwiv::core::parser
