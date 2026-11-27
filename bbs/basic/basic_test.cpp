/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2022, WWIV Software Services               */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
// ReSharper disable CppClangTidyCppcoreguidelinesMacroUsage
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "bbs/basic/next/next_engine.h"
#include "bbs/bbs.h"
#include "bbs/bbs_helper.h"
#include "common/input.h"
#include "common/output.h"
#include "core/strings.h"
#include "core/version.h"
#include "fmt/format.h"
#include "wwivbasic/interpreter.h"
#include <cmath>
#include <optional>
#include <string>
#include <vector>

using namespace wwiv::bbs;
using namespace wwiv::common;
using namespace wwiv::bbs::basic;
using namespace wwiv::strings;

// Scripts run through NextEngine, as the BBS runs them, with an ASSERT module.
// These began as the legacy (my_basic) engine's tests, and ran on both
// engines until it was removed.
class BasicTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    // Enable the FILE and OS package.
    helper.config().scripting_enabled(true);
    helper.config().script_package_file_enabled(true);
    helper.config().script_package_os_enabled(true);
  }

  [[nodiscard]] bool RunScript(const std::string& text) {
    const auto* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    const auto module = StrCat(test_info->test_suite_name(), "#", test_info->name());
    NextEngine engine(bin, bout, *a()->config(), &helper.context(), RegisterAssertModule);
    return engine.RunScript(module, text);
  }

  // ASSERT.EQUALS and ASSERT.NE: values of different types are never equal,
  // and REALs are equal within 0.001. A BOOLEAN compares as the INTEGER 0 or
  // 1, as my_basic's ASSERT did (it had no booleans).
  static void RegisterAssertModule(wwivbasic::Interpreter& interp) {
    using wwivbasic::Value;
    auto normalize = [](const Value& v) {
      return v.value_type() == Value::Type::BOOLEAN ? Value(v.toBool() ? 1 : 0) : v;
    };
    // Why a and b differ, or nullopt if they are equal.
    auto differ = [normalize](const Value& a0, const Value& b0) -> std::optional<std::string> {
      const auto a = normalize(a0);
      const auto b = normalize(b0);
      if (a.value_type() != b.value_type()) {
        return fmt::format("different types: {} and {}", a.a_type_name(), b.a_type_name());
      }
      switch (a.value_type()) {
      case Value::Type::INTEGER:
        if (a.toInt() == b.toInt()) {
          return std::nullopt;
        }
        break;
      case Value::Type::REAL:
        if (std::abs(a.toDouble() - b.toDouble()) < 0.001) {
          return std::nullopt;
        }
        break;
      case Value::Type::STRING:
        if (a.toString() == b.toString()) {
          return std::nullopt;
        }
        break;
      case Value::Type::BOOLEAN:
      case Value::Type::LIST:
      case Value::Type::ITERATOR:
      case Value::Type::OBJECT:
        return fmt::format("can't compare {}", a.a_type_name());
      }
      return fmt::format("'{}' != '{}'", a.toString(), b.toString());
    };
    // The error says why, so tests can tell an ASSERT failure from others.
    auto check = [differ](const char* fn, bool want_equal) {
      return [=](const std::vector<Value>& args) {
        if (args.size() < 2 || args.size() > 3) {
          throw wwivbasic::BasicError(fmt::format("{}: expected 2 or 3 arguments", fn));
        }
        const auto why = differ(args[0], args[1]);
        const auto msg = args.size() > 2 ? " " + args[2].toString() : std::string();
        if (want_equal && why) {
          throw wwivbasic::BasicError(fmt::format("{} failed: {}{}", fn, *why, msg));
        }
        if (!want_equal && !why) {
          throw wwivbasic::BasicError(
              fmt::format("{} failed: both are '{}'{}", fn, args[0].toString(), msg));
        }
        return Value();
      };
    };
    auto& m = interp.add_module("ASSERT");
    m.native_functionl("EQUALS", check("ASSERT.EQUALS", true));
    m.native_functionl("NE", check("ASSERT.NE", false));
  }

  // After a script that should fail on an ASSERT: checks that it was the
  // ASSERT that failed, and why, not some other error.
  void ExpectAssertFailure(const std::string& why) {
    EXPECT_THAT(helper.io()->captured(), testing::HasSubstr("ASSERT.EQUALS failed: " + why));
  }

  BbsHelper helper{};
};

#define BASIC_ASSERT_EQUALS(v1, v2)                                                                \
  do {                                                                                             \
    const auto __s = fmt::format(R"( ASSERT.EQUALS({}, {}) )", v1, v2);                            \
    EXPECT_TRUE(RunScript(__s));                                                                   \
  } while (0)

#define BASIC_ASSERT_NE(v1, v2)                                                                \
  do {                                                                                             \
    const auto __s = fmt::format(R"( ASSERT.NE({}, {}) )", v1, v2);                            \
    EXPECT_TRUE(RunScript(__s));                                                                   \
  } while (0)

TEST_F(BasicTest, Smoke) {
  EXPECT_TRUE(RunScript("print \"Hello World\""));
  EXPECT_EQ("Hello World", StringTrim(helper.io()->captured()));
}

TEST_F(BasicTest, Smoke_Fail) {
  // There is no outstr( function.
  EXPECT_FALSE(RunScript("outstr( \"Hello World\""));

  // Script errors are echoed to the caller so they can see why it failed.
  const auto captured = StringTrim(helper.io()->captured());
  // Errors are "name:line[:column]: error: message". It's the parse error,
  // not some other failure.
  EXPECT_THAT(captured, testing::StartsWith("BasicTest#Smoke_Fail:1:22: error: missing ')'"));
}

TEST_F(BasicTest, Assert_Int_False) {
  BASIC_ASSERT_NE(1, 2);
  EXPECT_FALSE(RunScript("ASSERT.EQUALS(1, 2)"));
  ExpectAssertFailure("'1' != '2'");
}

TEST_F(BasicTest, Assert_Int_True) {
  BASIC_ASSERT_EQUALS(1, 1);
  EXPECT_TRUE(RunScript("ASSERT.EQUALS(1, 1)"));
}

TEST_F(BasicTest, Assert_String_False) {
  BASIC_ASSERT_NE("\"hello\"", "\"world\"");
}

TEST_F(BasicTest, Assert_String_True) {
  BASIC_ASSERT_EQUALS("\"hello\"", "\"hello\"");
}

TEST_F(BasicTest, Assert_Real_False) {
  EXPECT_FALSE(RunScript("ASSERT.EQUALS(1.1, 1.2)"));
  ExpectAssertFailure("'1.1' != '1.2'");
}

TEST_F(BasicTest, Assert_Real_True) {
  EXPECT_TRUE(RunScript("ASSERT.EQUALS(1.1, 1.1)"));
}

TEST_F(BasicTest, Assert_DifferentTypes) {
  EXPECT_FALSE(RunScript(R"( ASSERT.EQUALS("1.1", 1.1) )"));
  ExpectAssertFailure("different types: a STRING and a REAL");
}

TEST_F(BasicTest, ModuleNames) {
  EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS("wwiv", wwiv.module_name()) )"));
  EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS("wwiv.io", wwiv.io.module_name()) )"));
  EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS("wwiv.io.file", wwiv.io.file.module_name()) )"));
  EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS("wwiv.data", wwiv.data.module_name()) )"));
  EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS("wwiv.os", wwiv.os.module_name()) )"));
}

TEST_F(BasicTest, WWIV_Version) {
  const auto script = fmt::format(R"(ASSERT.EQUALS("{}", wwiv.version()) )",
                                  wwiv::core::short_version());
  EXPECT_TRUE(RunScript(script));
}

TEST_F(BasicTest, WWIV_Version_Import) {
  const auto script = fmt::format(R"(
import "@wwiv"
ASSERT.EQUALS("{}", version())
)", wwiv::core::short_version());
  EXPECT_TRUE(RunScript(script));
}

TEST_F(BasicTest, WWIV_IO_PrintFile) {
  helper.files().CreateTempFile("gfiles/file1.msg", "this is file1");
  EXPECT_TRUE(RunScript("wwiv.io.printfile(\"file1.msg\")"));
  EXPECT_EQ("this is file1\r\n", helper.io()->captured());
}

TEST_F(BasicTest, WWIV_IO_PrintFile_DoesNotExist) {
  EXPECT_TRUE(RunScript("wwiv.io.printfile(\"file1.msg\")"));
  EXPECT_EQ("", helper.io()->captured());
}

TEST_F(BasicTest, WWIV_IO_Outstr) {
  EXPECT_TRUE(RunScript("wwiv.io.outstr(\"Hello World\")"));
  EXPECT_EQ("Hello World", helper.io()->captured());
}

TEST_F(BasicTest, WWIV_IO_Puts) {
  EXPECT_TRUE(RunScript("wwiv.io.puts(\"Hello World\")"));
  EXPECT_EQ("Hello World", helper.io()->captured());
}

TEST_F(BasicTest, WWIV_IO_PL) {
  EXPECT_TRUE(RunScript("wwiv.io.pl(\"Hello World\")"));
  EXPECT_EQ("Hello World\r\n", helper.io()->captured());
}

TEST_F(BasicTest, WWIV_Eval_Pass) {
  helper.user()->sl(200);
  EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS(wwiv.eval("user.sl > 100"), TRUE) )"));
  // ASSERT compares a BOOLEAN as an INTEGER, so also check what PRINT shows:
  // EVAL returns the integer 1, as in my_basic, not TRUE.
  helper.io()->Clear();
  EXPECT_TRUE(RunScript(R"( print wwiv.eval("user.sl > 100") )"));
  EXPECT_EQ("1\r\n", helper.io()->captured());
}

TEST_F(BasicTest, WWIV_Eval_Fail) {
  helper.user()->sl(200);
  EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS(wwiv.eval("user.sl < 200"), FALSE) )"));
  helper.io()->Clear();
  EXPECT_TRUE(RunScript(R"( print wwiv.eval("user.sl < 200") )"));
  EXPECT_EQ("0\r\n", helper.io()->captured());
}

TEST_F(BasicTest, WWIV_IO_FILE_ReadIntoString) {
  const std::string actual = "this is file1";
  helper.files().CreateTempFile("gfiles/file1.msg", actual);
  EXPECT_TRUE(RunScript(R"( 
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "file1.msg", "R")
s = wwiv.io.file.readintostring(h)
print s
)" ));
  EXPECT_EQ(StrCat(actual, "\r\n"), helper.io()->captured());
}

TEST_F(BasicTest, WWIV_IO_FILE_ReadLines) {
  const std::string expected = "1\r\n2\r\n3\r\n";
  helper.files().CreateTempFile("gfiles/file1.msg", expected);
  EXPECT_TRUE(RunScript(R"(

def PrintList(l)
  i = iterator(l)
  while move_next(i)
    print get(i)
  wend
enddef

lines = list()
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "file1.msg", "R")
wwiv.io.file.readlines(h, lines)
PrintList (lines)

)" ));
  const auto ex = SplitString(expected, "\r\n", true);
  EXPECT_EQ(3, wwiv::stl::ssize(ex));

  const auto actual = SplitString(helper.io()->captured(), "\r\n", true);
  EXPECT_THAT(actual, testing::ElementsAre("1", "2", "3"));
}
