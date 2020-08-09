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
#include "core/parser/lexer.h"

#include "core/stl.h"
#include "fmt/format.h"
#include <array>
#include <iomanip>
#include <ios>
#include <sstream>
#include <unordered_set>

enum class lexer_state_t { normal, in_char, in_string, in_mult_comment };

namespace wwiv::core::parser {

std::optional<char> peek(std::string::iterator& it, const std::string::iterator& end) {
  if (it == end) {
    return std::nullopt;
  }
  if (it + 1 == end) {
    return std::nullopt;
  }
  return *(it + 1);
}

static std::unordered_set<char> WS = {' ', '\r', '\n', '\t'};

Lexer::Lexer(std::string source)
    : source_(std::move(source)), tok_eof(TokenType::eof), start(std::begin(source_)),
      end(std::end(source_)) { 
  auto state = lexer_state_t::normal;

  auto it = start;
  while (it != end) {
    auto c = *it;
    switch (c) {
    case '(':
      emit(TokenType::lparen);
      break;
    case ')':
      emit(TokenType::rparen);
      break;
    case '=': {
      if (peek(it, end) == '=') {
        emit(TokenType::eq);
        ++it;
      } else {
        emit(TokenType::assign);
      }
    } break;
    case '!': {
      if (peek(it, end) == '=') {
        emit(TokenType::ne);
        ++it;
      } else {
        emit(TokenType::negate);
      }
    } break;
    case '>': {
      if (peek(it, end) == '=') {
        emit(TokenType::ge);
        ++it;
      } else {
        emit(TokenType::gt);
      }
    } break;
    case '<': {
      if (peek(it, end) == '=') {
        emit(TokenType::le);
        ++it;
      } else {
        emit(TokenType::lt);
      }
    } break;
    case '+':
      emit(TokenType::add);
      break;
    case '-':
      emit(TokenType::sub);
      break;
    case '*':
      emit(TokenType::mul);
      break;
    case '/':
      if (peek(it, end) == '*') {
        ++it; // skip / and *
        comment(++it);
      } else {
        emit(TokenType::div);
      }
      break;
    case '\'': // char
      character(++it);
      break;
    case '"': // string
      string(++it);
      break;
    case ';':
      emit(TokenType::semicolon);
      break;
    case '&': {
      if (peek(it, end) == '&') {
        emit(TokenType::logical_and);
        ++it;
      } else {
        error(it, "& is not a valid token; did you mean '&&'?");
      }
    } break;
    case '|': {
      if (peek(it, end) == '|') {
        emit(TokenType::logical_or);
        ++it;
      } else {
        error(it, "| is not a valid token; did you mean '||'?");
      }
    } break;
    default: {
      if (wwiv::stl::contains(WS, c)) {
        // skip WS
        break;
      } else if (isalpha(*it)) {
        identifier(it);
      } else if (isdigit(*it)) {
        number(it);
      }
    } break;
    }

    // advance iterator
    if (it == end) {
      break;
    }
    ++it;
  }

  iter_ = std::begin(tokens_);
}

void Lexer::comment(std::string::iterator& it) { 
  std::string tx;
  while ( it != end) {
    if (*it == '*' && peek(it, end) == '/') {
      // end if comment.
      ++it; // skip end of comment.
      emit(TokenType::comment, tx);
      return;
    }
    tx.push_back(*it++);
  }
}

void Lexer::string(std::string::iterator& it) {
  std::string tx;
  while (it != end) {
    if (*it == '"') {
      // end if comment.
      emit(TokenType::string, tx);
      return;
    }
    tx.push_back(*it++);
  }
  if (!tx.empty()) {
    error(it, "EOF before end of string");
  }
}

void Lexer::character(std::string::iterator& it) {
  std::string tx;
  while (it != end) {
    if (*it == '\'') {
      // end if comment.
      if (tx.size() == 1) {
        emit(TokenType::character, tx);
        return;
      } else {
        error(it, fmt::format("Expected a single character, got '{}'"));
        return;
      }
      return;
    }
    tx.push_back(*it++);
  }
  if (!tx.empty()) {
    error(it, "EOF before end of character");
  }
}

void Lexer::identifier(std::string::iterator& it) {
  std::string tx;
  while (it != end) {
    const auto c = *it;
    if (!isalnum(c) && c != '.') {
      // end if identifier.
      if (it != start) {
        // put back token
        --it;
      }
      emit(TokenType::identifier, tx);
      return;
    }
    tx.push_back(*it++);
  }
  if (!tx.empty()) {
    emit(TokenType::identifier, tx);
  }
}

void Lexer::number(std::string::iterator& it) {
  std::string tx;
  while (it != end) {
    const auto c = *it;
    if (!isdigit(c)) {
      // end if identifier.
      if (it != start) {
        // put back token
        --it;
      }
      emit(TokenType::number, tx);
      return;
    }
    tx.push_back(*it++);
  }
  if (!tx.empty()) {
    emit(TokenType::number, tx);
  }
}

Token& Lexer::next() {
  if (iter_ == std::end(tokens_)) {
    return tok_eof;
  }
  return *iter_++;
}

bool Lexer::ok() { return state_.ok; }

const std::vector<Token>& Lexer::tokens() const { return tokens_; }

void Lexer::emit(TokenType t) { 
  tokens_.emplace_back(t);
}

void Lexer::emit(TokenType t, std::string l ) { 
  tokens_.emplace_back(t, std::move(l)); 
}

void Lexer::error(std::string::iterator& it, std::string message) { 
  auto d = std::distance(start, it);
  auto msg = fmt::format("Error at pos: '{}', {}", d, message);
  emit(TokenType::error, msg);
}

} // namespace wwiv::core::parser