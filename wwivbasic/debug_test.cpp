// The debugger API (Interpreter::set_statement_hook, call_stack, variables,
// source), through the public embedding headers only.

#include "fmt/format.h"
#include "wwivbasic/interpreter.h"
#include "wwivbasic/stdio_console.h"
#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace wwivbasic;

namespace {

class DebugTest : public ::testing::Test {
protected:
  DebugTest() : console_(out_, in_), interp_(console_) {}

  // Records each statement's location as "file:line:column@depth".
  void record_locations() {
    interp_.set_statement_hook([this](const DebugLocation& loc) {
      locations_.push_back(fmt::format("{}:{}:{}@{}", loc.file, loc.line, loc.column, loc.depth));
      return false;
    });
  }

  std::ostringstream out_;
  std::istringstream in_;
  StdioConsole console_;
  Interpreter interp_;
  std::vector<std::string> locations_;
};

} // namespace

TEST_F(DebugTest, HookSeesEachStatementWithItsDepth) {
  record_locations();
  const auto result = interp_.run_source("t.bas", R"(def f(x)
  return x + 1
enddef
a = f(1)
  print a
)");
  ASSERT_TRUE(result.ok()) << result.errors.front();
  EXPECT_EQ(std::vector<std::string>({"t.bas:4:1@0", "t.bas:2:3@1", "t.bas:5:3@0"}), locations_);
  EXPECT_EQ("2\n", out_.str());
}

TEST_F(DebugTest, HookCanStopTheScript) {
  interp_.set_statement_hook([](const DebugLocation& loc) { return loc.line == 2; });
  const auto result = interp_.run_source("t.bas", "print 1\nprint 2\n");
  EXPECT_TRUE(result.stopped);
  EXPECT_TRUE(result.errors.empty());
  EXPECT_EQ("1\n", out_.str());
}

TEST_F(DebugTest, CallStackIsInnermostFirst) {
  std::vector<DebugFrame> stack;
  interp_.set_statement_hook([&](const DebugLocation& loc) {
    if (loc.line == 5) {
      stack = interp_.call_stack();
    }
    return false;
  });
  const auto result = interp_.run_source("t.bas", R"(def outer()
  inner()
enddef
def inner()
  print "in"
enddef
outer()
)");
  ASSERT_TRUE(result.ok()) << result.errors.front();
  ASSERT_EQ(3u, stack.size());
  EXPECT_EQ("inner", stack[0].function);
  EXPECT_EQ(5, stack[0].line);
  EXPECT_EQ("outer", stack[1].function);
  EXPECT_EQ(2, stack[1].line);
  EXPECT_EQ("", stack[2].function);
  EXPECT_EQ(7, stack[2].line);
  EXPECT_EQ("t.bas", stack[2].file);
  // Empty once the script has finished.
  EXPECT_TRUE(interp_.call_stack().empty());
}

TEST_F(DebugTest, VariablesOfEachFrame) {
  auto vars = std::vector<std::string>();
  interp_.set_statement_hook([&](const DebugLocation& loc) {
    if (loc.line == 3 && vars.empty()) {
      for (const auto& v : interp_.variables()) {
        vars.push_back(fmt::format("{}:{}:{}={}", v.frame, v.name, v.type, v.value));
      }
    }
    return false;
  });
  const auto result = interp_.run_source("t.bas", R"(def f(n)
  local = n * 2
  print local
enddef
g = "top"
l = list(1, "two", list(3))
f(21)
)");
  ASSERT_TRUE(result.ok()) << result.errors.front();
  EXPECT_EQ(std::vector<std::string>({"0:local:INTEGER=42", "0:n:INTEGER=21", "1:g:STRING=top",
                                      R"(1:l:LIST=[1, "two", [3]])"}),
            vars);
}

TEST_F(DebugTest, ImportedFilesHaveTheirOwnFrameAndSource) {
  const auto dir = std::filesystem::temp_directory_path() / "wwivbasic_debug_test";
  std::filesystem::create_directories(dir);
  const auto lib = (dir / "lib.bas").string();
  const auto main = (dir / "main.bas").string();
  {
    std::ofstream f(lib);
    f << "libvar = 1\n";
  }
  record_locations();
  const auto result = interp_.run_source(main, "import \"lib.bas\"\nprint libvar\n");
  ASSERT_TRUE(result.ok()) << result.errors.front();
  EXPECT_EQ(std::vector<std::string>({main + ":1:1@0", lib + ":1:1@1", main + ":2:1@0"}),
            locations_);
  EXPECT_EQ("libvar = 1\n", interp_.source(lib).value_or("(none)"));
  EXPECT_EQ("import \"lib.bas\"\nprint libvar\n", interp_.source(main).value_or("(none)"));
  EXPECT_FALSE(interp_.source("nosuch.bas").has_value());
  std::filesystem::remove_all(dir);
}

TEST(DebugStringTest, Values) {
  EXPECT_EQ("abc", debug_string(Value("abc")));
  EXPECT_EQ("42", debug_string(Value(42)));
  EXPECT_EQ("TRUE", debug_string(Value(true)));
  EXPECT_EQ("[]", debug_string(Value::new_list()));
  EXPECT_EQ(R"([1, "a"])", debug_string(Value::make_list({Value(1), Value("a")})));

  // A list that contains itself stops at the depth limit.
  auto l = Value::new_list();
  l.list("t")->items.push_back(l);
  EXPECT_EQ("[[[[[...]]]]]", debug_string(l));

  std::vector<Value> many;
  for (int i = 0; i < 102; i++) {
    many.emplace_back(i);
  }
  const auto s = debug_string(Value::make_list(many));
  EXPECT_TRUE(s.ends_with(", 99, ... (2 more)]")) << s;
}
