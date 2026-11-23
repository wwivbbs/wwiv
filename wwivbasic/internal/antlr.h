#pragma once

// Include the ANTLR runtime and the generated lexer and parser through this
// header, not directly.
//
// antlr4-common.h #undefs EOF. fmt built header-only (FMT_HEADER_ONLY, which
// WWIV's core library uses) needs EOF when format.h pulls in its
// implementation, so fmt has to be included first.
#include <cstdio>

#include "fmt/format.h"

#include "antlr4-runtime.h"

#include "BasicLexer.h"
#include "BasicParser.h"
#include "BasicParserBaseVisitor.h"
