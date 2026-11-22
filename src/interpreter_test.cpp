#include "gtest/gtest.h"
#include "testing/fake_console.h"
#include "wwivbasic/interpreter.h"

#include <string>
#include <vector>

using namespace wwivbasic;

namespace {

// Runs scripts through the public embedding API with a fake console.
class InterpreterTest : public ::testing::Test {
protected:
  // Runs the script and returns its output. Errors are kept in `errors`.
  std::string run(const std::string& text) {
    auto result = interp.run_source("test.bas", text);
    errors = result.errors;
    return output;
  }

  wwivbasic::testing::FakeConsole console;
  Interpreter interp{console};
  // What the script printed so far.
  std::string& output{console.output};
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
  interp.globals().native_function("ADD", [](int a, int b) { return a + b; });
  EXPECT_EQ("5\n", run("print add(\"2\", 3)\n"));
  EXPECT_TRUE(errors.empty());
}

TEST_F(InterpreterTest, NativeFunctionWrongArgCount) {
  interp.globals().native_function("ADD", [](int a, int b) { return a + b; });
  run("print \"x\"\nprint add(1)\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:2: error: ADD: expected 2 argument(s), got 1", errors.front());
}

TEST_F(InterpreterTest, ImportModuleQuotedAndUnqualified) {
  auto& io = interp.add_module("wwiv.io");
  io.native_function("OUTSTR", [this](std::string s) { output += "[" + s + "]"; });
  EXPECT_EQ("[a][b]", run("import \"@wwiv.io\"\noutstr(\"a\")\nwwiv.io.outstr(\"b\")\n"));
  EXPECT_TRUE(errors.empty());
}

TEST_F(InterpreterTest, UnqualifiedNeedsImport) {
  auto& io = interp.add_module("wwiv.io");
  io.native_function("OUTSTR", [](std::string) {});
  run("outstr(\"a\")\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:1: error: Unknown function 'outstr'", errors.front());
}

TEST_F(InterpreterTest, ScriptFunctionWinsOverImported) {
  auto& io = interp.add_module("wwiv.io");
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

// Lists (M2)

TEST_F(InterpreterTest, ListBasics) {
  const std::string script = R"(
l = list("a", "b")
push(l, "c")
print len(l); " "; get(l, 0); get(l, 2); " "; l(1)
print l
)";
  EXPECT_EQ("3 ac b\nLIST\n", run(script));
  EXPECT_TRUE(errors.empty()) << errors.front();
}

TEST_F(InterpreterTest, ListsAreReferences) {
  const std::string script = R"(
def fill(target)
  push(target, 1, 2)
enddef
a = list()
b = a
fill(b)
print len(a); " "; a = b
c = clone(a)
push(c, 3)
print len(a); len(c); " "; a = c
)";
  EXPECT_EQ("2 TRUE\n23 FALSE\n", run(script));
}

TEST_F(InterpreterTest, ListEditing) {
  const std::string script = R"(
l = list(3, 1, 2)
sort(l)
insert(l, 0, 0)
set(l, 3, 9)
remove(l, 1)
print get(l, 0); get(l, 1); get(l, 2); " "; pop(l); back(l); " "; len(l)
print exists(l, 2); " "; index_of(l, 2); index_of(l, 42)
clear(l)
print len(l)
)";
  EXPECT_EQ("029 92 2\nTRUE 1-1\n0\n", run(script));
  EXPECT_TRUE(errors.empty()) << errors.front();
}

TEST_F(InterpreterTest, Iterator) {
  const std::string script = R"(
l = list("x", "y")
i = iterator(l)
while move_next(i)
  print get(i);
wend
print move_next(i)
)";
  EXPECT_EQ("xyFALSE\n", run(script));
}

TEST_F(InterpreterTest, IteratorOnEmptyList) {
  EXPECT_EQ("FALSE\n", run("print move_next(iterator(list()))\n"));
}

TEST_F(InterpreterTest, ForIn) {
  const std::string script = R"(
total = 0
for n in list(1, 2, 3, 4)
  if n = 4 then
    exit
  endif
  total = total + n
next n
print total
)";
  EXPECT_EQ("6\n", run(script));
}

TEST_F(InterpreterTest, ForInRequiresList) {
  run("for c in \"abc\"\nnext\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:1: error: FOR ... IN: expected a LIST, got STRING", errors.front());
}

TEST_F(InterpreterTest, ListIndexOutOfRange) {
  run("l = list(1)\nprint get(l, 1)\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:2: error: GET: index 1 is out of range (list has 1 items)", errors.front());
}

TEST_F(InterpreterTest, ListInArithmeticIsAnError) {
  run("l = list()\nprint l + 1\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:2: error: Operator '+' cannot be used with a LIST", errors.front());
}

TEST_F(InterpreterTest, FunctionWinsOverListIndex) {
  EXPECT_EQ("fn\n", run("def l(x)\nreturn \"fn\"\nenddef\nl = list(1)\nprint l(0)\n"));
}

TEST_F(InterpreterTest, LenOfString) {
  EXPECT_EQ("5\n", run("print len(\"hello\")\n"));
}

TEST_F(InterpreterTest, Str) {
  EXPECT_EQ("12ab\n", run("print str(12) + \"ab\"\n"));
}

TEST_F(InterpreterTest, ImportOnlyAtTopLevel) {
  // IMPORT is a top-level statement; inside a function it's a parse error.
  run("def f()\nimport \"x.bas\"\nenddef\n");
  ASSERT_FALSE(errors.empty());
  EXPECT_EQ(0u, errors.front().rfind("test.bas:2:1: error:", 0)) << errors.front();
}

TEST_F(InterpreterTest, ListIndexIgnoresScriptGet) {
  // l(i) reads the list even if the module defines its own GET.
  const std::string script = R"(
module "m"
def get(a, b)
  return "mine"
enddef
l = list("item")
print l(0); " "; m.get(1, 2)
)";
  EXPECT_EQ("item mine\n", run(script));
  EXPECT_TRUE(errors.empty()) << errors.front();
}

TEST_F(InterpreterTest, SortMixedTypes) {
  const std::string script = R"(
l = sort(list("b", 10, "a", 2, true))
for x in l
  print x; " ";
next
)";
  EXPECT_EQ("TRUE 2 10 a b ", run(script));
}

TEST_F(InterpreterTest, SortRejectsNestedLists) {
  run("l = list(1, list())\nsort(l)\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:2: error: SORT: cannot sort a list containing a LIST", errors.front());
}

TEST_F(InterpreterTest, BooleanTimesIsInteger) {
  EXPECT_EQ("1 0\n", run("print true * true; \" \"; true * false\n"));
}

TEST_F(InterpreterTest, LenWithNoArgs) {
  EXPECT_EQ("0\n", run("print len()\n"));
}
