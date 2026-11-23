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

#include "bbs/basic/basic.h"
#include "bbs/basic/script_engine.h"
#include "bbs/basic/util.h"
#include "bbs/bbs_helper.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/version.h"
#include "deps/my_basic/core/my_basic.h"
#include "fmt/format.h"
#include <cmath>
#include <optional>
#include <iostream>
#include <string>
#include <vector>

#ifdef WWIV_WWIVBASIC_NEXT
#include "bbs/basic/next/next_engine.h"
#include "wwivbasic/interpreter.h"
#endif

using namespace wwiv::bbs;
using namespace wwiv::common;
using namespace wwiv::bbs::basic;
using namespace wwiv::strings;

class BasicTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    // Enable the FILE and OS package.
    helper.config().scripting_enabled(true);
    helper.config().script_package_file_enabled(true);
    helper.config().script_package_os_enabled(true);
    basic.reset(new Basic(bin, bout, *a()->config(), &helper.context()));
  }

  // The engines in this build. Each test runs its scripts on every one.
  static std::vector<script_engine_t> Engines() {
    std::vector<script_engine_t> engines{script_engine_t::legacy};
    if (script_engine_available(script_engine_t::next)) {
      engines.push_back(script_engine_t::next);
    }
    return engines;
  }

  // Makes RunScript use engine e, with no output captured yet.
  void Use(script_engine_t e) {
    engine_ = e;
    helper.io()->Clear();
  }

  // Runs text on the current engine (see Use), with an ASSERT module.
  [[nodiscard]] bool RunScript(const std::string& text) {
    const auto* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    const auto module = StrCat(test_info->test_suite_name(), "#", test_info->name());

#ifdef WWIV_WWIVBASIC_NEXT
    if (engine_ == script_engine_t::next) {
      NextEngine engine(bin, bout, *a()->config(), &helper.context(), RegisterNextAssertModule);
      return engine.RunScript(module, text);
    }
#endif
    RegisterBasicUnitTestModule(basic->bas());
    const auto r = basic->RunScript(module, text);
    basic.reset(new Basic(bin, bout, *a()->config(), &helper.context()));
    return r;
  }

#ifdef WWIV_WWIVBASIC_NEXT
  // ASSERT.EQUALS and ASSERT.NE for the new engine, like the legacy ones
  // below: values of different types are never equal, and REALs are equal
  // within 0.001. my_basic had no booleans (TRUE was the integer 1), so a
  // BOOLEAN compares as the INTEGER 0 or 1.
  static void RegisterNextAssertModule(wwivbasic::Interpreter& interp) {
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
#endif

  static bool IsValueEq(const mb_value_t& v1, const mb_value_t& v2, const std::string& msg) {
    if (v1.type != v2.type) {
      LOG(WARNING) << "Different datatypes in ASSERT.EQUALS";
      return false;
    }
    switch (v1.type) {
    case MB_DT_INT: {
      const auto eq = v1.value.integer == v2.value.integer;
      if (!eq) {
        std::cerr << "FAIL: ASSERT.EQUALS: '" << v1.value.integer << "' != '"
                  << v2.value.integer << "'" << msg << std::endl;
      }
      return eq;
    }
    case MB_DT_REAL: {
      const auto eq =
          std::abs(v1.value.float_point - v2.value.float_point) < 0.001f;
      if (!eq) {
        std::cerr << "FAIL: ASSERT.EQUALS: '" << v1.value.float_point << "' != '"
                  << v2.value.float_point << "'" << msg << std::endl;
      }
      return eq;
    }
    case MB_DT_STRING: {
      const auto eq = IsEquals(v1.value.string, v2.value.string);
      if (!eq) {
        std::cerr << "FAIL: ASSERT.EQUALS: '" << v1.value.string << "' != '"
                  << v2.value.string << "'" << msg << std::endl;
      }
      return eq;
    }
    case MB_DT_NIL:
    case MB_DT_UNKNOWN:
    case MB_DT_NUM:
    case MB_DT_TYPE:
    case MB_DT_USERTYPE:
    case MB_DT_USERTYPE_REF:
    case MB_DT_ARRAY:
    case MB_DT_LIST:
    case MB_DT_LIST_IT:
    case MB_DT_DICT:
    case MB_DT_DICT_IT:
    case MB_DT_COLLECTION:
    case MB_DT_ITERATOR:
    case MB_DT_CLASS:
    case MB_DT_ROUTINE:
      LOG(ERROR) << "Unknown datatype for LHS: " << v1.type;
      return false;
    }
    return false;
  }

  static bool IsValueNotEq(const mb_value_t& v1, const mb_value_t& v2, const std::string& msg) {
    return !IsValueEq(v1, v2, msg);
  }

  static void RegisterBasicUnitTestModule(mb_interpreter_t* basi) {
    mb_begin_module(basi, "ASSERT");

    mb_register_func(basi, "EQUALS", [](struct mb_interpreter_t* bas, void** l) -> int {
      mb_check(mb_attempt_open_bracket(bas, l));
      auto v1 = wwiv_mb_pop_value(bas, l);
      auto v2 = wwiv_mb_pop_value(bas, l);
      if (!v1 || !v2) {
        LOG(WARNING) << "missing params in ASSERT.EQUALS";
        return MB_FUNC_ERR;
      }
      std::string msg;
      if (mb_has_arg(bas, l)) {
        if (const auto rm = wwiv_mb_pop_string(bas, l)) {
          msg = rm.value();          
        }
      }
      mb_check(mb_attempt_close_bracket(bas, l));
      return IsValueEq(v1.value(), v2.value(), msg) ? MB_FUNC_OK : MB_FUNC_ERR;

    });

    mb_register_func(basi, "NE", [](struct mb_interpreter_t* bas, void** l) -> int {
      mb_check(mb_attempt_open_bracket(bas, l));
      auto v1 = wwiv_mb_pop_value(bas, l);
      auto v2 = wwiv_mb_pop_value(bas, l);
      if (!v1 || !v2) {
        LOG(WARNING) << "missing params in ASSERT.NE";
        return MB_FUNC_ERR;
      }
      std::string msg;
      if (mb_has_arg(bas, l)) {
        if (const auto rm = wwiv_mb_pop_string(bas, l)) {
          msg = rm.value();          
        }
      }
      mb_check(mb_attempt_close_bracket(bas, l));
      return IsValueNotEq(v1.value(), v2.value(), msg) ? MB_FUNC_OK : MB_FUNC_ERR;
    });

    mb_end_module(basi);
  }

  // After a script that should fail on an ASSERT: on the new engine, checks
  // that it was the ASSERT that failed (and why), not some other error. The
  // legacy ASSERT logs to stderr, so there's nothing to check there.
  void ExpectAssertFailure(const std::string& why) {
    const auto captured = helper.io()->captured();
    if (engine_ == script_engine_t::next) {
      EXPECT_THAT(captured, testing::HasSubstr("ASSERT.EQUALS failed: " + why));
    }
  }

  BbsHelper helper{};
  std::unique_ptr<Basic> basic;
  script_engine_t engine_{script_engine_t::legacy};
};

#define BASIC_ASSERT_EQUALS(v1, v2)                                                                \
  do {                                                                                             \
    const auto __s = fmt::format(R"( ASSERT.EQUALS({}, {}) )", v1, v2);                            \
    ASSERT_TRUE(RunScript(__s));                                                                   \
  } while (0)

#define BASIC_ASSERT_NE(v1, v2)                                                                \
  do {                                                                                             \
    const auto __s = fmt::format(R"( ASSERT.NE({}, {}) )", v1, v2);                            \
    ASSERT_TRUE(RunScript(__s));                                                                   \
  } while (0)

TEST_F(BasicTest, Smoke) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    ASSERT_TRUE(RunScript("print \"Hello World\""));
    EXPECT_EQ("Hello World", StringTrim(helper.io()->captured()));
  }
}

TEST_F(BasicTest, Smoke_Fail) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    // There is no outstr( function.
    ASSERT_FALSE(RunScript("outstr( \"Hello World\""));

    // Script errors are echoed to the caller so they can see why it failed.
    const auto captured = StringTrim(helper.io()->captured());
    if (e == script_engine_t::legacy) {
      EXPECT_THAT(captured, testing::StartsWith("Error:"));
      EXPECT_THAT(captured, testing::HasSubstr("Line 1"));
    } else {
      // The new engine's errors are "name:line[:column]: error: message".
      // It's the parse error, not some other failure.
      EXPECT_THAT(captured, testing::StartsWith("BasicTest#Smoke_Fail:1:22: error: missing ')'"));
    }
  }
}

TEST_F(BasicTest, Assert_Int_False) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    BASIC_ASSERT_NE(1, 2);
    ASSERT_FALSE(RunScript("ASSERT.EQUALS(1, 2)"));
    ExpectAssertFailure("'1' != '2'");
  }
}

TEST_F(BasicTest, Assert_Int_True) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    BASIC_ASSERT_EQUALS(1, 1);
    ASSERT_TRUE(RunScript("ASSERT.EQUALS(1, 1)"));
  }
}

TEST_F(BasicTest, Assert_String_False) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    BASIC_ASSERT_NE("\"hello\"", "\"world\"");
  }
}

TEST_F(BasicTest, Assert_String_True) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    BASIC_ASSERT_EQUALS("\"hello\"", "\"hello\"");
  }
}

TEST_F(BasicTest, Assert_Real_False) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    ASSERT_FALSE(RunScript("ASSERT.EQUALS(1.1, 1.2)"));
    ExpectAssertFailure("'1.1' != '1.2'");
  }
}

TEST_F(BasicTest, Assert_Real_True) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    ASSERT_TRUE(RunScript("ASSERT.EQUALS(1.1, 1.1)"));
  }
}

TEST_F(BasicTest, Assert_DifferentTypes) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    ASSERT_FALSE(RunScript(R"( ASSERT.EQUALS("1.1", 1.1) )"));
    ExpectAssertFailure("different types: a STRING and a REAL");
  }
}

TEST_F(BasicTest, ModuleNames) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS("wwiv", wwiv.module_name()) )"));
    EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS("wwiv.io", wwiv.io.module_name()) )"));
    EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS("wwiv.io.file", wwiv.io.file.module_name()) )"));
    EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS("wwiv.data", wwiv.data.module_name()) )"));
    EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS("wwiv.os", wwiv.os.module_name()) )"));
  }
}

TEST_F(BasicTest, WWIV_Version) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    const auto script = fmt::format(R"(ASSERT.EQUALS("{}", wwiv.version()) )",
                                    wwiv::core::short_version());
    EXPECT_TRUE(RunScript(script));
  }
}

TEST_F(BasicTest, WWIV_Version_Import) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    const auto script = fmt::format(R"(
import "@wwiv"
ASSERT.EQUALS("{}", version())
)", wwiv::core::short_version());
    EXPECT_TRUE(RunScript(script));
  }
}

TEST_F(BasicTest, WWIV_IO_PrintFile) {
  helper.files().CreateTempFile("gfiles/file1.msg", "this is file1");
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    ASSERT_TRUE(RunScript("wwiv.io.printfile(\"file1.msg\")"));
    EXPECT_EQ("this is file1\r\n", helper.io()->captured());
  }
}

TEST_F(BasicTest, WWIV_IO_PrintFile_DoesNotExist) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    ASSERT_TRUE(RunScript("wwiv.io.printfile(\"file1.msg\")"));
    EXPECT_EQ("", helper.io()->captured());
  }
}

TEST_F(BasicTest, WWIV_IO_Outstr) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    ASSERT_TRUE(RunScript("wwiv.io.outstr(\"Hello World\")"));
    EXPECT_EQ("Hello World", helper.io()->captured());
  }
}

TEST_F(BasicTest, WWIV_IO_Puts) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    ASSERT_TRUE(RunScript("wwiv.io.puts(\"Hello World\")"));
    EXPECT_EQ("Hello World", helper.io()->captured());
  }
}

TEST_F(BasicTest, WWIV_IO_PL) {
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    ASSERT_TRUE(RunScript("wwiv.io.pl(\"Hello World\")"));
    EXPECT_EQ("Hello World\r\n", helper.io()->captured());
  }
}

TEST_F(BasicTest, WWIV_Eval_Pass) {
  helper.user()->sl(200);
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS(wwiv.eval("user.sl > 100"), TRUE) )"));
    // ASSERT compares a BOOLEAN as an INTEGER, so also check what PRINT shows:
    // EVAL returns the integer 1, as in my_basic, not TRUE.
    helper.io()->Clear();
    EXPECT_TRUE(RunScript(R"( print wwiv.eval("user.sl > 100") )"));
    EXPECT_EQ("1\r\n", helper.io()->captured());
  }
}

TEST_F(BasicTest, WWIV_Eval_Fail) {
  helper.user()->sl(200);
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS(wwiv.eval("user.sl < 200"), FALSE) )"));
    helper.io()->Clear();
    EXPECT_TRUE(RunScript(R"( print wwiv.eval("user.sl < 200") )"));
    EXPECT_EQ("0\r\n", helper.io()->captured());
  }
}

TEST_F(BasicTest, WWIV_IO_FILE_ReadIntoString) {
  const std::string actual = "this is file1";
  helper.files().CreateTempFile("gfiles/file1.msg", actual);
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    ASSERT_TRUE(RunScript(R"( 
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "file1.msg", "R")
s = wwiv.io.file.readintostring(h)
print s
)" ));
    EXPECT_EQ(StrCat(actual, "\r\n"), helper.io()->captured());
  }
}

TEST_F(BasicTest, WWIV_IO_FILE_ReadLines) {
  const std::string expected = "1\r\n2\r\n3\r\n";
  helper.files().CreateTempFile("gfiles/file1.msg", expected);
  for (const auto e : Engines()) {
    SCOPED_TRACE(to_string(e));
    Use(e);
    ASSERT_TRUE(RunScript(R"(

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
    ASSERT_EQ(3, wwiv::stl::ssize(ex));

    const auto actual = SplitString(helper.io()->captured(), "\r\n", true);
    EXPECT_THAT(actual, testing::ElementsAre("1", "2", "3"));
  }
}
