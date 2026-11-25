#include "gtest/gtest.h"
#include "wwivbasic/testing/fake_console.h"
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

// A script written before a built-in was added keeps its own function.
TEST_F(InterpreterTest, DefReplacesBuiltIn) {
  run("def trim(s)\nreturn \"mine\"\nenddef\nprint trim(\" x \"); \"/\"; ucase(\"a\")\n");
  EXPECT_TRUE(errors.empty()) << errors.front();
  EXPECT_EQ("mine/A\n", output);
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

// Real numbers (M3)

TEST_F(InterpreterTest, RealLiterals) {
  EXPECT_EQ("1.5 0.5 3.14159\n", run("print 1.5; \" \"; .5; \" \"; 3.14159265\n"));
  EXPECT_TRUE(errors.empty()) << errors.front();
}

TEST_F(InterpreterTest, RealArithmetic) {
  const std::string script = R"(
print 7 / 2; " "; 6 / 2; " "; 0.1 + 0.2; " "; -1.5; " "; 10 - 2.5
print 1 < 1.5; " "; 2 = 2.0; " "; 7.9 mod 2
)";
  EXPECT_EQ("3.5 3 0.3 -1.5 7.5\nTRUE TRUE 1\n", run(script));
}

TEST_F(InterpreterTest, NumberFunctions) {
  const std::string script = R"(
print abs(-1.5); " "; abs(-3); " "; sgn(-0.5); " "; round(2.5); " "; round(-2.5)
print floor(-1.5); " "; ceil(1.2); " "; fix(-1.7); " "; val("1.5") + 1; " "; str(0.5) + "!"
)";
  EXPECT_EQ("1.5 3 -1 3 -3\n-2 2 -1 2.5 0.5!\n", run(script));
}

TEST_F(InterpreterTest, NativeFunctionWithDouble) {
  interp.globals().native_function("KB", [](double bytes) { return bytes / 1024; });
  EXPECT_EQ("1.5\n", run("print kb(1536)\n"));
}

TEST_F(InterpreterTest, SortReals) {
  EXPECT_EQ("0.5 1 2.5 x ",
            run("for v in sort(list(\"x\", 2.5, 1, 0.5))\nprint v; \" \";\nnext\n"));
}

// Member access (M3). No values have members yet, so these check parsing,
// module paths, and the errors for members of plain values.

TEST_F(InterpreterTest, MemberChainsParse) {
  RunOptions options;
  options.execute = false;
  const auto result = interp.run_source(
      "t.bas", "x = area.headers().count\narea.name = \"x\"\nprint a.b(1).c().d\nobj.go()\n",
      options);
  EXPECT_TRUE(result.ok()) << result.errors.front();
}

TEST_F(InterpreterTest, ModulePathsStillWork) {
  auto& io = interp.add_module("wwiv.io");
  io.native_function("OUTSTR", [this](std::string s) { output += s; });
  interp.add_module("wwiv").native_function("VERSION", []() { return std::string("5"); });
  EXPECT_EQ("v5", run("import @wwiv.io\nwwiv.io.outstr(\"v\")\noutstr(wwiv.version())\n"));
  EXPECT_TRUE(errors.empty()) << errors.front();
}

TEST_F(InterpreterTest, SpacesAroundDots) {
  interp.add_module("wwiv").native_function("VERSION", []() { return std::string("5"); });
  EXPECT_EQ("5\n", run("print wwiv . version()\n"));
}

TEST_F(InterpreterTest, UnknownFunctionInModule) {
  interp.add_module("wwiv.io");
  run("wwiv.io.nosuch()\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:1: error: Unknown function 'wwiv.io.nosuch'", errors.front());
}

TEST_F(InterpreterTest, MemberOfPlainValueIsAnError) {
  run("s = \"text\"\nprint s.size\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:2: error: Cannot read member 'size' of a STRING", errors.front());
}

TEST_F(InterpreterTest, SetMemberOfPlainValueIsAnError) {
  run("n = 1\nn.x = 2\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:2: error: Cannot set member 'x' of an INTEGER", errors.front());
}

TEST_F(InterpreterTest, MethodOfPlainValueIsAnError) {
  run("l = list()\nl.clear()\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:2: error: Cannot call method 'clear' on a LIST", errors.front());
}

TEST_F(InterpreterTest, MemberOfCallResultIsAnError) {
  run("print str(1).x\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:1: error: Cannot read member 'x' of a STRING", errors.front());
}

TEST_F(InterpreterTest, UnknownMemberPathRoot) {
  run("print nosuch.x\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:1: error: Unknown variable or module 'nosuch' in 'nosuch.x'",
            errors.front());
}

TEST_F(InterpreterTest, RealLiteralRules) {
  // A whole real literal is an INTEGER, and -0.0 prints 0, like my_basic.
  EXPECT_EQ("2 0 0.5\n", run("print 2.0; \" \"; -0.0; \" \"; -(-0.5)\n"));
}

TEST_F(InterpreterTest, RealLiteralOutOfRange) {
  run("x = ." + std::string(400, '0') + "1\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ(0u, errors.front().rfind("test.bas:1: error: Number out of range: .000", 0))
      << errors.front();
}

TEST_F(InterpreterTest, ForLoopWithRealStep) {
  EXPECT_EQ("0 0.25 0.5 0.75 1 ", run("for x = 0 to 1 step 0.25\nprint x; \" \";\nnext\n"));
}

TEST_F(InterpreterTest, ForLoopWithRealBoundsCountingDown) {
  EXPECT_EQ("1.5 1 0.5 ", run("for x = 1.5 to 0.5 step -0.5\nprint x; \" \";\nnext\n"));
}

TEST_F(InterpreterTest, ValOfNonDecimal) {
  EXPECT_EQ("0 0 -2.5 1\n", run("print val(\"nan\"); \" \"; val(\"0x10\"); \" \"; "
                                "val(\" -2.5\"); \" \"; 1 + \"0x10\"\n"));
}

// Dotted names through module variables (PR #6 review).

TEST_F(InterpreterTest, IndexListInModule) {
  EXPECT_EQ("20\n", run("module \"m\"\nm.items = list(10, 20)\nprint m.items(1)\n"));
  EXPECT_TRUE(errors.empty()) << errors.front();
}

TEST_F(InterpreterTest, MemberOfModuleVariable) {
  // m.items is found as module m's variable; .x is then a member of the LIST.
  run("module \"m\"\nm.items = list()\nprint m.items.x\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:3: error: Cannot read member 'x' of a LIST", errors.front());
}

TEST_F(InterpreterTest, LongestModulePrefixWins) {
  interp.add_module("wwiv");
  interp.add_module("wwiv.io").native_function("NAME", []() { return std::string("io"); });
  EXPECT_EQ("io\n", run("print wwiv.io.name()\n"));
  EXPECT_TRUE(errors.empty()) << errors.front();
}

TEST_F(InterpreterTest, UnknownSubmoduleNamesTheWholePrefix) {
  interp.add_module("wwiv");
  run("wwiv.nosuchsub.foo()\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:1: error: Unknown variable or module 'wwiv.nosuchsub' in "
            "'wwiv.nosuchsub.foo'",
            errors.front());
}

TEST_F(InterpreterTest, UnsetModuleVariableReadsEmpty) {
  EXPECT_EQ("[]\n", run("module \"m\"\nprint \"[\" + m.nosuch + \"]\"\n"));
}

TEST_F(InterpreterTest, ForWithModuleVariable) {
  EXPECT_EQ("12", run("module \"m\"\nfor m.i = 1 to 2\nprint m.i;\nnext m.i\n"));
  EXPECT_TRUE(errors.empty()) << errors.front();
}

TEST_F(InterpreterTest, NextMustMatchFor) {
  run("for i = 1 to 2\nnext j\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:1: error: NEXT j does not match FOR i", errors.front());
}

// Interpreter::call: native functions calling back into the script.

namespace {

class InterpreterCallTest : public InterpreterTest {
protected:
  void SetUp() override {
    // APPLY(fn, args...) returns fn(args...).
    interp.globals().native_functionl("APPLY", [this](std::vector<Value> args) {
      const auto fn = args.at(0).toString();
      args.erase(args.begin());
      return interp.call(fn, args);
    });
    // EACH(fn, n) calls fn(1), fn(2)... up to fn(n), like a lightbar calling
    // a script function for each key, until fn returns FALSE. Returns how
    // many calls it made.
    interp.globals().native_function("EACH", [this](std::string fn, int n) {
      for (int i = 1; i <= n; i++) {
        ++calls;
        if (!interp.call(fn, {Value(i)}).toBool()) {
          break;
        }
      }
      return calls;
    });
  }

  int calls{0};
};

} // namespace

TEST_F(InterpreterCallTest, ReturnsValue) {
  EXPECT_EQ("42\n", run("def double(x)\nreturn x * 2\nenddef\nprint apply(\"double\", 21)\n"));
  EXPECT_TRUE(errors.empty());
}

// The function runs as a call in the script would: it sees and changes
// globals, and calls other functions, built-in ones too.
TEST_F(InterpreterCallTest, CallsOtherFunctionsAndChangesGlobals) {
  const std::string script = R"(
total = 0
def square(x)
  return x * x
enddef
def add(i)
  total = total + square(i)
  return TRUE
enddef
print each("add", 3); " "; total; " "; apply("ucase", "x")
)";
  EXPECT_EQ("3 14 X\n", run(script));
  EXPECT_TRUE(errors.empty()) << errors.front();
}

TEST_F(InterpreterCallTest, StopsWhenCallbackReturnsFalse) {
  EXPECT_EQ("2\n", run("def f(i)\nreturn i < 2\nenddef\nprint each(\"f\", 10)\n"));
}

// An error in the function is reported at its line.
TEST_F(InterpreterCallTest, ErrorInCallback) {
  run("def f(i)\nx = nosuch(i)\nreturn TRUE\nenddef\nprint each(\"f\", 3)\nprint \"after\"\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:2: error: Unknown function 'nosuch'", errors.front());
  EXPECT_EQ(1, calls);
  EXPECT_EQ("", output);
}

// END in the function ends the native function that called it, then the
// script, without an error.
TEST_F(InterpreterCallTest, EndInCallback) {
  const std::string script = R"(
def f(i)
  print i
  if i = 2 then
    end
  endif
  return TRUE
enddef
x = each("f", 5)
print "after"
)";
  EXPECT_EQ("1\n2\n", run(script));
  EXPECT_TRUE(errors.empty()) << errors.front();
  EXPECT_EQ(2, calls);
}

TEST_F(InterpreterCallTest, UnknownFunction) {
  run("print apply(\"nosuch\", 1)\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:1: error: Unknown function 'nosuch'", errors.front());
}

TEST_F(InterpreterCallTest, WrongArgumentCount) {
  run("def f(a, b)\nreturn a\nenddef\nprint apply(\"f\", 1)\n");
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ("test.bas:4: error: f: expected 2 argument(s), got 1", errors.front());
}

TEST_F(InterpreterCallTest, NoScriptRunning) {
  EXPECT_THROW(interp.call("f", {}), BasicError);
  run("def f()\nreturn 1\nenddef\n");
  EXPECT_TRUE(errors.empty());
  EXPECT_THROW(interp.call("f", {}), BasicError);
}

// Recursion through native code counts toward the call depth limit, so it
// ends with an error instead of running out of stack.
TEST_F(InterpreterCallTest, Recursion) {
  const std::string script = R"(
def f(n)
  if n = 0 then
    return 0
  endif
  return apply("f", n - 1) + 1
enddef
print f(500)
print f(5000)
)";
  EXPECT_EQ("500\n", run(script));
  ASSERT_EQ(1u, errors.size());
  EXPECT_NE(std::string::npos, errors.front().find("too many nested calls")) << errors.front();
}

// The stop check (WWIV's hang-up check) runs in the function's statements.
TEST_F(InterpreterCallTest, StopCheck) {
  int statements{0};
  interp.set_stop_check([&statements] { return ++statements > 20; });
  const auto result =
      interp.run_source("test.bas", "def f(i)\nreturn TRUE\nenddef\nx = each(\"f\", 1000)\n");
  EXPECT_TRUE(result.stopped);
  EXPECT_TRUE(result.errors.empty());
  EXPECT_LT(calls, 1000);
}
