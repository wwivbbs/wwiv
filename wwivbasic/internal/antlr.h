#pragma once

// Include the ANTLR runtime and the generated lexer and parser through this
// header, not directly.
//
// antlr4-common.h #undefs EOF. fmt built header-only (FMT_HEADER_ONLY, which
// WWIV's core library uses) needs EOF when format.h pulls in its
// implementation, so fmt has to be included first.
#include <cstdio>

#include "fmt/format.h"

// On MSVC, antlr4-common.h defines ssize_t itself, as __int32 on 32-bit
// Windows, with no guard. core/wwivport.h (like cpp-httplib) defines it as
// __int64 on all Windows builds, so the two conflict on x86. Rename ANTLR's
// typedef while its headers are included: ANTLR's declarations then use
// antlr_ssize_t, the same type the ANTLR library was built with (a typedef's
// name doesn't affect linking), and everything else keeps WWIV's ssize_t.
#ifdef _MSC_VER
#define ssize_t antlr_ssize_t
#endif

#include "antlr4-runtime.h"

#include "BasicLexer.h"
#include "BasicParser.h"
#include "BasicParserBaseVisitor.h"

#ifdef _MSC_VER
#undef ssize_t
#endif
