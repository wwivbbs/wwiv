/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2021, WWIV Software Services               */
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
#include "bbs/basic/util.h"
#include "bbs/bbs_helper.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/version.h"
#include "deps/my_basic/core/my_basic.h"
#include "fmt/format.h"
#include <iostream>
#include <string>

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

  [[nodiscard]] bool RunScript(const std::string& text) {
    const auto* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    const auto module = StrCat(test_info->test_suite_name(), "#", test_info->name());

    RegisterBasicUnitTestModule(basic->bas());
    const auto r = basic->RunScript(module, text);
    basic.reset(new Basic(bin, bout, *a()->config(), &helper.context()));
    return r;
  }

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

  BbsHelper helper{};
  std::unique_ptr<Basic> basic;
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
  ASSERT_TRUE(RunScript("print \"Hello World\""));

  EXPECT_EQ("Hello World", StringTrim(helper.io()->captured()));
}

TEST_F(BasicTest, Smoke_Fail) {
  // There is no puts function.
  ASSERT_FALSE(RunScript("puts \"Hello World\""));

  EXPECT_EQ("", StringTrim(helper.io()->captured()));
}

TEST_F(BasicTest, Assert_Int_False) {
  BASIC_ASSERT_NE(1, 2);
  ASSERT_FALSE(RunScript("ASSERT.EQUALS(1, 2)"));
}

TEST_F(BasicTest, Assert_Int_True) {
  BASIC_ASSERT_EQUALS(1, 1);
  ASSERT_TRUE(RunScript("ASSERT.EQUALS(1, 1)"));
}

TEST_F(BasicTest, Assert_String_False) {
  BASIC_ASSERT_NE("\"hello\"", "\"world\"");
}

TEST_F(BasicTest, Assert_String_True) {
  BASIC_ASSERT_EQUALS("\"hello\"", "\"hello\"");
}

TEST_F(BasicTest, Assert_Real_False) {
  ASSERT_FALSE(RunScript("ASSERT.EQUALS(1.1, 1.2)"));
}

TEST_F(BasicTest, Assert_Real_True) {
  ASSERT_TRUE(RunScript("ASSERT.EQUALS(1.1, 1.1)"));
}

TEST_F(BasicTest, Assert_DifferentTypes) {
  ASSERT_FALSE(RunScript(R"( ASSERT.EQUALS("1.1", 1.1) )"));
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
  ASSERT_TRUE(RunScript("wwiv.io.printfile(\"file1.msg\")"));

  EXPECT_EQ("this is file1\r\n", helper.io()->captured());
}
  
TEST_F(BasicTest, WWIV_IO_PrintFile_DoesNotExist) {
  ASSERT_TRUE(RunScript("wwiv.io.printfile(\"file1.msg\")"));
  EXPECT_EQ("", helper.io()->captured());
}

TEST_F(BasicTest, WWIV_IO_Puts) {
  ASSERT_TRUE(RunScript("wwiv.io.puts(\"Hello World\")"));

  EXPECT_EQ("Hello World", helper.io()->captured());
}

TEST_F(BasicTest, WWIV_IO_PL) {
  ASSERT_TRUE(RunScript("wwiv.io.pl(\"Hello World\")"));

  EXPECT_EQ("Hello World\r\n", helper.io()->captured());
}

TEST_F(BasicTest, WWIV_Eval_Pass) {
  helper.user()->sl(200);
  EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS(wwiv.eval("user.sl > 100"), TRUE) )"));
}

TEST_F(BasicTest, WWIV_Eval_Fail) {
  helper.user()->sl(200);
  EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS(wwiv.eval("user.sl < 200"), FALSE) )"));
}

TEST_F(BasicTest, WWIV_IO_FILE_ReadIntoString) {
  const std::string actual = "this is file1";
  helper.files().CreateTempFile("gfiles/file1.msg", actual);
  ASSERT_TRUE(RunScript(R"( 
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
  const auto e = SplitString(expected, "\r\n", true);
  ASSERT_EQ(3, wwiv::stl::ssize(e));

  const auto actual = SplitString(helper.io()->captured(), "\r\n", true);
  EXPECT_THAT(actual, testing::ElementsAre("1", "2", "3"));
}
