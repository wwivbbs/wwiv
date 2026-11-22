
// Generated from BasicLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"


#ifndef _WIN32
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif


namespace wwivbasic {


class  BasicLexer : public antlr4::Lexer {
public:
  enum {
    DUMMY = 1, REAL = 2, INT = 3, TRUE = 4, FALSE = 5, LT = 6, LE = 7, GT = 8, 
    GE = 9, EQ = 10, NE = 11, AND = 12, OR = 13, NOT = 14, LET = 15, MOD = 16, 
    COLON = 17, SEMICOLON = 18, PLUS = 19, MINUS = 20, STAR = 21, SLASH = 22, 
    LPAREN = 23, RPAREN = 24, COMMA = 25, DOT = 26, AT = 27, DEF = 28, ENDDEF = 29, 
    RETURN = 30, IF = 31, THEN = 32, ELSE = 33, ELSEIF = 34, ENDIF = 35, 
    FOR = 36, NEXT = 37, STEP = 38, TO = 39, IN = 40, WHILE = 41, WEND = 42, 
    DO = 43, UNTIL = 44, EXIT = 45, BREAK = 46, END = 47, PRINT = 48, MODULE = 49, 
    IMPORT = 50, STRING = 51, ID = 52, COMMENT = 53, WS = 54, NEWLINE = 55
  };

  enum {
    CommentsChannel = 2, DirectiveChannel = 3
  };

  explicit BasicLexer(antlr4::CharStream *input);

  ~BasicLexer() override;


  std::string getGrammarFileName() const override;

  const std::vector<std::string>& getRuleNames() const override;

  const std::vector<std::string>& getChannelNames() const override;

  const std::vector<std::string>& getModeNames() const override;

  const antlr4::dfa::Vocabulary& getVocabulary() const override;

  antlr4::atn::SerializedATNView getSerializedATN() const override;

  const antlr4::atn::ATN& getATN() const override;

  // By default the static state used to implement the lexer is lazily initialized during the first
  // call to the constructor. You can call this function if you wish to initialize the static state
  // ahead of time.
  static void initialize();

private:

  // Individual action functions triggered by action() above.

  // Individual semantic predicate functions triggered by sempred() above.

};

}  // namespace wwivbasic
