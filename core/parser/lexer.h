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
#ifndef __INCLUDED_WWIV_CORE_LEXER_H__
#define __INCLUDED_WWIV_CORE_LEXER_H__

#include "core/parser/token.h"
#include <optional>
#include <string>

namespace wwiv::core::parser {

struct LexerState {
  bool ok;
  std::string err;
};

class Lexer final {
public:
  Lexer(std::string source);
  void comment(std::string::iterator& it);
  void character(std::string::iterator& it);
  void string(std::string::iterator& it);
  void identifier(std::string::iterator& it);
  void number(std::string::iterator& it);

  Token& next();
  bool ok();

  const std::vector<Token>& tokens() const;

  std::vector<Token> tokens_;
  std::string source_;
  LexerState state_;
  Token tok_eof;
  std::string::iterator start;
  std::string::iterator end;
  decltype(tokens_)::iterator iter_;

private:
  void emit(TokenType);
  void emit(TokenType, std::string);
  void error(std::string::iterator& it, std::string message);
};

} // namespace wwiv::core::parser

#endif
