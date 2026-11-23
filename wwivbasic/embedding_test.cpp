// Uses only the public embedding API (the headers at the top of wwivbasic/), as an embedder
// such as WWIV would.

#include "gtest/gtest.h"
#include "wwivbasic/interpreter.h"
#include "wwivbasic/stdio_console.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
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

TEST(EmbeddingTest, SecondRunIsAnError) {
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);

  ASSERT_TRUE(interp.run_source("one.bas", "print 1\n").ok());
  const auto result = interp.run_source("two.bas", "print 2\n");
  ASSERT_EQ(1u, result.errors.size());
  EXPECT_EQ("two.bas: error: this Interpreter has already run a script; use a new one",
            result.errors.front());
  // The second script didn't run.
  EXPECT_EQ("1\n", out.str());
}

// Objects: a fake message base bound the way WWIV's SDK bindings will be.

namespace {

struct Header {
  int number{0};
  std::string from;
  std::string subject;
  std::string summary(int width) const { return subject.substr(0, static_cast<size_t>(width)); }
};

struct Area {
  std::string name;
  std::vector<Header> headers;
  int count() const { return static_cast<int>(headers.size()); }
};

class ObjectTest : public ::testing::Test {
protected:
  ObjectTest() {
    area_ = std::make_shared<Area>(Area{
        "general", {{1, "Rushfan", "Hello"}, {2, "Sysop", "Welcome to the BBS"}}});

    auto& msgs = interp.add_module("msgs");
    auto& header = msgs.class_<Header>("header");
    header.property("number", &Header::number)
        .property("from", [](const Header& h) { return h.from; })
        .property("subject", &Header::subject, &Header::subject)
        .method("summary", &Header::summary);

    auto& area = msgs.class_<Area>("area");
    area.property("name", &Area::name)
        .method("count", &Area::count)
        // Each header object points into the area (the shared_ptr aliasing
        // constructor), so changes show up in the area and it stays alive.
        .method("header",
                [this, &header](Area&, int i) {
                  return header.wrap(std::shared_ptr<Header>(
                      area_, &area_->headers.at(static_cast<size_t>(i))));
                })
        .method("headers", [this, &header](Area&) {
          std::vector<wwivbasic::Value> result;
          for (auto& h : area_->headers) {
            result.push_back(header.wrap(std::shared_ptr<Header>(area_, &h)));
          }
          return result;
        });

    msgs.native_function("OPEN_AREA", [this, &area](std::string name) {
      if (name != area_->name) {
        throw BasicError("No such area: " + name);
      }
      return area.wrap(area_);
    });
    // A native function can get the C++ object back from a script value.
    msgs.native_function("SUBJECT_OF", [&header](wwivbasic::Value v) {
      auto h = header.unwrap(v);
      return h ? h->subject : std::string("(not a header)");
    });
  }

  RunResult run(const std::string& script) { return interp.run_source("viewer.bas", script); }

  std::shared_ptr<Area> area_;
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console{out, in};
  Interpreter interp{console};
};

} // namespace

TEST_F(ObjectTest, MessageViewer) {
  const auto result = run(R"bas(
import "@msgs"
area = open_area("general")
print area.name; " ("; area.count(); " messages)"
for m in area.headers()
  print m.number; " "; m.from; ": "; m.subject
next
print area.header(1).summary(7)
)bas");
  ASSERT_TRUE(result.ok()) << result.errors.front();
  EXPECT_EQ("general (2 messages)\n"
            "1 Rushfan: Hello\n"
            "2 Sysop: Welcome to the BBS\n"
            "Welcome\n",
            out.str());
}

TEST_F(ObjectTest, ReadWritePropertyChangesTheObject) {
  const auto result = run(R"bas(
area = msgs.open_area("general")
h = area.header(0)
h.SUBJECT = "Re: Hello"
print area.header(0).subject; " / "; msgs.subject_of(h); " / "; msgs.subject_of(42)
)bas");
  ASSERT_TRUE(result.ok()) << result.errors.front();
  EXPECT_EQ("Re: Hello / Re: Hello / (not a header)\n", out.str());
  EXPECT_EQ("Re: Hello", area_->headers[0].subject);
}

TEST_F(ObjectTest, ObjectsAreReferences) {
  const auto result = run(R"bas(
a = msgs.open_area("general")
b = a
print a = b; " "; a.header(0) = a.header(0); " "; a
)bas");
  ASSERT_TRUE(result.ok()) << result.errors.front();
  // Two wraps of the same C++ header are equal; printing shows the class.
  EXPECT_EQ("TRUE TRUE area\n", out.str());
}

TEST_F(ObjectTest, Errors) {
  auto expect_error = [](const RunResult& r, const std::string& message) {
    ASSERT_EQ(1u, r.errors.size());
    EXPECT_EQ("viewer.bas:2: error: " + message, r.errors.front());
  };
  expect_error(run("a = msgs.open_area(\"general\")\nprint a.nosuch\n"),
               "area has no member 'nosuch'");
}

TEST_F(ObjectTest, ReadOnlyProperty) {
  const auto r = run("a = msgs.open_area(\"general\")\na.name = \"x\"\n");
  ASSERT_EQ(1u, r.errors.size());
  EXPECT_EQ("viewer.bas:2: error: area.name is read-only", r.errors.front());
}

TEST_F(ObjectTest, MethodWithoutParens) {
  const auto r = run("a = msgs.open_area(\"general\")\nprint a.count\n");
  ASSERT_EQ(1u, r.errors.size());
  EXPECT_EQ("viewer.bas:2: error: area.count is a method; call it with ()", r.errors.front());
}

TEST_F(ObjectTest, PropertyCalledAsMethod) {
  const auto r = run("a = msgs.open_area(\"general\")\nprint a.name()\n");
  ASSERT_EQ(1u, r.errors.size());
  EXPECT_EQ("viewer.bas:2: error: area.name is a property, not a method; use it without ()",
            r.errors.front());
}

TEST_F(ObjectTest, MethodArgumentCount) {
  const auto r = run("a = msgs.open_area(\"general\")\nprint a.header()\n");
  ASSERT_EQ(1u, r.errors.size());
  EXPECT_EQ("viewer.bas:2: error: area.header: expected 1 argument(s), got 0", r.errors.front());
}

TEST_F(ObjectTest, ErrorFromNativeCode) {
  const auto r = run("print 1\na = msgs.open_area(\"nosuch\")\n");
  ASSERT_EQ(1u, r.errors.size());
  EXPECT_EQ("viewer.bas:2: error: No such area: nosuch", r.errors.front());
}

TEST_F(ObjectTest, ObjectsCantBeUsedInArithmeticOrSorted) {
  const auto r = run("a = msgs.open_area(\"general\")\nprint a + 1\n");
  ASSERT_EQ(1u, r.errors.size());
  EXPECT_EQ("viewer.bas:2: error: Operator '+' cannot be used with an object of class area",
            r.errors.front());
}

TEST(EmbeddingTest, NativeFunctionTakingConstStringRef) {
  // Parameters declared as const std::string& used to bind to a destroyed
  // temporary.
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);
  interp.globals().native_function(
      "SHOUT", [](const std::string& s) { return s + std::string(3, '!'); });
  ASSERT_TRUE(interp.run_source("t.bas", "print shout(\"a fairly long string, past SSO\")\n").ok());
  EXPECT_EQ("a fairly long string, past SSO!!!\n", out.str());
}

// PR #7 review fixes.

TEST_F(ObjectTest, WrapNullIsAScriptError) {
  auto& msgs = interp.add_module("msgs");
  auto& nothing = msgs.class_<Header>("nothing");
  msgs.native_function("FIND", [&nothing]() { return nothing.wrap(std::shared_ptr<Header>()); });
  const auto r = run("print 1\nh = msgs.find()\n");
  ASSERT_EQ(1u, r.errors.size());
  EXPECT_EQ("viewer.bas:2: error: nothing: no object (null)", r.errors.front());
}

TEST_F(ObjectTest, AddModuleAgainKeepsBindings) {
  // Getting the module again must not destroy its classes, which the native
  // functions registered in the fixture captured by reference.
  auto& again = interp.add_module("msgs");
  again.native_function("EXTRA", []() { return 1; });
  const auto r = run("a = msgs.open_area(\"general\")\nprint a.header(0).subject; msgs.extra()\n");
  ASSERT_TRUE(r.ok()) << r.errors.front();
  EXPECT_EQ("Hello1\n", out.str());
}

TEST_F(ObjectTest, CppExceptionsBecomeScriptErrors) {
  // area.header(5) uses std::vector::at, which throws std::out_of_range.
  const auto r = run("a = msgs.open_area(\"general\")\nprint a.header(5)\n");
  ASSERT_EQ(1u, r.errors.size());
  EXPECT_EQ(0u, r.errors.front().rfind("viewer.bas:2: error: area.header: ", 0))
      << r.errors.front();
}

TEST_F(ObjectTest, ExistsFindsTheSameObject) {
  const auto r = run(
      "a = msgs.open_area(\"general\")\n"
      "print exists(a.headers(), a.header(1)); index_of(a.headers(), a.header(1))\n");
  ASSERT_TRUE(r.ok()) << r.errors.front();
  EXPECT_EQ("TRUE1\n", out.str());
}

TEST_F(ObjectTest, SetterRejectsObjects) {
  const auto r = run("a = msgs.open_area(\"general\")\nh = a.header(0)\nh.subject = a\n");
  ASSERT_EQ(1u, r.errors.size());
  EXPECT_EQ("viewer.bas:3: error: header.subject: expected a STRING, got an object of class area",
            r.errors.front());
}

namespace {

struct Counter {
  int n{0};
  int next() noexcept { return ++n; }
};

// Runs a one-line script with a fresh interpreter; returns output or errors.
std::string run_one(const std::function<void(Interpreter&)>& setup, const std::string& script) {
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);
  setup(interp);
  const auto r = interp.run_source("t.bas", script);
  return r.ok() ? out.str() : r.errors.front();
}

} // namespace

TEST(EmbeddingTest, NativeExceptionsBecomeScriptErrors) {
  auto setup = [](Interpreter& i) {
    i.globals().native_function("FAIL", []() -> int { throw std::runtime_error("disk full"); });
    i.globals().native_function("WEIRD", []() -> int { throw 42; });
  };
  EXPECT_EQ("t.bas:1: error: FAIL: disk full", run_one(setup, "print fail()\n"));
  EXPECT_EQ("t.bas:1: error: WEIRD: unknown C++ exception", run_one(setup, "print weird()\n"));
}

TEST(EmbeddingTest, WideIntegers) {
  auto setup = [](Interpreter& i) {
    i.globals().native_function("BIG", []() { return std::int64_t{3000000000}; });
    i.globals().native_function("SMALL", []() { return std::int64_t{42}; });
    i.globals().native_function("U32", []() { return std::uint32_t{4000000000u}; });
    i.globals().native_function("ECHO64", [](std::int64_t v) { return v; });
    i.globals().native_function("NEG_U32", [](std::uint32_t v) { return v; });
  };
  // Too big for an INTEGER: a REAL instead of a truncated value.
  EXPECT_EQ("3000000000 42 4000000000\n",
            run_one(setup, "print big(); \" \"; small(); \" \"; u32()\n"));
  EXPECT_EQ("TRUE TRUE\n", run_one(setup, "print echo64(big()) = big(); \" \"; neg_u32(-5) = 0\n"));
}

TEST(EmbeddingTest, NoexceptFunctionsAndMethods) {
  auto setup = [](Interpreter& i) {
    auto& m = i.add_module("m");
    auto& counter = m.class_<Counter>("counter");
    counter.method("next", &Counter::next)
        .method("peek", [](const Counter& c) noexcept { return c.n; });
    m.native_function("NEW", [&counter]() noexcept { return counter.wrap(Counter{}); });
  };
  EXPECT_EQ("121\n", run_one(setup, "c = m.new()\nprint c.next(); c.next(); c.peek() - 1\n"));
}

TEST(EmbeddingTest, VectorOfBool) {
  auto setup = [](Interpreter& i) {
    i.globals().native_function("FLAGS", []() { return std::vector<bool>{true, false}; });
  };
  EXPECT_EQ("2 TRUE FALSE\n",
            run_one(setup, "f = flags()\nprint len(f); \" \"; f(0); \" \"; f(1)\n"));
}

TEST(EmbeddingTest, ListArgumentWhereStringExpected) {
  auto setup = [](Interpreter& i) {
    i.globals().native_function("SHOUT", [](const std::string& s) { return s + "!"; });
  };
  EXPECT_EQ("t.bas:1: error: SHOUT: expected a STRING, got a LIST",
            run_one(setup, "print shout(list())\n"));
}

TEST(EmbeddingTest, KeywordsAsMemberNames) {
  // Names after a dot can be keywords: module functions like wwiv.io.print,
  // and methods or properties like obj.next() and obj.end.
  auto setup = [](Interpreter& i) {
    i.add_module("wwiv.io").native_function("PRINT", [](std::string s) { return "[" + s + "]"; });
    auto& m = i.add_module("m");
    auto& counter = m.class_<Counter>("counter");
    counter.method("next", &Counter::next).property("end", [](const Counter& c) { return c.n; });
    m.native_function("NEW", [&counter]() { return counter.wrap(Counter{}); });
  };
  EXPECT_EQ("[x]1 1\n",
            run_one(setup, "c = m.new()\nprint wwiv.io.print(\"x\"); c.next(); \" \"; c.end\n"));
}

TEST(EmbeddingTest, StopCheck_StopsBetweenStatements) {
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);
  int calls = 0;
  // Stop once two statements have run.
  interp.set_stop_check([&calls]() { return ++calls > 2; });

  const auto result = interp.run_source("t.bas", "print 1\nprint 2\nprint 3\n");
  EXPECT_TRUE(result.stopped);
  EXPECT_TRUE(result.errors.empty());
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("1\n2\n", out.str());
}

TEST(EmbeddingTest, StopCheck_StopsAnEmptyLoop) {
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);
  int calls = 0;
  interp.set_stop_check([&calls]() { return ++calls > 1000; });

  const auto result = interp.run_source("t.bas", "while 1\nwend\nprint \"never\"\n");
  EXPECT_TRUE(result.stopped);
  EXPECT_EQ("", out.str());
}

TEST(EmbeddingTest, StopCheck_StopsAFunctionCalledFromNativeCode) {
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);
  bool stop = false;
  interp.globals().native_function("HANGUP", [&stop]() { stop = true; });
  interp.set_stop_check([&stop]() { return stop; });

  const auto result = interp.run_source("t.bas", R"(
def f()
  hangup()
  print "not reached"
enddef
f()
print "not reached either"
)");
  EXPECT_TRUE(result.stopped);
  EXPECT_TRUE(result.errors.empty());
  EXPECT_EQ("", out.str());
}

TEST(EmbeddingTest, StopCheck_DefaultNeverStops) {
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);
  const auto result = interp.run_source("t.bas", "for i = 1 to 3\nprint i;\nnext\n");
  EXPECT_TRUE(result.ok());
  EXPECT_FALSE(result.stopped);
  EXPECT_EQ("123", out.str());
}

namespace {
// A directory with some .bas files for the import tests, removed afterwards.
class ImportDir {
public:
  ImportDir() {
    root_ = std::filesystem::temp_directory_path() /
            ("wwivbasic_import_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
    std::filesystem::create_directories(root_ / "scripts" / "sub");
    write("scripts/lib.bas", "def greet()\n  return \"hi\"\nenddef\n");
    write("scripts/sub/inner.bas", "def inner()\n  return \"in\"\nenddef\n");
    write("secret.bas", "def secret()\n  return \"leak\"\nenddef\n");
  }
  ~ImportDir() {
    std::error_code ec;
    std::filesystem::remove_all(root_, ec);
  }
  void write(const std::string& name, const std::string& text) const {
    std::ofstream f(root_ / name);
    f << text;
  }
  std::filesystem::path scripts() const { return root_ / "scripts"; }
  std::filesystem::path root() const { return root_; }

private:
  std::filesystem::path root_;
};
} // namespace

TEST(EmbeddingTest, ImportRoot_AllowsFilesUnderIt) {
  ImportDir dir;
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);
  interp.set_import_root(dir.scripts());

  const auto main = (dir.scripts() / "main.bas").string();
  const auto result = interp.run_source(
      main, "import \"lib.bas\"\nimport \"sub/inner.bas\"\nprint greet(); inner()\n");
  ASSERT_TRUE(result.ok()) << result.errors.front();
  EXPECT_EQ("hiin\n", out.str());
}

TEST(EmbeddingTest, ImportRoot_TrailingSeparator) {
  ImportDir dir;
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);
  interp.set_import_root(dir.scripts().string() + "/");

  const auto main = (dir.scripts() / "main.bas").string();
  const auto result = interp.run_source(main, "import \"lib.bas\"\nprint greet()\n");
  ASSERT_TRUE(result.ok()) << result.errors.front();
  EXPECT_EQ("hi\n", out.str());
}

TEST(EmbeddingTest, ImportRoot_RejectsParent) {
  ImportDir dir;
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);
  interp.set_import_root(dir.scripts());

  const auto main = (dir.scripts() / "main.bas").string();
  const auto result = interp.run_source(main, "import \"../secret.bas\"\nprint secret()\n");
  ASSERT_EQ(1u, result.errors.size());
  EXPECT_NE(std::string::npos, result.errors.front().find("Can't import '../secret.bas'"))
      << result.errors.front();
  EXPECT_EQ("", out.str());
}

TEST(EmbeddingTest, ImportRoot_RejectsAbsolutePath) {
  ImportDir dir;
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);
  interp.set_import_root(dir.scripts());

  const auto secret = (dir.root() / "secret.bas").generic_string();
  const auto main = (dir.scripts() / "main.bas").string();
  const auto result = interp.run_source(main, "import \"" + secret + "\"\nprint secret()\n");
  ASSERT_EQ(1u, result.errors.size());
  EXPECT_NE(std::string::npos, result.errors.front().find("Can't import")) << result.errors.front();
}

TEST(EmbeddingTest, ImportRoot_DefaultAllowsAnyFile) {
  ImportDir dir;
  std::ostringstream out;
  std::istringstream in;
  StdioConsole console(out, in);
  Interpreter interp(console);

  const auto main = (dir.scripts() / "main.bas").string();
  const auto result = interp.run_source(main, "import \"../secret.bas\"\nprint secret()\n");
  ASSERT_TRUE(result.ok()) << result.errors.front();
  EXPECT_EQ("leak\n", out.str());
}
