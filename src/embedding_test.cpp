// Uses only the public embedding API (include/wwivbasic/), as an embedder
// such as WWIV would.

#include "gtest/gtest.h"
#include "wwivbasic/interpreter.h"
#include "wwivbasic/stdio_console.h"

#include <sstream>
#include <string>

using namespace wwivbasic;

TEST(EmbeddingTest, ModuleAndConsole) {
  std::ostringstream out;
  std::istringstream in("world\nY\n");
  StdioConsole console(out, in);
  Interpreter interp(console);

  auto& m = interp.add_module("my.io");
  m.native_function("HELLO", [](std::string name) { return "Hello, " + name; });
  m.native_function("ASK", [&console]() { return console.read_line(3); });

  const auto result = interp.run_source("embed.bas", R"(
import "@my.io"
print hello(ask())
print my.io.ask()
)");
  ASSERT_TRUE(result.ok()) << result.errors.front();
  // read_line(3) cuts "world" to "wor".
  EXPECT_EQ("Hello, wor\nY\n", out.str());
}

TEST(EmbeddingTest, GlobalsAreCallableWithoutPrefix) {
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);
  interp.globals().native_function("TWICE", [](int n) { return n * 2; });

  ASSERT_TRUE(interp.run_source("t.bas", "print twice(21)\n").ok());
  EXPECT_EQ("42\n", out.str());
}

TEST(EmbeddingTest, ErrorsHaveLocations) {
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);

  const auto result = interp.run_source("bad.bas", "print 1\nnosuch()\n");
  ASSERT_EQ(1u, result.errors.size());
  EXPECT_EQ("bad.bas:2: error: Unknown function 'nosuch'", result.errors.front());
}

TEST(EmbeddingTest, CheckOnlyDoesNotRun) {
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);

  RunOptions options;
  options.execute = false;
  ASSERT_TRUE(interp.run_source("t.bas", "print \"x\"\n", options).ok());
  EXPECT_EQ("", out.str());
  EXPECT_TRUE(interp.parse_tree("t.bas").has_value());
}

TEST(StdioConsoleTest, ReadKeyAndEndOfInput) {
  std::ostringstream out;
  std::istringstream in("yes\r\n");
  StdioConsole console(out, in);
  EXPECT_EQ("y", console.read_key());
  EXPECT_EQ("", console.read_line(0));
  console.clear_screen();
  EXPECT_EQ("[cls]\n", out.str());
}
