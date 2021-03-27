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
#include "core/parser/lexer.h"

#include "core/stl.h"
#include "fmt/format.h"
#include <array>
#include <iomanip>
#include <ios>
#include <sstream>
#include <unordered_set>

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
    : source_(std::move(source)), tok_eof(TokenType::eof, wwiv::stl::size_int(source_)), 
      start(std::begin(source_)), end(std::end(source_)), it_(start) { 

  while (it_ != end) {
    auto c = *it_;
    switch (c) {
    case '(':
      emit(TokenType::lparen);
      break;
    case ')':
      emit(TokenType::rparen);
      break;
    case '=': {
      if (peek(it_, end) == '=') {
        emit(TokenType::eq);
        ++it_;
      } else {
        emit(TokenType::assign);
      }
    } break;
    case '!': {
      if (peek(it_, end) == '=') {
        emit(TokenType::ne);
        ++it_;
      } else {
        emit(TokenType::negate);
      }
    } break;
    case '>': {
      if (peek(it_, end) == '=') {
        emit(TokenType::ge);
        ++it_;
      } else {
        emit(TokenType::gt);
      }
    } break;
    case '<': {
      if (peek(it_, end) == '=') {
        emit(TokenType::le);
        ++it_;
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
      if (peek(it_, end) == '*') {
        ++it_; // skip / and *
        ++it_; // skip / and *
        comment();
      } else {
        emit(TokenType::div);
      }
      break;
    case '\'': // char
      ++it_;   // skip '
      character();
      break;
    case '"': // string
      ++it_;  // skip "
      string();
      break;
    case ';':
      emit(TokenType::semicolon);
      break;
    case '&': {
      if (peek(it_, end) == '&') {
        emit(TokenType::logical_and);
        ++it_;
      } else {
        error("& is not a valid token; did you mean '&&'?");
      }
    } break;
    case '|': {
      if (peek(it_, end) == '|') {
        emit(TokenType::logical_or);
        ++it_;
      } else {
        error("| is not a valid token; did you mean '||'?");
      }
    } break;
    default: {
      if (wwiv::stl::contains(WS, c)) {
        // skip WS
        break;
      }
      if (isalpha(*it_)) {
        identifier();
      } else if (isdigit(*it_)) {
        number();
      }
    } break;
    }

    // advance iterator
    if (it_ == end) {
      break;
    }
    ++it_;
  }

  token_iter_ = std::begin(tokens_);
}

void Lexer::comment() { 
  std::string tx;
  while ( it_ != end) {
    if (*it_ == '*' && peek(it_, end) == '/') {
      // end if comment.
      ++it_; // skip end of comment.
      emit(TokenType::comment, tx);
      return;
    }
    tx.push_back(*it_++);
  }
}

void Lexer::string() {
  std::string tx;
  while (it_ != end) {
    if (*it_ == '"') {
      // end if comment.
      emit(TokenType::string, tx);
      return;
    }
    tx.push_back(*it_++);
  }
  if (!tx.empty()) {
    error("EOF before end of string");
  }
}

void Lexer::character() {
  std::string tx;
  while (it_ != end) {
    if (*it_ == '\'') {
      // end if comment.
      if (tx.size() == 1) {
        emit(TokenType::character, tx);
        return;
      } else {
        error(fmt::format("Expected a single character, got '{}'"));
        return;
      }
      return;
    }
    tx.push_back(*it_++);
  }
  if (!tx.empty()) {
    error("EOF before end of character");
  }
}

void Lexer::identifier() {
  std::string tx;
  while (it_ != end) {
    const auto c = *it_;
    if (!isalnum(c) && c != '.' && c != '_' && c != '-') {
      // end if identifier.
      if (it_ != start) {
        // put back token
        --it_;
      }
      emit(TokenType::identifier, tx);
      return;
    }
    tx.push_back(*it_++);
  }
  if (!tx.empty()) {
    emit(TokenType::identifier, tx);
  }
}

void Lexer::number() {
  std::string tx;
  while (it_ != end) {
    const auto c = *it_;
    if (!isdigit(c)) {
      // end if identifier.
      if (it_ != start) {
        // put back token
        --it_;
      }
      emit(TokenType::number, tx);
      return;
    }
    tx.push_back(*it_++);
  }
  if (!tx.empty()) {
    emit(TokenType::number, tx);
  }
}

Token& Lexer::next() {
  if (token_iter_ == std::end(tokens_)) {
    return tok_eof;
  }
  return *token_iter_++;
}

bool Lexer::ok() { 
  if (!state_.ok) {
    VLOG(1) << "Lexer state !ok: " << state_.err;
  }
  return state_.ok;
}

const std::vector<Token>& Lexer::tokens() const { return tokens_; }

void Lexer::emit(TokenType t) { 
  VLOG(1) << "Emitting TokenType: " << static_cast<int>(t);
  const auto pos = static_cast<int>(std::distance(start, it_));
  tokens_.emplace_back(t, pos);
}

void Lexer::emit(TokenType t, std::string l) { 
  VLOG(1) << "Emitting TokenType: " << static_cast<int>(t) << "; l: '" << l << "'";
  const auto pos = std::distance(start, it_);
  tokens_.emplace_back(t, std::move(l), pos);
}

void Lexer::error(std::string message) { 
  const auto pos = std::distance(start, it_);
  const auto msg = fmt::format("Error at pos: '{}', {}", pos, message);
  emit(TokenType::error, msg);
  state_.ok = false;
  state_.err = msg;
}

std::ostream& operator<<(std::ostream& os, const Lexer& a) {
  os << "Lexer: expression: '" << a.source_ << "'; Tokens: ";
  for (const auto& t : a.tokens()) {
    os << t << ", ";
  }
  return os;
}

} // namespace wwiv::core::parser