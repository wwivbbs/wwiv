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
#ifndef INCLUDED_WWIV_CORE_TOKEN_H
#define INCLUDED_WWIV_CORE_TOKEN_H

#include <string>

namespace wwiv::core::parser {

enum class TokenType {
  lparen,     //  (
  rparen,     //  )
  eq,         //  ==
  assign,     //  =
  ne,         //  !=
  negate,     //  !
  gt,         //  >
  ge,         //  >=
  lt,         //  <
  le,         //  <
  add,        //  +
  sub,        //  -
  mul,        //  *
  div,        //  /
  semicolon,  //  ;
  comment,    //  /* 
  character,  //  '?'
  string,     // "???"
  eof,        // EOF of ^Z
  identifier, // [A-Za-z0-9.]
  number,     // [0-9]
  logical_and, // &&
  logical_or,  // ||
  error
};

class Token final {
public:
  Token(TokenType t, int pos);
  Token(TokenType t, std::string l, int pos);
  TokenType type;
  std::string lexeme;
  int pos;
};

std::ostream& operator<<(std::ostream& os, const TokenType& a);
std::ostream& operator<<(std::ostream& os, const Token& a);
std::string to_string(TokenType t);
std::string to_string(Token t);

} // namespace wwiv::core

#endif
