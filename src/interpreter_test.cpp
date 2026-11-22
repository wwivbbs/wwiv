#include "gtest/gtest.h"
#include "context.h"
#include "interpreter.h"

#include <string>
#include <vector>

using namespace wwivbasic;

namespace {

class InterpreterTest : public ::testing::Test {
protected:
  InterpreterTest() {
    ctx.out = [this](const std::string& s) { output += s; };
  }

  // Runs the script and returns its output. Errors are kept in `errors`.
  std::string run(const std::string& text) {
    auto result = run_source(ctx, "test.bas", text);
    errors = result.errors;
    return output;
  }

  Context ctx;
  std::string output;
  std::vector<std::string> errors;
};

} // namespace

TEST_F(InterpreterTest, Print) {
  EXPECT_EQ("hello\n", run("print \"hello\"\n"));
  EXPECT_TRUE(errors.empty());
}

TEST_F(InterpreterTest, PrintSeparatorsAndTrailing) {
  // A trailing ',' or ';' suppresses the newline.
  EXPECT_EQ("a1b2\nxy", run("print \"a\", 1; \"b\", 2\nprint \"x\";\nprint \"y\",\n"));
}

TEST_F(InterpreterTest, PrintWithParens) {
  EXPECT_EQ("5\n", run("print(2 + 3)\n"));
}

TEST_F(InterpreterTest, NoTrailingNewlineAtEof) {
  EXPECT_EQ("ok\n", run("print \"ok\""));
}

TEST_F(InterpreterTest, Comments) {
  EXPECT_EQ("1\n", run("' leading comment\na = 1 ' trailing comment\nprint a'no space\n"));
  EXPECT_TRUE(errors.empty());
}

TEST_F(InterpreterTest, TrailingSemicolonOnStatement) {
  EXPECT_EQ("3\n", run("a = 3;\nprint a\n"));
  EXPECT_TRUE(errors.empty());
}

TEST_F(InterpreterTest, Precedence) {
  EXPECT_EQ("14\n", run("print 2 + 3 * 4\n"));
}

TEST_F(InterpreterTest, RelationBindsLooserThanArithmetic) {
  EXPECT_EQ("TRUE\n", run("a = 4\nprint a + 1 = 5\n"));
}

TEST_F(InterpreterTest, AndOrNot) {
  EXPECT_EQ("TRUE\nFALSE\nTRUE\n",
            run("print 1 = 1 and 2 = 2\nprint not 1 = 1\nprint false or not false\n"));
}

TEST_F(InterpreterTest, UnaryMinus) {
  EXPECT_EQ("-3\n7\n", run("print -3\nprint 10 + -3\n"));
}

TEST_F(InterpreterTest, IfElseIfElse) {
  const std::string script = R"(
def f(a)
  if a = 1 then
    return "one"
  elseif a = 2 then
    return "two"
  else
    return "many"
  endif
enddef
print f(1); f(2); f(3)
)";
  EXPECT_EQ("onetwomany\n", run(script));
}

TEST_F(InterpreterTest, IfWithIntCondition) {
  EXPECT_EQ("yes\n", run("if 1 then\nprint \"yes\"\nendif\n"));
}

TEST_F(InterpreterTest, ForLoop) {
  EXPECT_EQ("123", run("for i = 1 to 3\nprint i;\nnext\n"));
}

TEST_F(InterpreterTest, ForLoopNegativeStep) {
  EXPECT_EQ("531", run("for i = 5 to 1 step -2\nprint i;\nnext i\n"));
}

TEST_F(InterpreterTest, ForLoopNoIterations) {
  EXPECT_EQ("", run("for i = 3 to 1\nprint i;\nnext\n"));
}

TEST_F(InterpreterTest, WhileWend) {
  EXPECT_EQ("012", run("i = 0\nwhile i < 3\nprint i;\ni = i + 1\nwend\n"));
}

TEST_F(InterpreterTest, DoUntil) {
  EXPECT_EQ("012", run("i = 0\ndo\nprint i;\ni = i + 1\nuntil i = 3\n"));
}

TEST_F(InterpreterTest, ExitLeavesInnermostLoop) {
  const std::string script = R"(
while true
  for i = 1 to 10
    if i = 3 then
      exit
    endif
    print i;
  next
  exit
wend
print "done"
)";
  EXPECT_EQ("12done\n", run(script));
}

TEST_F(InterpreterTest, ReturnFromInsideLoop) {
  const std::string script = R"(
def find()
  while true
    return 42
  wend
  return 0
enddef
print find()
)";
  EXPECT_EQ("42\n", run(script));
}

TEST_F(InterpreterTest, BareReturn) {
  EXPECT_EQ("a", run("def f()\nprint \"a\";\nreturn\nprint \"b\";\nenddef\nf()\n"));
}

TEST_F(InterpreterTest, End) {
  EXPECT_EQ("a", run("print \"a\";\nend\nprint \"b\";\n"));
}

TEST_F(InterpreterTest, Recursion) {
  const std::string script = R"(
def fact(n)
  if n <= 1 then
    return 1
  endif
  return n * fact(n - 1)
enddef
print fact(5)
)";
  EXPECT_EQ("120\n", run(script));
}

TEST_F(InterpreterTest, FunctionLocalsAreNotVisibleToCallees) {
  const std::string script = R"(
def inner()
  return x
enddef
def outer()
  x = "local"
  return inner()
enddef
print "[" + outer() + "]"
)";
  EXPECT_EQ("[]\n", run(script));
}

TEST_F(InterpreterTest, FunctionsSeeAndUpdateGlobals) {
  EXPECT_EQ("2\n", run("g = 1\ndef inc()\ng = g + 1\nenddef\ninc()\nprint g\n"));
}

TEST_F(InterpreterTest, Builtins) {
  EXPECT_EQ("65A\n3\n", run("print asc(\"A\"); chr(65)\nprint len(\"abc\")\n"));
}

TEST_F(InterpreterTest, NativeFunctionConvertsArguments) {
  ctx.root->native_function("ADD", [](int a, int b) { return a + b; });
  EXPECT_EQ("5\n", run("print add(\"2\", 3)\n"));
  EXPECT_TRUE(errors.empty());
}

TEST_F(InterpreterTest, NativeFunctionWrongArgCount) {
  ctx.root->native_function("ADD", [](int a, int b) { return a + b; });
  run("print \"x\"\nprint add(1)\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:2: error: ADD: expected 2 argument(s), got 1", errors.front());
}

TEST_F(InterpreterTest, ImportModuleQuotedAndUnqualified) {
  auto& io = ctx.add_module("wwiv.io");
  io.native_function("OUTSTR", [this](std::string s) { output += "[" + s + "]"; });
  EXPECT_EQ("[a][b]", run("import \"@wwiv.io\"\noutstr(\"a\")\nwwiv.io.outstr(\"b\")\n"));
  EXPECT_TRUE(errors.empty());
}

TEST_F(InterpreterTest, UnqualifiedNeedsImport) {
  auto& io = ctx.add_module("wwiv.io");
  io.native_function("OUTSTR", [](std::string) {});
  run("outstr(\"a\")\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:1: error: Unknown function 'outstr'", errors.front());
}

TEST_F(InterpreterTest, ScriptFunctionWinsOverImported) {
  auto& io = ctx.add_module("wwiv.io");
  io.native_function("NL", [this]() { output += "native"; });
  EXPECT_EQ("script\n", run("import \"@wwiv.io\"\ndef nl()\nprint \"script\"\nenddef\nnl()\n"));
}

TEST_F(InterpreterTest, UnknownFunctionIsAnError) {
  run("print \"a\"\nnosuch(1)\nprint \"b\"\n");
  EXPECT_EQ("a\n", output);
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:2: error: Unknown function 'nosuch'", errors.front());
}

TEST_F(InterpreterTest, UnknownModuleImportIsAnError) {
  run("import \"@nosuch\"\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:1: error: Unknown module: '@nosuch'", errors.front());
}

TEST_F(InterpreterTest, DivisionByZeroIsAnError) {
  run("a = 0\nprint 1 / a\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:2: error: Division by zero", errors.front());
}

TEST_F(InterpreterTest, ParseErrorHasLocation) {
  run("print \"a\"\nif then\n");
  ASSERT_FALSE(errors.empty());
  EXPECT_EQ(0u, errors.front().rfind("test.bas:2:4: error:", 0)) << errors.front();
  EXPECT_EQ("", output);
}

TEST_F(InterpreterTest, WrongArgCountToScriptFunction) {
  run("def f(a)\nreturn a\nenddef\nprint f()\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:4: error: f: expected 1 argument(s), got 0", errors.front());
}

TEST_F(InterpreterTest, DuplicateFunctionIsAnError) {
  run("def f()\nenddef\ndef f()\nenddef\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:3: error: function 'f' is already defined", errors.front());
}
