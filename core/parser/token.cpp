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
#include "core/parser/token.h"

#include "core/stl.h"
#include "fmt/format.h"
#include <array>
#include <iomanip>
#include <ios>
#include <random>
#include <sstream>

namespace wwiv::core::parser {

Token::Token(TokenType t, int p) : type(t), pos(p) {}

Token::Token(TokenType t, std::string l, int p) : type(t), lexeme(l), pos(p) {}

std::ostream& operator<<(std::ostream& os, const TokenType& t) {
  os << to_string(t);
  return os;
}

std::ostream& operator<<(std::ostream& os, const Token& t) {
  os << to_string(t);
  return os;
}

std::string to_string(TokenType t) { 
  switch (t) {
  case TokenType::lparen:      //  (
    return "lparen";
  case TokenType::rparen:      //  )
    return "rparen";
  case TokenType::eq:          //  ==
    return "eq";
  case TokenType::assign:      //  =
    return "assign";
  case TokenType::ne:          //  !=
    return "ne";
  case TokenType::negate:      //  !
    return "negate";
  case TokenType::gt:          //  >
    return "gt";
  case TokenType::ge:          //  >=
    return "ge";
  case TokenType::lt:          //  <
    return "lt";
  case TokenType::le:          //  <
    return "le";
  case TokenType::add:         //  +
    return "add";
  case TokenType::sub:         //  -
    return "sub";
  case TokenType::mul:         //  *
    return "mul";
  case TokenType::div:         //  /
    return "div";
  case TokenType::semicolon:   //  ;
    return "semicolon";
  case TokenType::comment:     //  /*
    return "comment";
  case TokenType::character:   //  '?'
    return "character";
  case TokenType::string:      // "???"
    return "string";
  case TokenType::eof:         // EOF of ^Z
    return "eof";
  case TokenType::identifier:  // [A-Za-z0-9.]
    return "identifier";
  case TokenType::number:      // [0-9]
    return "number";
  case TokenType::logical_and: // &&
    return "logical_and";
  case TokenType::logical_or: // ||
    return "logical_or";
  case TokenType::error:
    return "error";
  }
  return "**UNKNOWN**";
}

std::string to_string(Token t) {
  return fmt::format("[Token: {}({}), pos: {}]", to_string(t.type), t.lexeme, t.pos);
}

} 
