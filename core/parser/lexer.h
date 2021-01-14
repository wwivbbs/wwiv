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
#ifndef INCLUDED_WWIV_CORE_LEXER_H
#define INCLUDED_WWIV_CORE_LEXER_H

#include "core/parser/token.h"
#include <string>
#include <vector>

namespace wwiv::core::parser {

struct LexerState {
  bool ok{true};
  std::string err;
};

class Lexer final {
public:
  explicit Lexer(std::string source);
  void comment();
  void character();
  void string();
  void identifier();
  void number();

  Token& next();
  bool ok();

  const std::vector<Token>& tokens() const;

  std::vector<Token> tokens_;
  std::string source_;
  LexerState state_;
  Token tok_eof;
  std::string::iterator start;
  std::string::iterator end;
  std::string::iterator it_;
  decltype(tokens_)::iterator token_iter_;

private:
  void emit(TokenType);
  void emit(TokenType, std::string);
  void error(std::string message);
};

std::ostream& operator<<(std::ostream& os, const Lexer& l);

} // namespace wwiv::core::parser

#endif
