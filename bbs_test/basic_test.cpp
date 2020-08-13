/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2020, WWIV Software Services               */
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
#include "gtest/gtest.h"

#include "bbs/basic/basic.h"
#include "bbs/interpret.h"
#include "bbs_test/bbs_helper.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/version.h"
#include "deps/my_basic/core/my_basic.h"
#include "fmt/format.h"
#include "sdk/files/dirs.h"
#include <iostream>
#include <string>

using std::cout;
using std::endl;
using std::string;

using namespace wwiv::bbs;
using namespace wwiv::bbs::basic;
using namespace wwiv::strings;

class TestMacroContext final : public MacroContext {
public:
  explicit TestMacroContext(BbsHelper& helper) : helper_(helper) {}
  [[nodiscard]] const wwiv::sdk::User& u() const override { return *helper_.user(); }
  [[nodiscard]] const wwiv::sdk::files::directory_t& dir() const override { return dir_; }
  [[nodiscard]] bool mci_enabled() const override { return true; };

  BbsHelper& helper_;
  wwiv::sdk::files::directory_t dir_{};
};

class BasicTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    ctx.reset(new TestMacroContext(helper));
    basic.reset(new Basic(bout, *a()->config(), ctx.get()));
  }

  [[nodiscard]] bool RunScript(const std::string& text) {
    const auto* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    const auto module = StrCat(test_info->test_suite_name(), "#", test_info->name());

    RegisterBasicUnitTestModule(basic->bas());
    const auto r = basic->RunScript(module, text);
    basic.reset(new Basic(bout, *a()->config(), ctx.get()));
    return r;
  }

  static void RegisterBasicUnitTestModule(mb_interpreter_t* bas) {
    mb_begin_module(bas, "ASSERT");

    mb_register_func(bas, "EQUALS", [](struct mb_interpreter_t* bas, void** l) -> int {
      mb_value_t v1{};
      mb_value_t v2{};
      char* raw_msg = nullptr;
      mb_check(mb_attempt_open_bracket(bas, l));
      mb_check(mb_pop_value(bas, l, &v1));
      mb_check(mb_pop_value(bas, l, &v2));
      if (mb_has_arg(bas, l)) {
        mb_check(mb_pop_string(bas, l, &raw_msg));
      }
      mb_check(mb_attempt_close_bracket(bas, l));
      if (v1.type != v2.type) {
        LOG(WARNING) << "Different datatypes in ASSERT.EQUALS";
        return MB_FUNC_ERR;
      }
      const std::string msg = raw_msg != nullptr ? raw_msg : "";
      switch (v1.type) {
      case MB_DT_INT: {
        const auto eq = v1.value.integer == v2.value.integer;
        if (!eq) {
          std::cerr << "FAIL: ASSERT.EQUALS: '" << v1.value.integer << "' != '" << v2.value.integer
                    << "'" << msg << std::endl;
        }
        return  eq ? MB_FUNC_OK : MB_FUNC_ERR;
      }
      case MB_DT_REAL: {
        const auto eq = v1.value.float_point == v2.value.float_point;
        if (!eq) {
          std::cerr << "FAIL: ASSERT.EQUALS: '" << v1.value.float_point << "' != '" << v2.value.float_point
                    << "'" << msg << std::endl;
        }
        return eq ? MB_FUNC_OK : MB_FUNC_ERR;
      }
      case MB_DT_STRING: {
        const auto eq = IsEquals(v1.value.string, v2.value.string);
        if (!eq) {
          std::cerr << "FAIL: ASSERT.EQUALS: '" << v1.value.string << "' != '" << v2.value.string
                    << "'" << msg << std::endl;
        }
        return eq ? MB_FUNC_OK : MB_FUNC_ERR;
      }
      default:
        LOG(ERROR) << "Unknown datatype for LHS: " << v1.type;
        return MB_FUNC_ERR;
      }
    });

    mb_end_module(bas);
  }

  BbsHelper helper{};
  std::unique_ptr<TestMacroContext> ctx;
  std::unique_ptr<Basic> basic;
};

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
  ASSERT_FALSE(RunScript("ASSERT.EQUALS(1, 2)"));
}

TEST_F(BasicTest, Assert_Int_True) {
  ASSERT_TRUE(RunScript("ASSERT.EQUALS(1, 1)"));
}

TEST_F(BasicTest, Assert_String_False) {
  ASSERT_FALSE(RunScript(R"( ASSERT.EQUALS("hello", "world") )"));
}

TEST_F(BasicTest, Assert_String_True) {
  ASSERT_TRUE(RunScript(R"( ASSERT.EQUALS("hello", "hello") )"));
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
  EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS("wwiv.data", wwiv.data.module_name()) )"));
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
  helper.user()->SetSl(200);
  EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS(wwiv.eval("user.sl > 100"), TRUE) )"));
}

TEST_F(BasicTest, WWIV_Eval_Fail) {
  helper.user()->SetSl(200);
  EXPECT_TRUE(RunScript(R"( ASSERT.EQUALS(wwiv.eval("user.sl < 200"), FALSE) )"));
}
