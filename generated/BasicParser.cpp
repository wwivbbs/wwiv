
// Generated from BasicParser.g4 by ANTLR 4.13.2


#include "BasicParserVisitor.h"

#include "BasicParser.h"


#ifndef _WIN32
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif


using namespace antlrcpp;
using namespace wwivbasic;

using namespace antlr4;

namespace {

struct BasicParserStaticData final {
  BasicParserStaticData(std::vector<std::string> ruleNames,
                        std::vector<std::string> literalNames,
                        std::vector<std::string> symbolicNames)
      : ruleNames(std::move(ruleNames)), literalNames(std::move(literalNames)),
        symbolicNames(std::move(symbolicNames)),
        vocabulary(this->literalNames, this->symbolicNames) {}

  BasicParserStaticData(const BasicParserStaticData&) = delete;
  BasicParserStaticData(BasicParserStaticData&&) = delete;
  BasicParserStaticData& operator=(const BasicParserStaticData&) = delete;
  BasicParserStaticData& operator=(BasicParserStaticData&&) = delete;

  std::vector<antlr4::dfa::DFA> decisionToDFA;
  antlr4::atn::PredictionContextCache sharedContextCache;
  const std::vector<std::string> ruleNames;
  const std::vector<std::string> literalNames;
  const std::vector<std::string> symbolicNames;
  const antlr4::dfa::Vocabulary vocabulary;
  antlr4::atn::SerializedATNView serializedATN;
  std::unique_ptr<antlr4::atn::ATN> atn;
};

::antlr4::internal::OnceFlag basicparserParserOnceFlag;
#if ANTLR4_USE_THREAD_LOCAL_CACHE
static thread_local
#endif
std::unique_ptr<BasicParserStaticData> basicparserParserStaticData = nullptr;

void basicparserParserInitialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  if (basicparserParserStaticData != nullptr) {
    return;
  }
#else
  assert(basicparserParserStaticData == nullptr);
#endif
  auto staticData = std::make_unique<BasicParserStaticData>(
    std::vector<std::string>{
      "main", "eos", "procedureCall", "parameterList", "procedureDefinition", 
      "parameterDefinitionList", "importModule", "moduleDefinition", "statements", 
      "statement", "emptyStatement", "expr", "booleanExpr", "assignmentStatement", 
      "ifStatement", "forStatement", "forInStatement", "whileStatement", 
      "doUntilStatement", "printStatement", "printItem", "returnStatement", 
      "exitStatement", "endStatement", "lvalue", "id", "rvalue", "procedureName", 
      "qualifiedName", "callChain", "memberSuffix", "memberName"
    },
    std::vector<std::string>{
      "", "", "", "", "'TRUE'", "'FALSE'", "'<'", "'<='", "'>'", "'>='", 
      "'='", "'<>'", "'AND'", "'OR'", "'NOT'", "'LET'", "'MOD'", "':'", 
      "';'", "'+'", "'-'", "'*'", "'/'", "'('", "')'", "','", "'.'", "'@'", 
      "'DEF'", "'ENDDEF'", "'RETURN'", "'IF'", "'THEN'", "'ELSE'", "'ELSEIF'", 
      "'ENDIF'", "'FOR'", "'NEXT'", "'STEP'", "'TO'", "'IN'", "'WHILE'", 
      "'WEND'", "'DO'", "'UNTIL'", "'EXIT'", "'BREAK'", "'END'", "'PRINT'", 
      "'MODULE'", "'IMPORT'"
    },
    std::vector<std::string>{
      "", "DUMMY", "REAL", "INT", "TRUE", "FALSE", "LT", "LE", "GT", "GE", 
      "EQ", "NE", "AND", "OR", "NOT", "LET", "MOD", "COLON", "SEMICOLON", 
      "PLUS", "MINUS", "STAR", "SLASH", "LPAREN", "RPAREN", "COMMA", "DOT", 
      "AT", "DEF", "ENDDEF", "RETURN", "IF", "THEN", "ELSE", "ELSEIF", "ENDIF", 
      "FOR", "NEXT", "STEP", "TO", "IN", "WHILE", "WEND", "DO", "UNTIL", 
      "EXIT", "BREAK", "END", "PRINT", "MODULE", "IMPORT", "STRING", "ID", 
      "COMMENT", "WS", "NEWLINE"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,55,330,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
  	14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
  	21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
  	28,2,29,7,29,2,30,7,30,2,31,7,31,1,0,1,0,1,0,1,0,5,0,69,8,0,10,0,12,0,
  	72,9,0,1,0,1,0,1,1,3,1,77,8,1,1,1,1,1,1,2,1,2,1,2,3,2,84,8,2,1,2,1,2,
  	1,3,1,3,1,3,5,3,91,8,3,10,3,12,3,94,9,3,1,4,1,4,1,4,1,4,3,4,100,8,4,1,
  	4,1,4,1,4,1,4,1,4,1,4,1,5,1,5,1,5,5,5,111,8,5,10,5,12,5,114,9,5,1,6,1,
  	6,1,6,1,6,1,6,1,6,1,6,1,6,3,6,124,8,6,1,7,1,7,1,7,1,7,1,8,5,8,131,8,8,
  	10,8,12,8,134,9,8,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,
  	1,9,3,9,150,8,9,1,10,1,10,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,
  	1,11,1,11,1,11,1,11,1,11,1,11,3,11,169,8,11,1,11,1,11,1,11,1,11,1,11,
  	1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,5,11,186,8,11,10,11,
  	12,11,189,9,11,1,12,1,12,1,13,3,13,194,8,13,1,13,1,13,1,13,1,13,1,13,
  	1,14,1,14,1,14,1,14,1,14,1,14,1,14,1,14,1,14,5,14,210,8,14,10,14,12,14,
  	213,9,14,1,14,1,14,3,14,217,8,14,1,14,1,14,1,14,1,15,1,15,1,15,1,15,1,
  	15,1,15,1,15,1,15,3,15,230,8,15,1,15,1,15,1,15,1,15,3,15,236,8,15,1,15,
  	1,15,1,16,1,16,1,16,1,16,1,16,1,16,1,16,1,16,3,16,248,8,16,1,16,1,16,
  	1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,18,1,18,1,18,1,18,1,18,1,18,1,18,
  	1,19,1,19,1,19,1,19,5,19,270,8,19,10,19,12,19,273,9,19,1,19,3,19,276,
  	8,19,3,19,278,8,19,1,19,1,19,1,20,1,20,1,21,1,21,3,21,286,8,21,1,21,1,
  	21,1,22,1,22,1,22,1,23,1,23,1,23,1,24,1,24,1,25,1,25,1,26,1,26,1,27,1,
  	27,1,28,1,28,1,28,5,28,307,8,28,10,28,12,28,310,9,28,1,29,1,29,5,29,314,
  	8,29,10,29,12,29,317,9,29,1,30,1,30,1,30,1,30,3,30,323,8,30,1,30,3,30,
  	326,8,30,1,31,1,31,1,31,0,1,22,32,0,2,4,6,8,10,12,14,16,18,20,22,24,26,
  	28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,0,7,2,0,16,16,21,
  	22,1,0,19,20,1,0,6,11,1,0,4,5,2,0,18,18,25,25,1,0,45,46,4,0,4,5,12,16,
  	28,50,52,52,346,0,70,1,0,0,0,2,76,1,0,0,0,4,80,1,0,0,0,6,87,1,0,0,0,8,
  	95,1,0,0,0,10,107,1,0,0,0,12,123,1,0,0,0,14,125,1,0,0,0,16,132,1,0,0,
  	0,18,149,1,0,0,0,20,151,1,0,0,0,22,168,1,0,0,0,24,190,1,0,0,0,26,193,
  	1,0,0,0,28,200,1,0,0,0,30,221,1,0,0,0,32,239,1,0,0,0,34,251,1,0,0,0,36,
  	258,1,0,0,0,38,265,1,0,0,0,40,281,1,0,0,0,42,283,1,0,0,0,44,289,1,0,0,
  	0,46,292,1,0,0,0,48,295,1,0,0,0,50,297,1,0,0,0,52,299,1,0,0,0,54,301,
  	1,0,0,0,56,303,1,0,0,0,58,311,1,0,0,0,60,318,1,0,0,0,62,327,1,0,0,0,64,
  	69,3,18,9,0,65,69,3,8,4,0,66,69,3,12,6,0,67,69,3,14,7,0,68,64,1,0,0,0,
  	68,65,1,0,0,0,68,66,1,0,0,0,68,67,1,0,0,0,69,72,1,0,0,0,70,68,1,0,0,0,
  	70,71,1,0,0,0,71,73,1,0,0,0,72,70,1,0,0,0,73,74,5,0,0,1,74,1,1,0,0,0,
  	75,77,5,18,0,0,76,75,1,0,0,0,76,77,1,0,0,0,77,78,1,0,0,0,78,79,5,55,0,
  	0,79,3,1,0,0,0,80,81,3,56,28,0,81,83,5,23,0,0,82,84,3,6,3,0,83,82,1,0,
  	0,0,83,84,1,0,0,0,84,85,1,0,0,0,85,86,5,24,0,0,86,5,1,0,0,0,87,92,3,22,
  	11,0,88,89,5,25,0,0,89,91,3,22,11,0,90,88,1,0,0,0,91,94,1,0,0,0,92,90,
  	1,0,0,0,92,93,1,0,0,0,93,7,1,0,0,0,94,92,1,0,0,0,95,96,5,28,0,0,96,97,
  	3,54,27,0,97,99,5,23,0,0,98,100,3,10,5,0,99,98,1,0,0,0,99,100,1,0,0,0,
  	100,101,1,0,0,0,101,102,5,24,0,0,102,103,3,2,1,0,103,104,3,16,8,0,104,
  	105,5,29,0,0,105,106,3,2,1,0,106,9,1,0,0,0,107,112,3,50,25,0,108,109,
  	5,25,0,0,109,111,3,50,25,0,110,108,1,0,0,0,111,114,1,0,0,0,112,110,1,
  	0,0,0,112,113,1,0,0,0,113,11,1,0,0,0,114,112,1,0,0,0,115,116,5,50,0,0,
  	116,117,5,27,0,0,117,118,3,56,28,0,118,119,3,2,1,0,119,124,1,0,0,0,120,
  	121,5,50,0,0,121,122,5,51,0,0,122,124,3,2,1,0,123,115,1,0,0,0,123,120,
  	1,0,0,0,124,13,1,0,0,0,125,126,5,49,0,0,126,127,5,51,0,0,127,128,3,2,
  	1,0,128,15,1,0,0,0,129,131,3,18,9,0,130,129,1,0,0,0,131,134,1,0,0,0,132,
  	130,1,0,0,0,132,133,1,0,0,0,133,17,1,0,0,0,134,132,1,0,0,0,135,150,3,
  	26,13,0,136,150,3,28,14,0,137,150,3,30,15,0,138,150,3,32,16,0,139,150,
  	3,34,17,0,140,150,3,36,18,0,141,150,3,38,19,0,142,143,3,58,29,0,143,144,
  	3,2,1,0,144,150,1,0,0,0,145,150,3,42,21,0,146,150,3,44,22,0,147,150,3,
  	46,23,0,148,150,3,20,10,0,149,135,1,0,0,0,149,136,1,0,0,0,149,137,1,0,
  	0,0,149,138,1,0,0,0,149,139,1,0,0,0,149,140,1,0,0,0,149,141,1,0,0,0,149,
  	142,1,0,0,0,149,145,1,0,0,0,149,146,1,0,0,0,149,147,1,0,0,0,149,148,1,
  	0,0,0,150,19,1,0,0,0,151,152,3,2,1,0,152,21,1,0,0,0,153,154,6,11,-1,0,
  	154,169,3,58,29,0,155,156,5,23,0,0,156,157,3,22,11,0,157,158,5,24,0,0,
  	158,169,1,0,0,0,159,169,3,52,26,0,160,169,5,3,0,0,161,169,5,2,0,0,162,
  	169,5,51,0,0,163,169,3,24,12,0,164,165,5,20,0,0,165,169,3,22,11,7,166,
  	167,5,14,0,0,167,169,3,22,11,3,168,153,1,0,0,0,168,155,1,0,0,0,168,159,
  	1,0,0,0,168,160,1,0,0,0,168,161,1,0,0,0,168,162,1,0,0,0,168,163,1,0,0,
  	0,168,164,1,0,0,0,168,166,1,0,0,0,169,187,1,0,0,0,170,171,10,6,0,0,171,
  	172,7,0,0,0,172,186,3,22,11,7,173,174,10,5,0,0,174,175,7,1,0,0,175,186,
  	3,22,11,6,176,177,10,4,0,0,177,178,7,2,0,0,178,186,3,22,11,5,179,180,
  	10,2,0,0,180,181,5,12,0,0,181,186,3,22,11,3,182,183,10,1,0,0,183,184,
  	5,13,0,0,184,186,3,22,11,2,185,170,1,0,0,0,185,173,1,0,0,0,185,176,1,
  	0,0,0,185,179,1,0,0,0,185,182,1,0,0,0,186,189,1,0,0,0,187,185,1,0,0,0,
  	187,188,1,0,0,0,188,23,1,0,0,0,189,187,1,0,0,0,190,191,7,3,0,0,191,25,
  	1,0,0,0,192,194,5,15,0,0,193,192,1,0,0,0,193,194,1,0,0,0,194,195,1,0,
  	0,0,195,196,3,48,24,0,196,197,5,10,0,0,197,198,3,22,11,0,198,199,3,2,
  	1,0,199,27,1,0,0,0,200,201,5,31,0,0,201,202,3,22,11,0,202,203,5,32,0,
  	0,203,211,3,16,8,0,204,205,5,34,0,0,205,206,3,22,11,0,206,207,5,32,0,
  	0,207,208,3,16,8,0,208,210,1,0,0,0,209,204,1,0,0,0,210,213,1,0,0,0,211,
  	209,1,0,0,0,211,212,1,0,0,0,212,216,1,0,0,0,213,211,1,0,0,0,214,215,5,
  	33,0,0,215,217,3,16,8,0,216,214,1,0,0,0,216,217,1,0,0,0,217,218,1,0,0,
  	0,218,219,5,35,0,0,219,220,3,2,1,0,220,29,1,0,0,0,221,222,5,36,0,0,222,
  	223,3,56,28,0,223,224,5,10,0,0,224,225,3,22,11,0,225,226,5,39,0,0,226,
  	229,3,22,11,0,227,228,5,38,0,0,228,230,3,22,11,0,229,227,1,0,0,0,229,
  	230,1,0,0,0,230,231,1,0,0,0,231,232,3,2,1,0,232,233,3,16,8,0,233,235,
  	5,37,0,0,234,236,3,56,28,0,235,234,1,0,0,0,235,236,1,0,0,0,236,237,1,
  	0,0,0,237,238,3,2,1,0,238,31,1,0,0,0,239,240,5,36,0,0,240,241,3,56,28,
  	0,241,242,5,40,0,0,242,243,3,22,11,0,243,244,3,2,1,0,244,245,3,16,8,0,
  	245,247,5,37,0,0,246,248,3,56,28,0,247,246,1,0,0,0,247,248,1,0,0,0,248,
  	249,1,0,0,0,249,250,3,2,1,0,250,33,1,0,0,0,251,252,5,41,0,0,252,253,3,
  	22,11,0,253,254,3,2,1,0,254,255,3,16,8,0,255,256,5,42,0,0,256,257,3,2,
  	1,0,257,35,1,0,0,0,258,259,5,43,0,0,259,260,3,2,1,0,260,261,3,16,8,0,
  	261,262,5,44,0,0,262,263,3,22,11,0,263,264,3,2,1,0,264,37,1,0,0,0,265,
  	277,5,48,0,0,266,271,3,40,20,0,267,268,7,4,0,0,268,270,3,40,20,0,269,
  	267,1,0,0,0,270,273,1,0,0,0,271,269,1,0,0,0,271,272,1,0,0,0,272,275,1,
  	0,0,0,273,271,1,0,0,0,274,276,7,4,0,0,275,274,1,0,0,0,275,276,1,0,0,0,
  	276,278,1,0,0,0,277,266,1,0,0,0,277,278,1,0,0,0,278,279,1,0,0,0,279,280,
  	5,55,0,0,280,39,1,0,0,0,281,282,3,22,11,0,282,41,1,0,0,0,283,285,5,30,
  	0,0,284,286,3,22,11,0,285,284,1,0,0,0,285,286,1,0,0,0,286,287,1,0,0,0,
  	287,288,3,2,1,0,288,43,1,0,0,0,289,290,7,5,0,0,290,291,3,2,1,0,291,45,
  	1,0,0,0,292,293,5,47,0,0,293,294,3,2,1,0,294,47,1,0,0,0,295,296,3,56,
  	28,0,296,49,1,0,0,0,297,298,5,52,0,0,298,51,1,0,0,0,299,300,3,56,28,0,
  	300,53,1,0,0,0,301,302,5,52,0,0,302,55,1,0,0,0,303,308,5,52,0,0,304,305,
  	5,26,0,0,305,307,3,62,31,0,306,304,1,0,0,0,307,310,1,0,0,0,308,306,1,
  	0,0,0,308,309,1,0,0,0,309,57,1,0,0,0,310,308,1,0,0,0,311,315,3,4,2,0,
  	312,314,3,60,30,0,313,312,1,0,0,0,314,317,1,0,0,0,315,313,1,0,0,0,315,
  	316,1,0,0,0,316,59,1,0,0,0,317,315,1,0,0,0,318,319,5,26,0,0,319,325,3,
  	62,31,0,320,322,5,23,0,0,321,323,3,6,3,0,322,321,1,0,0,0,322,323,1,0,
  	0,0,323,324,1,0,0,0,324,326,5,24,0,0,325,320,1,0,0,0,325,326,1,0,0,0,
  	326,61,1,0,0,0,327,328,7,6,0,0,328,63,1,0,0,0,27,68,70,76,83,92,99,112,
  	123,132,149,168,185,187,193,211,216,229,235,247,271,275,277,285,308,315,
  	322,325
  };
  staticData->serializedATN = antlr4::atn::SerializedATNView(serializedATNSegment, sizeof(serializedATNSegment) / sizeof(serializedATNSegment[0]));

  antlr4::atn::ATNDeserializer deserializer;
  staticData->atn = deserializer.deserialize(staticData->serializedATN);

  const size_t count = staticData->atn->getNumberOfDecisions();
  staticData->decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    staticData->decisionToDFA.emplace_back(staticData->atn->getDecisionState(i), i);
  }
  basicparserParserStaticData = std::move(staticData);
}

}

BasicParser::BasicParser(TokenStream *input) : BasicParser(input, antlr4::atn::ParserATNSimulatorOptions()) {}

BasicParser::BasicParser(TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options) : Parser(input) {
  BasicParser::initialize();
  _interpreter = new atn::ParserATNSimulator(this, *basicparserParserStaticData->atn, basicparserParserStaticData->decisionToDFA, basicparserParserStaticData->sharedContextCache, options);
}

BasicParser::~BasicParser() {
  delete _interpreter;
}

const atn::ATN& BasicParser::getATN() const {
  return *basicparserParserStaticData->atn;
}

std::string BasicParser::getGrammarFileName() const {
  return "BasicParser.g4";
}

const std::vector<std::string>& BasicParser::getRuleNames() const {
  return basicparserParserStaticData->ruleNames;
}

const dfa::Vocabulary& BasicParser::getVocabulary() const {
  return basicparserParserStaticData->vocabulary;
}

antlr4::atn::SerializedATNView BasicParser::getSerializedATN() const {
  return basicparserParserStaticData->serializedATN;
}


//----------------- MainContext ------------------------------------------------------------------

BasicParser::MainContext::MainContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::MainContext::EOF() {
  return getToken(BasicParser::EOF, 0);
}

std::vector<BasicParser::StatementContext *> BasicParser::MainContext::statement() {
  return getRuleContexts<BasicParser::StatementContext>();
}

BasicParser::StatementContext* BasicParser::MainContext::statement(size_t i) {
  return getRuleContext<BasicParser::StatementContext>(i);
}

std::vector<BasicParser::ProcedureDefinitionContext *> BasicParser::MainContext::procedureDefinition() {
  return getRuleContexts<BasicParser::ProcedureDefinitionContext>();
}

BasicParser::ProcedureDefinitionContext* BasicParser::MainContext::procedureDefinition(size_t i) {
  return getRuleContext<BasicParser::ProcedureDefinitionContext>(i);
}

std::vector<BasicParser::ImportModuleContext *> BasicParser::MainContext::importModule() {
  return getRuleContexts<BasicParser::ImportModuleContext>();
}

BasicParser::ImportModuleContext* BasicParser::MainContext::importModule(size_t i) {
  return getRuleContext<BasicParser::ImportModuleContext>(i);
}

std::vector<BasicParser::ModuleDefinitionContext *> BasicParser::MainContext::moduleDefinition() {
  return getRuleContexts<BasicParser::ModuleDefinitionContext>();
}

BasicParser::ModuleDefinitionContext* BasicParser::MainContext::moduleDefinition(size_t i) {
  return getRuleContext<BasicParser::ModuleDefinitionContext>(i);
}


size_t BasicParser::MainContext::getRuleIndex() const {
  return BasicParser::RuleMain;
}


std::any BasicParser::MainContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitMain(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::MainContext* BasicParser::main() {
  MainContext *_localctx = _tracker.createInstance<MainContext>(_ctx, getState());
  enterRule(_localctx, 0, BasicParser::RuleMain);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(70);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 42760079413641216) != 0)) {
      setState(68);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case BasicParser::LET:
        case BasicParser::SEMICOLON:
        case BasicParser::RETURN:
        case BasicParser::IF:
        case BasicParser::FOR:
        case BasicParser::WHILE:
        case BasicParser::DO:
        case BasicParser::EXIT:
        case BasicParser::BREAK:
        case BasicParser::END:
        case BasicParser::PRINT:
        case BasicParser::ID:
        case BasicParser::NEWLINE: {
          setState(64);
          statement();
          break;
        }

        case BasicParser::DEF: {
          setState(65);
          procedureDefinition();
          break;
        }

        case BasicParser::IMPORT: {
          setState(66);
          importModule();
          break;
        }

        case BasicParser::MODULE: {
          setState(67);
          moduleDefinition();
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(72);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(73);
    match(BasicParser::EOF);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- EosContext ------------------------------------------------------------------

BasicParser::EosContext::EosContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::EosContext::NEWLINE() {
  return getToken(BasicParser::NEWLINE, 0);
}

tree::TerminalNode* BasicParser::EosContext::SEMICOLON() {
  return getToken(BasicParser::SEMICOLON, 0);
}


size_t BasicParser::EosContext::getRuleIndex() const {
  return BasicParser::RuleEos;
}


std::any BasicParser::EosContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitEos(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::EosContext* BasicParser::eos() {
  EosContext *_localctx = _tracker.createInstance<EosContext>(_ctx, getState());
  enterRule(_localctx, 2, BasicParser::RuleEos);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(76);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == BasicParser::SEMICOLON) {
      setState(75);
      match(BasicParser::SEMICOLON);
    }
    setState(78);
    match(BasicParser::NEWLINE);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ProcedureCallContext ------------------------------------------------------------------

BasicParser::ProcedureCallContext::ProcedureCallContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

BasicParser::QualifiedNameContext* BasicParser::ProcedureCallContext::qualifiedName() {
  return getRuleContext<BasicParser::QualifiedNameContext>(0);
}

tree::TerminalNode* BasicParser::ProcedureCallContext::LPAREN() {
  return getToken(BasicParser::LPAREN, 0);
}

tree::TerminalNode* BasicParser::ProcedureCallContext::RPAREN() {
  return getToken(BasicParser::RPAREN, 0);
}

BasicParser::ParameterListContext* BasicParser::ProcedureCallContext::parameterList() {
  return getRuleContext<BasicParser::ParameterListContext>(0);
}


size_t BasicParser::ProcedureCallContext::getRuleIndex() const {
  return BasicParser::RuleProcedureCall;
}


std::any BasicParser::ProcedureCallContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitProcedureCall(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::ProcedureCallContext* BasicParser::procedureCall() {
  ProcedureCallContext *_localctx = _tracker.createInstance<ProcedureCallContext>(_ctx, getState());
  enterRule(_localctx, 4, BasicParser::RuleProcedureCall);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(80);
    qualifiedName();
    setState(81);
    match(BasicParser::LPAREN);
    setState(83);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 6755399450509372) != 0)) {
      setState(82);
      parameterList();
    }
    setState(85);
    match(BasicParser::RPAREN);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ParameterListContext ------------------------------------------------------------------

BasicParser::ParameterListContext::ParameterListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<BasicParser::ExprContext *> BasicParser::ParameterListContext::expr() {
  return getRuleContexts<BasicParser::ExprContext>();
}

BasicParser::ExprContext* BasicParser::ParameterListContext::expr(size_t i) {
  return getRuleContext<BasicParser::ExprContext>(i);
}

std::vector<tree::TerminalNode *> BasicParser::ParameterListContext::COMMA() {
  return getTokens(BasicParser::COMMA);
}

tree::TerminalNode* BasicParser::ParameterListContext::COMMA(size_t i) {
  return getToken(BasicParser::COMMA, i);
}


size_t BasicParser::ParameterListContext::getRuleIndex() const {
  return BasicParser::RuleParameterList;
}


std::any BasicParser::ParameterListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitParameterList(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::ParameterListContext* BasicParser::parameterList() {
  ParameterListContext *_localctx = _tracker.createInstance<ParameterListContext>(_ctx, getState());
  enterRule(_localctx, 6, BasicParser::RuleParameterList);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(87);
    expr(0);
    setState(92);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == BasicParser::COMMA) {
      setState(88);
      match(BasicParser::COMMA);
      setState(89);
      expr(0);
      setState(94);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ProcedureDefinitionContext ------------------------------------------------------------------

BasicParser::ProcedureDefinitionContext::ProcedureDefinitionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::ProcedureDefinitionContext::DEF() {
  return getToken(BasicParser::DEF, 0);
}

BasicParser::ProcedureNameContext* BasicParser::ProcedureDefinitionContext::procedureName() {
  return getRuleContext<BasicParser::ProcedureNameContext>(0);
}

tree::TerminalNode* BasicParser::ProcedureDefinitionContext::LPAREN() {
  return getToken(BasicParser::LPAREN, 0);
}

tree::TerminalNode* BasicParser::ProcedureDefinitionContext::RPAREN() {
  return getToken(BasicParser::RPAREN, 0);
}

std::vector<BasicParser::EosContext *> BasicParser::ProcedureDefinitionContext::eos() {
  return getRuleContexts<BasicParser::EosContext>();
}

BasicParser::EosContext* BasicParser::ProcedureDefinitionContext::eos(size_t i) {
  return getRuleContext<BasicParser::EosContext>(i);
}

BasicParser::StatementsContext* BasicParser::ProcedureDefinitionContext::statements() {
  return getRuleContext<BasicParser::StatementsContext>(0);
}

tree::TerminalNode* BasicParser::ProcedureDefinitionContext::ENDDEF() {
  return getToken(BasicParser::ENDDEF, 0);
}

BasicParser::ParameterDefinitionListContext* BasicParser::ProcedureDefinitionContext::parameterDefinitionList() {
  return getRuleContext<BasicParser::ParameterDefinitionListContext>(0);
}


size_t BasicParser::ProcedureDefinitionContext::getRuleIndex() const {
  return BasicParser::RuleProcedureDefinition;
}


std::any BasicParser::ProcedureDefinitionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitProcedureDefinition(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::ProcedureDefinitionContext* BasicParser::procedureDefinition() {
  ProcedureDefinitionContext *_localctx = _tracker.createInstance<ProcedureDefinitionContext>(_ctx, getState());
  enterRule(_localctx, 8, BasicParser::RuleProcedureDefinition);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(95);
    match(BasicParser::DEF);
    setState(96);
    procedureName();
    setState(97);
    match(BasicParser::LPAREN);
    setState(99);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == BasicParser::ID) {
      setState(98);
      parameterDefinitionList();
    }
    setState(101);
    match(BasicParser::RPAREN);
    setState(102);
    eos();
    setState(103);
    statements();
    setState(104);
    match(BasicParser::ENDDEF);
    setState(105);
    eos();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ParameterDefinitionListContext ------------------------------------------------------------------

BasicParser::ParameterDefinitionListContext::ParameterDefinitionListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<BasicParser::IdContext *> BasicParser::ParameterDefinitionListContext::id() {
  return getRuleContexts<BasicParser::IdContext>();
}

BasicParser::IdContext* BasicParser::ParameterDefinitionListContext::id(size_t i) {
  return getRuleContext<BasicParser::IdContext>(i);
}

std::vector<tree::TerminalNode *> BasicParser::ParameterDefinitionListContext::COMMA() {
  return getTokens(BasicParser::COMMA);
}

tree::TerminalNode* BasicParser::ParameterDefinitionListContext::COMMA(size_t i) {
  return getToken(BasicParser::COMMA, i);
}


size_t BasicParser::ParameterDefinitionListContext::getRuleIndex() const {
  return BasicParser::RuleParameterDefinitionList;
}


std::any BasicParser::ParameterDefinitionListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitParameterDefinitionList(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::ParameterDefinitionListContext* BasicParser::parameterDefinitionList() {
  ParameterDefinitionListContext *_localctx = _tracker.createInstance<ParameterDefinitionListContext>(_ctx, getState());
  enterRule(_localctx, 10, BasicParser::RuleParameterDefinitionList);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(107);
    antlrcpp::downCast<ParameterDefinitionListContext *>(_localctx)->param = id();
    setState(112);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == BasicParser::COMMA) {
      setState(108);
      match(BasicParser::COMMA);
      setState(109);
      antlrcpp::downCast<ParameterDefinitionListContext *>(_localctx)->param = id();
      setState(114);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ImportModuleContext ------------------------------------------------------------------

BasicParser::ImportModuleContext::ImportModuleContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::ImportModuleContext::IMPORT() {
  return getToken(BasicParser::IMPORT, 0);
}

tree::TerminalNode* BasicParser::ImportModuleContext::AT() {
  return getToken(BasicParser::AT, 0);
}

BasicParser::QualifiedNameContext* BasicParser::ImportModuleContext::qualifiedName() {
  return getRuleContext<BasicParser::QualifiedNameContext>(0);
}

BasicParser::EosContext* BasicParser::ImportModuleContext::eos() {
  return getRuleContext<BasicParser::EosContext>(0);
}

tree::TerminalNode* BasicParser::ImportModuleContext::STRING() {
  return getToken(BasicParser::STRING, 0);
}


size_t BasicParser::ImportModuleContext::getRuleIndex() const {
  return BasicParser::RuleImportModule;
}


std::any BasicParser::ImportModuleContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitImportModule(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::ImportModuleContext* BasicParser::importModule() {
  ImportModuleContext *_localctx = _tracker.createInstance<ImportModuleContext>(_ctx, getState());
  enterRule(_localctx, 12, BasicParser::RuleImportModule);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(123);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 7, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(115);
      match(BasicParser::IMPORT);
      setState(116);
      match(BasicParser::AT);
      setState(117);
      qualifiedName();
      setState(118);
      eos();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(120);
      match(BasicParser::IMPORT);
      setState(121);
      match(BasicParser::STRING);
      setState(122);
      eos();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ModuleDefinitionContext ------------------------------------------------------------------

BasicParser::ModuleDefinitionContext::ModuleDefinitionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::ModuleDefinitionContext::MODULE() {
  return getToken(BasicParser::MODULE, 0);
}

tree::TerminalNode* BasicParser::ModuleDefinitionContext::STRING() {
  return getToken(BasicParser::STRING, 0);
}

BasicParser::EosContext* BasicParser::ModuleDefinitionContext::eos() {
  return getRuleContext<BasicParser::EosContext>(0);
}


size_t BasicParser::ModuleDefinitionContext::getRuleIndex() const {
  return BasicParser::RuleModuleDefinition;
}


std::any BasicParser::ModuleDefinitionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitModuleDefinition(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::ModuleDefinitionContext* BasicParser::moduleDefinition() {
  ModuleDefinitionContext *_localctx = _tracker.createInstance<ModuleDefinitionContext>(_ctx, getState());
  enterRule(_localctx, 14, BasicParser::RuleModuleDefinition);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(125);
    match(BasicParser::MODULE);
    setState(126);
    match(BasicParser::STRING);
    setState(127);
    eos();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StatementsContext ------------------------------------------------------------------

BasicParser::StatementsContext::StatementsContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<BasicParser::StatementContext *> BasicParser::StatementsContext::statement() {
  return getRuleContexts<BasicParser::StatementContext>();
}

BasicParser::StatementContext* BasicParser::StatementsContext::statement(size_t i) {
  return getRuleContext<BasicParser::StatementContext>(i);
}


size_t BasicParser::StatementsContext::getRuleIndex() const {
  return BasicParser::RuleStatements;
}


std::any BasicParser::StatementsContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitStatements(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::StatementsContext* BasicParser::statements() {
  StatementsContext *_localctx = _tracker.createInstance<StatementsContext>(_ctx, getState());
  enterRule(_localctx, 16, BasicParser::RuleStatements);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(132);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 41071229284941824) != 0)) {
      setState(129);
      statement();
      setState(134);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StatementContext ------------------------------------------------------------------

BasicParser::StatementContext::StatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

BasicParser::AssignmentStatementContext* BasicParser::StatementContext::assignmentStatement() {
  return getRuleContext<BasicParser::AssignmentStatementContext>(0);
}

BasicParser::IfStatementContext* BasicParser::StatementContext::ifStatement() {
  return getRuleContext<BasicParser::IfStatementContext>(0);
}

BasicParser::ForStatementContext* BasicParser::StatementContext::forStatement() {
  return getRuleContext<BasicParser::ForStatementContext>(0);
}

BasicParser::ForInStatementContext* BasicParser::StatementContext::forInStatement() {
  return getRuleContext<BasicParser::ForInStatementContext>(0);
}

BasicParser::WhileStatementContext* BasicParser::StatementContext::whileStatement() {
  return getRuleContext<BasicParser::WhileStatementContext>(0);
}

BasicParser::DoUntilStatementContext* BasicParser::StatementContext::doUntilStatement() {
  return getRuleContext<BasicParser::DoUntilStatementContext>(0);
}

BasicParser::PrintStatementContext* BasicParser::StatementContext::printStatement() {
  return getRuleContext<BasicParser::PrintStatementContext>(0);
}

BasicParser::CallChainContext* BasicParser::StatementContext::callChain() {
  return getRuleContext<BasicParser::CallChainContext>(0);
}

BasicParser::EosContext* BasicParser::StatementContext::eos() {
  return getRuleContext<BasicParser::EosContext>(0);
}

BasicParser::ReturnStatementContext* BasicParser::StatementContext::returnStatement() {
  return getRuleContext<BasicParser::ReturnStatementContext>(0);
}

BasicParser::ExitStatementContext* BasicParser::StatementContext::exitStatement() {
  return getRuleContext<BasicParser::ExitStatementContext>(0);
}

BasicParser::EndStatementContext* BasicParser::StatementContext::endStatement() {
  return getRuleContext<BasicParser::EndStatementContext>(0);
}

BasicParser::EmptyStatementContext* BasicParser::StatementContext::emptyStatement() {
  return getRuleContext<BasicParser::EmptyStatementContext>(0);
}


size_t BasicParser::StatementContext::getRuleIndex() const {
  return BasicParser::RuleStatement;
}


std::any BasicParser::StatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitStatement(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::StatementContext* BasicParser::statement() {
  StatementContext *_localctx = _tracker.createInstance<StatementContext>(_ctx, getState());
  enterRule(_localctx, 18, BasicParser::RuleStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(149);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 9, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(135);
      assignmentStatement();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(136);
      ifStatement();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(137);
      forStatement();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(138);
      forInStatement();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(139);
      whileStatement();
      break;
    }

    case 6: {
      enterOuterAlt(_localctx, 6);
      setState(140);
      doUntilStatement();
      break;
    }

    case 7: {
      enterOuterAlt(_localctx, 7);
      setState(141);
      printStatement();
      break;
    }

    case 8: {
      enterOuterAlt(_localctx, 8);
      setState(142);
      callChain();
      setState(143);
      eos();
      break;
    }

    case 9: {
      enterOuterAlt(_localctx, 9);
      setState(145);
      returnStatement();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(146);
      exitStatement();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(147);
      endStatement();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(148);
      emptyStatement();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- EmptyStatementContext ------------------------------------------------------------------

BasicParser::EmptyStatementContext::EmptyStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

BasicParser::EosContext* BasicParser::EmptyStatementContext::eos() {
  return getRuleContext<BasicParser::EosContext>(0);
}


size_t BasicParser::EmptyStatementContext::getRuleIndex() const {
  return BasicParser::RuleEmptyStatement;
}


std::any BasicParser::EmptyStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitEmptyStatement(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::EmptyStatementContext* BasicParser::emptyStatement() {
  EmptyStatementContext *_localctx = _tracker.createInstance<EmptyStatementContext>(_ctx, getState());
  enterRule(_localctx, 20, BasicParser::RuleEmptyStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(151);
    eos();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExprContext ------------------------------------------------------------------

BasicParser::ExprContext::ExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t BasicParser::ExprContext::getRuleIndex() const {
  return BasicParser::RuleExpr;
}

void BasicParser::ExprContext::copyFrom(ExprContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- IdentContext ------------------------------------------------------------------

BasicParser::RvalueContext* BasicParser::IdentContext::rvalue() {
  return getRuleContext<BasicParser::RvalueContext>(0);
}

BasicParser::IdentContext::IdentContext(ExprContext *ctx) { copyFrom(ctx); }


std::any BasicParser::IdentContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitIdent(this);
  else
    return visitor->visitChildren(this);
}
//----------------- OrContext ------------------------------------------------------------------

std::vector<BasicParser::ExprContext *> BasicParser::OrContext::expr() {
  return getRuleContexts<BasicParser::ExprContext>();
}

BasicParser::ExprContext* BasicParser::OrContext::expr(size_t i) {
  return getRuleContext<BasicParser::ExprContext>(i);
}

tree::TerminalNode* BasicParser::OrContext::OR() {
  return getToken(BasicParser::OR, 0);
}

BasicParser::OrContext::OrContext(ExprContext *ctx) { copyFrom(ctx); }


std::any BasicParser::OrContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitOr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- MulDivContext ------------------------------------------------------------------

std::vector<BasicParser::ExprContext *> BasicParser::MulDivContext::expr() {
  return getRuleContexts<BasicParser::ExprContext>();
}

BasicParser::ExprContext* BasicParser::MulDivContext::expr(size_t i) {
  return getRuleContext<BasicParser::ExprContext>(i);
}

tree::TerminalNode* BasicParser::MulDivContext::STAR() {
  return getToken(BasicParser::STAR, 0);
}

tree::TerminalNode* BasicParser::MulDivContext::SLASH() {
  return getToken(BasicParser::SLASH, 0);
}

tree::TerminalNode* BasicParser::MulDivContext::MOD() {
  return getToken(BasicParser::MOD, 0);
}

BasicParser::MulDivContext::MulDivContext(ExprContext *ctx) { copyFrom(ctx); }


std::any BasicParser::MulDivContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitMulDiv(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AddSubContext ------------------------------------------------------------------

std::vector<BasicParser::ExprContext *> BasicParser::AddSubContext::expr() {
  return getRuleContexts<BasicParser::ExprContext>();
}

BasicParser::ExprContext* BasicParser::AddSubContext::expr(size_t i) {
  return getRuleContext<BasicParser::ExprContext>(i);
}

tree::TerminalNode* BasicParser::AddSubContext::PLUS() {
  return getToken(BasicParser::PLUS, 0);
}

tree::TerminalNode* BasicParser::AddSubContext::MINUS() {
  return getToken(BasicParser::MINUS, 0);
}

BasicParser::AddSubContext::AddSubContext(ExprContext *ctx) { copyFrom(ctx); }


std::any BasicParser::AddSubContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitAddSub(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ParensContext ------------------------------------------------------------------

tree::TerminalNode* BasicParser::ParensContext::LPAREN() {
  return getToken(BasicParser::LPAREN, 0);
}

BasicParser::ExprContext* BasicParser::ParensContext::expr() {
  return getRuleContext<BasicParser::ExprContext>(0);
}

tree::TerminalNode* BasicParser::ParensContext::RPAREN() {
  return getToken(BasicParser::RPAREN, 0);
}

BasicParser::ParensContext::ParensContext(ExprContext *ctx) { copyFrom(ctx); }


std::any BasicParser::ParensContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitParens(this);
  else
    return visitor->visitChildren(this);
}
//----------------- StringContext ------------------------------------------------------------------

tree::TerminalNode* BasicParser::StringContext::STRING() {
  return getToken(BasicParser::STRING, 0);
}

BasicParser::StringContext::StringContext(ExprContext *ctx) { copyFrom(ctx); }


std::any BasicParser::StringContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitString(this);
  else
    return visitor->visitChildren(this);
}
//----------------- IntContext ------------------------------------------------------------------

tree::TerminalNode* BasicParser::IntContext::INT() {
  return getToken(BasicParser::INT, 0);
}

BasicParser::IntContext::IntContext(ExprContext *ctx) { copyFrom(ctx); }


std::any BasicParser::IntContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitInt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- NotContext ------------------------------------------------------------------

tree::TerminalNode* BasicParser::NotContext::NOT() {
  return getToken(BasicParser::NOT, 0);
}

BasicParser::ExprContext* BasicParser::NotContext::expr() {
  return getRuleContext<BasicParser::ExprContext>(0);
}

BasicParser::NotContext::NotContext(ExprContext *ctx) { copyFrom(ctx); }


std::any BasicParser::NotContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitNot(this);
  else
    return visitor->visitChildren(this);
}
//----------------- RelationContext ------------------------------------------------------------------

std::vector<BasicParser::ExprContext *> BasicParser::RelationContext::expr() {
  return getRuleContexts<BasicParser::ExprContext>();
}

BasicParser::ExprContext* BasicParser::RelationContext::expr(size_t i) {
  return getRuleContext<BasicParser::ExprContext>(i);
}

tree::TerminalNode* BasicParser::RelationContext::EQ() {
  return getToken(BasicParser::EQ, 0);
}

tree::TerminalNode* BasicParser::RelationContext::NE() {
  return getToken(BasicParser::NE, 0);
}

tree::TerminalNode* BasicParser::RelationContext::LT() {
  return getToken(BasicParser::LT, 0);
}

tree::TerminalNode* BasicParser::RelationContext::LE() {
  return getToken(BasicParser::LE, 0);
}

tree::TerminalNode* BasicParser::RelationContext::GT() {
  return getToken(BasicParser::GT, 0);
}

tree::TerminalNode* BasicParser::RelationContext::GE() {
  return getToken(BasicParser::GE, 0);
}

BasicParser::RelationContext::RelationContext(ExprContext *ctx) { copyFrom(ctx); }


std::any BasicParser::RelationContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitRelation(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ProcCallContext ------------------------------------------------------------------

BasicParser::CallChainContext* BasicParser::ProcCallContext::callChain() {
  return getRuleContext<BasicParser::CallChainContext>(0);
}

BasicParser::ProcCallContext::ProcCallContext(ExprContext *ctx) { copyFrom(ctx); }


std::any BasicParser::ProcCallContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitProcCall(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AndContext ------------------------------------------------------------------

std::vector<BasicParser::ExprContext *> BasicParser::AndContext::expr() {
  return getRuleContexts<BasicParser::ExprContext>();
}

BasicParser::ExprContext* BasicParser::AndContext::expr(size_t i) {
  return getRuleContext<BasicParser::ExprContext>(i);
}

tree::TerminalNode* BasicParser::AndContext::AND() {
  return getToken(BasicParser::AND, 0);
}

BasicParser::AndContext::AndContext(ExprContext *ctx) { copyFrom(ctx); }


std::any BasicParser::AndContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitAnd(this);
  else
    return visitor->visitChildren(this);
}
//----------------- RealContext ------------------------------------------------------------------

tree::TerminalNode* BasicParser::RealContext::REAL() {
  return getToken(BasicParser::REAL, 0);
}

BasicParser::RealContext::RealContext(ExprContext *ctx) { copyFrom(ctx); }


std::any BasicParser::RealContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitReal(this);
  else
    return visitor->visitChildren(this);
}
//----------------- NegateContext ------------------------------------------------------------------

tree::TerminalNode* BasicParser::NegateContext::MINUS() {
  return getToken(BasicParser::MINUS, 0);
}

BasicParser::ExprContext* BasicParser::NegateContext::expr() {
  return getRuleContext<BasicParser::ExprContext>(0);
}

BasicParser::NegateContext::NegateContext(ExprContext *ctx) { copyFrom(ctx); }


std::any BasicParser::NegateContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitNegate(this);
  else
    return visitor->visitChildren(this);
}
//----------------- BooleanContext ------------------------------------------------------------------

BasicParser::BooleanExprContext* BasicParser::BooleanContext::booleanExpr() {
  return getRuleContext<BasicParser::BooleanExprContext>(0);
}

BasicParser::BooleanContext::BooleanContext(ExprContext *ctx) { copyFrom(ctx); }


std::any BasicParser::BooleanContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitBoolean(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::ExprContext* BasicParser::expr() {
   return expr(0);
}

BasicParser::ExprContext* BasicParser::expr(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  BasicParser::ExprContext *_localctx = _tracker.createInstance<ExprContext>(_ctx, parentState);
  BasicParser::ExprContext *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 22;
  enterRecursionRule(_localctx, 22, BasicParser::RuleExpr, precedence);

    size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(168);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 10, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<ProcCallContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;

      setState(154);
      callChain();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<ParensContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(155);
      match(BasicParser::LPAREN);
      setState(156);
      expr(0);
      setState(157);
      match(BasicParser::RPAREN);
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<IdentContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(159);
      rvalue();
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<IntContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(160);
      match(BasicParser::INT);
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<RealContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(161);
      match(BasicParser::REAL);
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<StringContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(162);
      match(BasicParser::STRING);
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<BooleanContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(163);
      booleanExpr();
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<NegateContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(164);
      match(BasicParser::MINUS);
      setState(165);
      expr(7);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<NotContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(166);
      match(BasicParser::NOT);
      setState(167);
      expr(3);
      break;
    }

    default:
      break;
    }
    _ctx->stop = _input->LT(-1);
    setState(187);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 12, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        setState(185);
        _errHandler->sync(this);
        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 11, _ctx)) {
        case 1: {
          auto newContext = _tracker.createInstance<MulDivContext>(_tracker.createInstance<ExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleExpr);
          setState(170);

          if (!(precpred(_ctx, 6))) throw FailedPredicateException(this, "precpred(_ctx, 6)");
          setState(171);
          antlrcpp::downCast<MulDivContext *>(_localctx)->op = _input->LT(1);
          _la = _input->LA(1);
          if (!((((_la & ~ 0x3fULL) == 0) &&
            ((1ULL << _la) & 6356992) != 0))) {
            antlrcpp::downCast<MulDivContext *>(_localctx)->op = _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(172);
          expr(7);
          break;
        }

        case 2: {
          auto newContext = _tracker.createInstance<AddSubContext>(_tracker.createInstance<ExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleExpr);
          setState(173);

          if (!(precpred(_ctx, 5))) throw FailedPredicateException(this, "precpred(_ctx, 5)");
          setState(174);
          antlrcpp::downCast<AddSubContext *>(_localctx)->op = _input->LT(1);
          _la = _input->LA(1);
          if (!(_la == BasicParser::PLUS

          || _la == BasicParser::MINUS)) {
            antlrcpp::downCast<AddSubContext *>(_localctx)->op = _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(175);
          expr(6);
          break;
        }

        case 3: {
          auto newContext = _tracker.createInstance<RelationContext>(_tracker.createInstance<ExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleExpr);
          setState(176);

          if (!(precpred(_ctx, 4))) throw FailedPredicateException(this, "precpred(_ctx, 4)");
          setState(177);
          antlrcpp::downCast<RelationContext *>(_localctx)->op = _input->LT(1);
          _la = _input->LA(1);
          if (!((((_la & ~ 0x3fULL) == 0) &&
            ((1ULL << _la) & 4032) != 0))) {
            antlrcpp::downCast<RelationContext *>(_localctx)->op = _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(178);
          expr(5);
          break;
        }

        case 4: {
          auto newContext = _tracker.createInstance<AndContext>(_tracker.createInstance<ExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleExpr);
          setState(179);

          if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
          setState(180);
          match(BasicParser::AND);
          setState(181);
          expr(3);
          break;
        }

        case 5: {
          auto newContext = _tracker.createInstance<OrContext>(_tracker.createInstance<ExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleExpr);
          setState(182);

          if (!(precpred(_ctx, 1))) throw FailedPredicateException(this, "precpred(_ctx, 1)");
          setState(183);
          match(BasicParser::OR);
          setState(184);
          expr(2);
          break;
        }

        default:
          break;
        } 
      }
      setState(189);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 12, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- BooleanExprContext ------------------------------------------------------------------

BasicParser::BooleanExprContext::BooleanExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::BooleanExprContext::TRUE() {
  return getToken(BasicParser::TRUE, 0);
}

tree::TerminalNode* BasicParser::BooleanExprContext::FALSE() {
  return getToken(BasicParser::FALSE, 0);
}


size_t BasicParser::BooleanExprContext::getRuleIndex() const {
  return BasicParser::RuleBooleanExpr;
}


std::any BasicParser::BooleanExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitBooleanExpr(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::BooleanExprContext* BasicParser::booleanExpr() {
  BooleanExprContext *_localctx = _tracker.createInstance<BooleanExprContext>(_ctx, getState());
  enterRule(_localctx, 24, BasicParser::RuleBooleanExpr);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(190);
    _la = _input->LA(1);
    if (!(_la == BasicParser::TRUE

    || _la == BasicParser::FALSE)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- AssignmentStatementContext ------------------------------------------------------------------

BasicParser::AssignmentStatementContext::AssignmentStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

BasicParser::LvalueContext* BasicParser::AssignmentStatementContext::lvalue() {
  return getRuleContext<BasicParser::LvalueContext>(0);
}

tree::TerminalNode* BasicParser::AssignmentStatementContext::EQ() {
  return getToken(BasicParser::EQ, 0);
}

BasicParser::ExprContext* BasicParser::AssignmentStatementContext::expr() {
  return getRuleContext<BasicParser::ExprContext>(0);
}

BasicParser::EosContext* BasicParser::AssignmentStatementContext::eos() {
  return getRuleContext<BasicParser::EosContext>(0);
}

tree::TerminalNode* BasicParser::AssignmentStatementContext::LET() {
  return getToken(BasicParser::LET, 0);
}


size_t BasicParser::AssignmentStatementContext::getRuleIndex() const {
  return BasicParser::RuleAssignmentStatement;
}


std::any BasicParser::AssignmentStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitAssignmentStatement(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::AssignmentStatementContext* BasicParser::assignmentStatement() {
  AssignmentStatementContext *_localctx = _tracker.createInstance<AssignmentStatementContext>(_ctx, getState());
  enterRule(_localctx, 26, BasicParser::RuleAssignmentStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(193);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == BasicParser::LET) {
      setState(192);
      match(BasicParser::LET);
    }
    setState(195);
    lvalue();
    setState(196);
    match(BasicParser::EQ);
    setState(197);
    expr(0);
    setState(198);
    eos();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- IfStatementContext ------------------------------------------------------------------

BasicParser::IfStatementContext::IfStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::IfStatementContext::IF() {
  return getToken(BasicParser::IF, 0);
}

std::vector<BasicParser::ExprContext *> BasicParser::IfStatementContext::expr() {
  return getRuleContexts<BasicParser::ExprContext>();
}

BasicParser::ExprContext* BasicParser::IfStatementContext::expr(size_t i) {
  return getRuleContext<BasicParser::ExprContext>(i);
}

std::vector<tree::TerminalNode *> BasicParser::IfStatementContext::THEN() {
  return getTokens(BasicParser::THEN);
}

tree::TerminalNode* BasicParser::IfStatementContext::THEN(size_t i) {
  return getToken(BasicParser::THEN, i);
}

std::vector<BasicParser::StatementsContext *> BasicParser::IfStatementContext::statements() {
  return getRuleContexts<BasicParser::StatementsContext>();
}

BasicParser::StatementsContext* BasicParser::IfStatementContext::statements(size_t i) {
  return getRuleContext<BasicParser::StatementsContext>(i);
}

tree::TerminalNode* BasicParser::IfStatementContext::ENDIF() {
  return getToken(BasicParser::ENDIF, 0);
}

BasicParser::EosContext* BasicParser::IfStatementContext::eos() {
  return getRuleContext<BasicParser::EosContext>(0);
}

std::vector<tree::TerminalNode *> BasicParser::IfStatementContext::ELSEIF() {
  return getTokens(BasicParser::ELSEIF);
}

tree::TerminalNode* BasicParser::IfStatementContext::ELSEIF(size_t i) {
  return getToken(BasicParser::ELSEIF, i);
}

tree::TerminalNode* BasicParser::IfStatementContext::ELSE() {
  return getToken(BasicParser::ELSE, 0);
}


size_t BasicParser::IfStatementContext::getRuleIndex() const {
  return BasicParser::RuleIfStatement;
}


std::any BasicParser::IfStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitIfStatement(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::IfStatementContext* BasicParser::ifStatement() {
  IfStatementContext *_localctx = _tracker.createInstance<IfStatementContext>(_ctx, getState());
  enterRule(_localctx, 28, BasicParser::RuleIfStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(200);
    match(BasicParser::IF);
    setState(201);
    expr(0);
    setState(202);
    match(BasicParser::THEN);
    setState(203);
    statements();
    setState(211);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == BasicParser::ELSEIF) {
      setState(204);
      match(BasicParser::ELSEIF);
      setState(205);
      expr(0);
      setState(206);
      match(BasicParser::THEN);
      setState(207);
      statements();
      setState(213);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(216);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == BasicParser::ELSE) {
      setState(214);
      match(BasicParser::ELSE);
      setState(215);
      statements();
    }
    setState(218);
    match(BasicParser::ENDIF);
    setState(219);
    eos();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ForStatementContext ------------------------------------------------------------------

BasicParser::ForStatementContext::ForStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::ForStatementContext::FOR() {
  return getToken(BasicParser::FOR, 0);
}

std::vector<BasicParser::QualifiedNameContext *> BasicParser::ForStatementContext::qualifiedName() {
  return getRuleContexts<BasicParser::QualifiedNameContext>();
}

BasicParser::QualifiedNameContext* BasicParser::ForStatementContext::qualifiedName(size_t i) {
  return getRuleContext<BasicParser::QualifiedNameContext>(i);
}

tree::TerminalNode* BasicParser::ForStatementContext::EQ() {
  return getToken(BasicParser::EQ, 0);
}

std::vector<BasicParser::ExprContext *> BasicParser::ForStatementContext::expr() {
  return getRuleContexts<BasicParser::ExprContext>();
}

BasicParser::ExprContext* BasicParser::ForStatementContext::expr(size_t i) {
  return getRuleContext<BasicParser::ExprContext>(i);
}

tree::TerminalNode* BasicParser::ForStatementContext::TO() {
  return getToken(BasicParser::TO, 0);
}

std::vector<BasicParser::EosContext *> BasicParser::ForStatementContext::eos() {
  return getRuleContexts<BasicParser::EosContext>();
}

BasicParser::EosContext* BasicParser::ForStatementContext::eos(size_t i) {
  return getRuleContext<BasicParser::EosContext>(i);
}

BasicParser::StatementsContext* BasicParser::ForStatementContext::statements() {
  return getRuleContext<BasicParser::StatementsContext>(0);
}

tree::TerminalNode* BasicParser::ForStatementContext::NEXT() {
  return getToken(BasicParser::NEXT, 0);
}

tree::TerminalNode* BasicParser::ForStatementContext::STEP() {
  return getToken(BasicParser::STEP, 0);
}


size_t BasicParser::ForStatementContext::getRuleIndex() const {
  return BasicParser::RuleForStatement;
}


std::any BasicParser::ForStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitForStatement(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::ForStatementContext* BasicParser::forStatement() {
  ForStatementContext *_localctx = _tracker.createInstance<ForStatementContext>(_ctx, getState());
  enterRule(_localctx, 30, BasicParser::RuleForStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(221);
    match(BasicParser::FOR);
    setState(222);
    qualifiedName();
    setState(223);
    match(BasicParser::EQ);
    setState(224);
    expr(0);
    setState(225);
    match(BasicParser::TO);
    setState(226);
    expr(0);
    setState(229);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == BasicParser::STEP) {
      setState(227);
      match(BasicParser::STEP);
      setState(228);
      expr(0);
    }
    setState(231);
    eos();
    setState(232);
    statements();
    setState(233);
    match(BasicParser::NEXT);
    setState(235);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == BasicParser::ID) {
      setState(234);
      qualifiedName();
    }
    setState(237);
    eos();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ForInStatementContext ------------------------------------------------------------------

BasicParser::ForInStatementContext::ForInStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::ForInStatementContext::FOR() {
  return getToken(BasicParser::FOR, 0);
}

std::vector<BasicParser::QualifiedNameContext *> BasicParser::ForInStatementContext::qualifiedName() {
  return getRuleContexts<BasicParser::QualifiedNameContext>();
}

BasicParser::QualifiedNameContext* BasicParser::ForInStatementContext::qualifiedName(size_t i) {
  return getRuleContext<BasicParser::QualifiedNameContext>(i);
}

tree::TerminalNode* BasicParser::ForInStatementContext::IN() {
  return getToken(BasicParser::IN, 0);
}

BasicParser::ExprContext* BasicParser::ForInStatementContext::expr() {
  return getRuleContext<BasicParser::ExprContext>(0);
}

std::vector<BasicParser::EosContext *> BasicParser::ForInStatementContext::eos() {
  return getRuleContexts<BasicParser::EosContext>();
}

BasicParser::EosContext* BasicParser::ForInStatementContext::eos(size_t i) {
  return getRuleContext<BasicParser::EosContext>(i);
}

BasicParser::StatementsContext* BasicParser::ForInStatementContext::statements() {
  return getRuleContext<BasicParser::StatementsContext>(0);
}

tree::TerminalNode* BasicParser::ForInStatementContext::NEXT() {
  return getToken(BasicParser::NEXT, 0);
}


size_t BasicParser::ForInStatementContext::getRuleIndex() const {
  return BasicParser::RuleForInStatement;
}


std::any BasicParser::ForInStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitForInStatement(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::ForInStatementContext* BasicParser::forInStatement() {
  ForInStatementContext *_localctx = _tracker.createInstance<ForInStatementContext>(_ctx, getState());
  enterRule(_localctx, 32, BasicParser::RuleForInStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(239);
    match(BasicParser::FOR);
    setState(240);
    qualifiedName();
    setState(241);
    match(BasicParser::IN);
    setState(242);
    expr(0);
    setState(243);
    eos();
    setState(244);
    statements();
    setState(245);
    match(BasicParser::NEXT);
    setState(247);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == BasicParser::ID) {
      setState(246);
      qualifiedName();
    }
    setState(249);
    eos();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- WhileStatementContext ------------------------------------------------------------------

BasicParser::WhileStatementContext::WhileStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::WhileStatementContext::WHILE() {
  return getToken(BasicParser::WHILE, 0);
}

BasicParser::ExprContext* BasicParser::WhileStatementContext::expr() {
  return getRuleContext<BasicParser::ExprContext>(0);
}

std::vector<BasicParser::EosContext *> BasicParser::WhileStatementContext::eos() {
  return getRuleContexts<BasicParser::EosContext>();
}

BasicParser::EosContext* BasicParser::WhileStatementContext::eos(size_t i) {
  return getRuleContext<BasicParser::EosContext>(i);
}

BasicParser::StatementsContext* BasicParser::WhileStatementContext::statements() {
  return getRuleContext<BasicParser::StatementsContext>(0);
}

tree::TerminalNode* BasicParser::WhileStatementContext::WEND() {
  return getToken(BasicParser::WEND, 0);
}


size_t BasicParser::WhileStatementContext::getRuleIndex() const {
  return BasicParser::RuleWhileStatement;
}


std::any BasicParser::WhileStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitWhileStatement(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::WhileStatementContext* BasicParser::whileStatement() {
  WhileStatementContext *_localctx = _tracker.createInstance<WhileStatementContext>(_ctx, getState());
  enterRule(_localctx, 34, BasicParser::RuleWhileStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(251);
    match(BasicParser::WHILE);
    setState(252);
    expr(0);
    setState(253);
    eos();
    setState(254);
    statements();
    setState(255);
    match(BasicParser::WEND);
    setState(256);
    eos();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- DoUntilStatementContext ------------------------------------------------------------------

BasicParser::DoUntilStatementContext::DoUntilStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::DoUntilStatementContext::DO() {
  return getToken(BasicParser::DO, 0);
}

std::vector<BasicParser::EosContext *> BasicParser::DoUntilStatementContext::eos() {
  return getRuleContexts<BasicParser::EosContext>();
}

BasicParser::EosContext* BasicParser::DoUntilStatementContext::eos(size_t i) {
  return getRuleContext<BasicParser::EosContext>(i);
}

BasicParser::StatementsContext* BasicParser::DoUntilStatementContext::statements() {
  return getRuleContext<BasicParser::StatementsContext>(0);
}

tree::TerminalNode* BasicParser::DoUntilStatementContext::UNTIL() {
  return getToken(BasicParser::UNTIL, 0);
}

BasicParser::ExprContext* BasicParser::DoUntilStatementContext::expr() {
  return getRuleContext<BasicParser::ExprContext>(0);
}


size_t BasicParser::DoUntilStatementContext::getRuleIndex() const {
  return BasicParser::RuleDoUntilStatement;
}


std::any BasicParser::DoUntilStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitDoUntilStatement(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::DoUntilStatementContext* BasicParser::doUntilStatement() {
  DoUntilStatementContext *_localctx = _tracker.createInstance<DoUntilStatementContext>(_ctx, getState());
  enterRule(_localctx, 36, BasicParser::RuleDoUntilStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(258);
    match(BasicParser::DO);
    setState(259);
    eos();
    setState(260);
    statements();
    setState(261);
    match(BasicParser::UNTIL);
    setState(262);
    expr(0);
    setState(263);
    eos();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- PrintStatementContext ------------------------------------------------------------------

BasicParser::PrintStatementContext::PrintStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::PrintStatementContext::PRINT() {
  return getToken(BasicParser::PRINT, 0);
}

tree::TerminalNode* BasicParser::PrintStatementContext::NEWLINE() {
  return getToken(BasicParser::NEWLINE, 0);
}

std::vector<BasicParser::PrintItemContext *> BasicParser::PrintStatementContext::printItem() {
  return getRuleContexts<BasicParser::PrintItemContext>();
}

BasicParser::PrintItemContext* BasicParser::PrintStatementContext::printItem(size_t i) {
  return getRuleContext<BasicParser::PrintItemContext>(i);
}

std::vector<tree::TerminalNode *> BasicParser::PrintStatementContext::COMMA() {
  return getTokens(BasicParser::COMMA);
}

tree::TerminalNode* BasicParser::PrintStatementContext::COMMA(size_t i) {
  return getToken(BasicParser::COMMA, i);
}

std::vector<tree::TerminalNode *> BasicParser::PrintStatementContext::SEMICOLON() {
  return getTokens(BasicParser::SEMICOLON);
}

tree::TerminalNode* BasicParser::PrintStatementContext::SEMICOLON(size_t i) {
  return getToken(BasicParser::SEMICOLON, i);
}


size_t BasicParser::PrintStatementContext::getRuleIndex() const {
  return BasicParser::RulePrintStatement;
}


std::any BasicParser::PrintStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitPrintStatement(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::PrintStatementContext* BasicParser::printStatement() {
  PrintStatementContext *_localctx = _tracker.createInstance<PrintStatementContext>(_ctx, getState());
  enterRule(_localctx, 38, BasicParser::RulePrintStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(265);
    match(BasicParser::PRINT);
    setState(277);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 6755399450509372) != 0)) {
      setState(266);
      printItem();
      setState(271);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 19, _ctx);
      while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
        if (alt == 1) {
          setState(267);
          _la = _input->LA(1);
          if (!(_la == BasicParser::SEMICOLON

          || _la == BasicParser::COMMA)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(268);
          printItem(); 
        }
        setState(273);
        _errHandler->sync(this);
        alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 19, _ctx);
      }
      setState(275);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == BasicParser::SEMICOLON

      || _la == BasicParser::COMMA) {
        setState(274);
        antlrcpp::downCast<PrintStatementContext *>(_localctx)->trailing = _input->LT(1);
        _la = _input->LA(1);
        if (!(_la == BasicParser::SEMICOLON

        || _la == BasicParser::COMMA)) {
          antlrcpp::downCast<PrintStatementContext *>(_localctx)->trailing = _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
      }
    }
    setState(279);
    match(BasicParser::NEWLINE);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- PrintItemContext ------------------------------------------------------------------

BasicParser::PrintItemContext::PrintItemContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

BasicParser::ExprContext* BasicParser::PrintItemContext::expr() {
  return getRuleContext<BasicParser::ExprContext>(0);
}


size_t BasicParser::PrintItemContext::getRuleIndex() const {
  return BasicParser::RulePrintItem;
}


std::any BasicParser::PrintItemContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitPrintItem(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::PrintItemContext* BasicParser::printItem() {
  PrintItemContext *_localctx = _tracker.createInstance<PrintItemContext>(_ctx, getState());
  enterRule(_localctx, 40, BasicParser::RulePrintItem);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(281);
    expr(0);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ReturnStatementContext ------------------------------------------------------------------

BasicParser::ReturnStatementContext::ReturnStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::ReturnStatementContext::RETURN() {
  return getToken(BasicParser::RETURN, 0);
}

BasicParser::EosContext* BasicParser::ReturnStatementContext::eos() {
  return getRuleContext<BasicParser::EosContext>(0);
}

BasicParser::ExprContext* BasicParser::ReturnStatementContext::expr() {
  return getRuleContext<BasicParser::ExprContext>(0);
}


size_t BasicParser::ReturnStatementContext::getRuleIndex() const {
  return BasicParser::RuleReturnStatement;
}


std::any BasicParser::ReturnStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitReturnStatement(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::ReturnStatementContext* BasicParser::returnStatement() {
  ReturnStatementContext *_localctx = _tracker.createInstance<ReturnStatementContext>(_ctx, getState());
  enterRule(_localctx, 42, BasicParser::RuleReturnStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(283);
    match(BasicParser::RETURN);
    setState(285);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 6755399450509372) != 0)) {
      setState(284);
      expr(0);
    }
    setState(287);
    eos();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExitStatementContext ------------------------------------------------------------------

BasicParser::ExitStatementContext::ExitStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

BasicParser::EosContext* BasicParser::ExitStatementContext::eos() {
  return getRuleContext<BasicParser::EosContext>(0);
}

tree::TerminalNode* BasicParser::ExitStatementContext::EXIT() {
  return getToken(BasicParser::EXIT, 0);
}

tree::TerminalNode* BasicParser::ExitStatementContext::BREAK() {
  return getToken(BasicParser::BREAK, 0);
}


size_t BasicParser::ExitStatementContext::getRuleIndex() const {
  return BasicParser::RuleExitStatement;
}


std::any BasicParser::ExitStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitExitStatement(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::ExitStatementContext* BasicParser::exitStatement() {
  ExitStatementContext *_localctx = _tracker.createInstance<ExitStatementContext>(_ctx, getState());
  enterRule(_localctx, 44, BasicParser::RuleExitStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(289);
    _la = _input->LA(1);
    if (!(_la == BasicParser::EXIT

    || _la == BasicParser::BREAK)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(290);
    eos();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- EndStatementContext ------------------------------------------------------------------

BasicParser::EndStatementContext::EndStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::EndStatementContext::END() {
  return getToken(BasicParser::END, 0);
}

BasicParser::EosContext* BasicParser::EndStatementContext::eos() {
  return getRuleContext<BasicParser::EosContext>(0);
}


size_t BasicParser::EndStatementContext::getRuleIndex() const {
  return BasicParser::RuleEndStatement;
}


std::any BasicParser::EndStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitEndStatement(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::EndStatementContext* BasicParser::endStatement() {
  EndStatementContext *_localctx = _tracker.createInstance<EndStatementContext>(_ctx, getState());
  enterRule(_localctx, 46, BasicParser::RuleEndStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(292);
    match(BasicParser::END);
    setState(293);
    eos();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- LvalueContext ------------------------------------------------------------------

BasicParser::LvalueContext::LvalueContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

BasicParser::QualifiedNameContext* BasicParser::LvalueContext::qualifiedName() {
  return getRuleContext<BasicParser::QualifiedNameContext>(0);
}


size_t BasicParser::LvalueContext::getRuleIndex() const {
  return BasicParser::RuleLvalue;
}


std::any BasicParser::LvalueContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitLvalue(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::LvalueContext* BasicParser::lvalue() {
  LvalueContext *_localctx = _tracker.createInstance<LvalueContext>(_ctx, getState());
  enterRule(_localctx, 48, BasicParser::RuleLvalue);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(295);
    qualifiedName();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- IdContext ------------------------------------------------------------------

BasicParser::IdContext::IdContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::IdContext::ID() {
  return getToken(BasicParser::ID, 0);
}


size_t BasicParser::IdContext::getRuleIndex() const {
  return BasicParser::RuleId;
}


std::any BasicParser::IdContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitId(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::IdContext* BasicParser::id() {
  IdContext *_localctx = _tracker.createInstance<IdContext>(_ctx, getState());
  enterRule(_localctx, 50, BasicParser::RuleId);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(297);
    match(BasicParser::ID);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- RvalueContext ------------------------------------------------------------------

BasicParser::RvalueContext::RvalueContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

BasicParser::QualifiedNameContext* BasicParser::RvalueContext::qualifiedName() {
  return getRuleContext<BasicParser::QualifiedNameContext>(0);
}


size_t BasicParser::RvalueContext::getRuleIndex() const {
  return BasicParser::RuleRvalue;
}


std::any BasicParser::RvalueContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitRvalue(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::RvalueContext* BasicParser::rvalue() {
  RvalueContext *_localctx = _tracker.createInstance<RvalueContext>(_ctx, getState());
  enterRule(_localctx, 52, BasicParser::RuleRvalue);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(299);
    qualifiedName();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ProcedureNameContext ------------------------------------------------------------------

BasicParser::ProcedureNameContext::ProcedureNameContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::ProcedureNameContext::ID() {
  return getToken(BasicParser::ID, 0);
}


size_t BasicParser::ProcedureNameContext::getRuleIndex() const {
  return BasicParser::RuleProcedureName;
}


std::any BasicParser::ProcedureNameContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitProcedureName(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::ProcedureNameContext* BasicParser::procedureName() {
  ProcedureNameContext *_localctx = _tracker.createInstance<ProcedureNameContext>(_ctx, getState());
  enterRule(_localctx, 54, BasicParser::RuleProcedureName);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(301);
    match(BasicParser::ID);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- QualifiedNameContext ------------------------------------------------------------------

BasicParser::QualifiedNameContext::QualifiedNameContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::QualifiedNameContext::ID() {
  return getToken(BasicParser::ID, 0);
}

std::vector<tree::TerminalNode *> BasicParser::QualifiedNameContext::DOT() {
  return getTokens(BasicParser::DOT);
}

tree::TerminalNode* BasicParser::QualifiedNameContext::DOT(size_t i) {
  return getToken(BasicParser::DOT, i);
}

std::vector<BasicParser::MemberNameContext *> BasicParser::QualifiedNameContext::memberName() {
  return getRuleContexts<BasicParser::MemberNameContext>();
}

BasicParser::MemberNameContext* BasicParser::QualifiedNameContext::memberName(size_t i) {
  return getRuleContext<BasicParser::MemberNameContext>(i);
}


size_t BasicParser::QualifiedNameContext::getRuleIndex() const {
  return BasicParser::RuleQualifiedName;
}


std::any BasicParser::QualifiedNameContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitQualifiedName(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::QualifiedNameContext* BasicParser::qualifiedName() {
  QualifiedNameContext *_localctx = _tracker.createInstance<QualifiedNameContext>(_ctx, getState());
  enterRule(_localctx, 56, BasicParser::RuleQualifiedName);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(303);
    match(BasicParser::ID);
    setState(308);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 23, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        setState(304);
        match(BasicParser::DOT);
        setState(305);
        memberName(); 
      }
      setState(310);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 23, _ctx);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- CallChainContext ------------------------------------------------------------------

BasicParser::CallChainContext::CallChainContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

BasicParser::ProcedureCallContext* BasicParser::CallChainContext::procedureCall() {
  return getRuleContext<BasicParser::ProcedureCallContext>(0);
}

std::vector<BasicParser::MemberSuffixContext *> BasicParser::CallChainContext::memberSuffix() {
  return getRuleContexts<BasicParser::MemberSuffixContext>();
}

BasicParser::MemberSuffixContext* BasicParser::CallChainContext::memberSuffix(size_t i) {
  return getRuleContext<BasicParser::MemberSuffixContext>(i);
}


size_t BasicParser::CallChainContext::getRuleIndex() const {
  return BasicParser::RuleCallChain;
}


std::any BasicParser::CallChainContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitCallChain(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::CallChainContext* BasicParser::callChain() {
  CallChainContext *_localctx = _tracker.createInstance<CallChainContext>(_ctx, getState());
  enterRule(_localctx, 58, BasicParser::RuleCallChain);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(311);
    procedureCall();
    setState(315);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 24, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        setState(312);
        memberSuffix(); 
      }
      setState(317);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 24, _ctx);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- MemberSuffixContext ------------------------------------------------------------------

BasicParser::MemberSuffixContext::MemberSuffixContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::MemberSuffixContext::DOT() {
  return getToken(BasicParser::DOT, 0);
}

BasicParser::MemberNameContext* BasicParser::MemberSuffixContext::memberName() {
  return getRuleContext<BasicParser::MemberNameContext>(0);
}

tree::TerminalNode* BasicParser::MemberSuffixContext::LPAREN() {
  return getToken(BasicParser::LPAREN, 0);
}

tree::TerminalNode* BasicParser::MemberSuffixContext::RPAREN() {
  return getToken(BasicParser::RPAREN, 0);
}

BasicParser::ParameterListContext* BasicParser::MemberSuffixContext::parameterList() {
  return getRuleContext<BasicParser::ParameterListContext>(0);
}


size_t BasicParser::MemberSuffixContext::getRuleIndex() const {
  return BasicParser::RuleMemberSuffix;
}


std::any BasicParser::MemberSuffixContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitMemberSuffix(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::MemberSuffixContext* BasicParser::memberSuffix() {
  MemberSuffixContext *_localctx = _tracker.createInstance<MemberSuffixContext>(_ctx, getState());
  enterRule(_localctx, 60, BasicParser::RuleMemberSuffix);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(318);
    match(BasicParser::DOT);
    setState(319);
    memberName();
    setState(325);
    _errHandler->sync(this);

    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 26, _ctx)) {
    case 1: {
      setState(320);
      match(BasicParser::LPAREN);
      setState(322);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 6755399450509372) != 0)) {
        setState(321);
        parameterList();
      }
      setState(324);
      match(BasicParser::RPAREN);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- MemberNameContext ------------------------------------------------------------------

BasicParser::MemberNameContext::MemberNameContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* BasicParser::MemberNameContext::ID() {
  return getToken(BasicParser::ID, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::TRUE() {
  return getToken(BasicParser::TRUE, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::FALSE() {
  return getToken(BasicParser::FALSE, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::AND() {
  return getToken(BasicParser::AND, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::OR() {
  return getToken(BasicParser::OR, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::NOT() {
  return getToken(BasicParser::NOT, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::LET() {
  return getToken(BasicParser::LET, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::MOD() {
  return getToken(BasicParser::MOD, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::DEF() {
  return getToken(BasicParser::DEF, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::ENDDEF() {
  return getToken(BasicParser::ENDDEF, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::RETURN() {
  return getToken(BasicParser::RETURN, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::IF() {
  return getToken(BasicParser::IF, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::THEN() {
  return getToken(BasicParser::THEN, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::ELSE() {
  return getToken(BasicParser::ELSE, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::ELSEIF() {
  return getToken(BasicParser::ELSEIF, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::ENDIF() {
  return getToken(BasicParser::ENDIF, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::FOR() {
  return getToken(BasicParser::FOR, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::NEXT() {
  return getToken(BasicParser::NEXT, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::STEP() {
  return getToken(BasicParser::STEP, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::TO() {
  return getToken(BasicParser::TO, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::IN() {
  return getToken(BasicParser::IN, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::WHILE() {
  return getToken(BasicParser::WHILE, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::WEND() {
  return getToken(BasicParser::WEND, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::DO() {
  return getToken(BasicParser::DO, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::UNTIL() {
  return getToken(BasicParser::UNTIL, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::EXIT() {
  return getToken(BasicParser::EXIT, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::BREAK() {
  return getToken(BasicParser::BREAK, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::END() {
  return getToken(BasicParser::END, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::PRINT() {
  return getToken(BasicParser::PRINT, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::MODULE() {
  return getToken(BasicParser::MODULE, 0);
}

tree::TerminalNode* BasicParser::MemberNameContext::IMPORT() {
  return getToken(BasicParser::IMPORT, 0);
}


size_t BasicParser::MemberNameContext::getRuleIndex() const {
  return BasicParser::RuleMemberName;
}


std::any BasicParser::MemberNameContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<BasicParserVisitor*>(visitor))
    return parserVisitor->visitMemberName(this);
  else
    return visitor->visitChildren(this);
}

BasicParser::MemberNameContext* BasicParser::memberName() {
  MemberNameContext *_localctx = _tracker.createInstance<MemberNameContext>(_ctx, getState());
  enterRule(_localctx, 62, BasicParser::RuleMemberName);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(327);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 6755399172747312) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

bool BasicParser::sempred(RuleContext *context, size_t ruleIndex, size_t predicateIndex) {
  switch (ruleIndex) {
    case 11: return exprSempred(antlrcpp::downCast<ExprContext *>(context), predicateIndex);

  default:
    break;
  }
  return true;
}

bool BasicParser::exprSempred(ExprContext *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 0: return precpred(_ctx, 6);
    case 1: return precpred(_ctx, 5);
    case 2: return precpred(_ctx, 4);
    case 3: return precpred(_ctx, 2);
    case 4: return precpred(_ctx, 1);

  default:
    break;
  }
  return true;
}

void BasicParser::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  basicparserParserInitialize();
#else
  ::antlr4::internal::call_once(basicparserParserOnceFlag, basicparserParserInitialize);
#endif
}
