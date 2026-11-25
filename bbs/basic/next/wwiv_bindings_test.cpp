/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2026, WWIV Software Services                  */
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
/**************************************************************************/
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "bbs/basic/next/bbs_console.h"
#include "bbs/basic/next/next_engine.h"
#include "bbs/bbs.h"
#include "bbs/bbs_helper.h"
#include "common/common_events.h"
#include "common/input.h"
#include "common/output.h"
#include "core/datetime.h"
#include "core/eventbus.h"
#include "core/strings.h"
#include "core/version.h"
#include "sdk/config.h"

#include <cstring>
#include <string>

using namespace wwiv::bbs::basic;
using namespace wwiv::strings;
using testing::HasSubstr;
using testing::Not;

namespace {

// The session to hang up the next time Input::getkey checks for a hangup, or
// nullptr. getkey checks at the start of every call, before it reads a key
// that is already typed ahead, so this hangs up in the middle of a read.
wwiv::common::SessionContext* hang_up_on_check = nullptr;

void HangUpDuringNextRead(wwiv::common::SessionContext& sess) {
  [[maybe_unused]] static const auto registered = [] {
    wwiv::core::bus().add_handler<wwiv::common::CheckForHangupEvent>([] {
      if (hang_up_on_check) {
        hang_up_on_check->hangup(true);
      }
    });
    return true;
  }();
  hang_up_on_check = &sess;
}

} // namespace

// The wwiv, wwiv.io and wwiv.time modules on the new engine. These mirror the
// legacy engine's tests in bbs/basic/basic_test.cpp. Input is typed ahead into
// bin's character buffer. PAUSE isn't covered.
class WwivBindingsTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    helper.config().scripting_enabled(true);
  }

  void TearDown() override {
    hang_up_on_check = nullptr;
    bin.charbufferpointer_ = 0;
    bin.charbuffer[0] = '\0';
    bin.charbuffer[1] = '\0';
  }

  // Types keys ahead, as if the caller had typed them.
  static void TypeAhead(const std::string& keys) {
    ASSERT_LT(keys.size(), sizeof(bin.charbuffer) - 1);
    std::strcpy(&bin.charbuffer[1], keys.c_str());
    bin.charbufferpointer_ = 1;
  }

  wwiv::common::SessionContext& sess() { return helper.context().session_context(); }

  [[nodiscard]] bool Run(const std::string& text) {
    NextEngine engine(bin, bout, helper.config(), &helper.context());
    return engine.RunScript("test", text);
  }

  BbsHelper helper{};
};

TEST_F(WwivBindingsTest, ModuleNames) {
  ASSERT_TRUE(Run(R"(
print wwiv.io.module_name()
print wwiv.time.module_name()
)"));
  EXPECT_EQ("wwiv.io\r\nwwiv.time\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_ModuleName_AlsoPrintsIt) {
  ASSERT_TRUE(Run("x = wwiv.module_name()\nprint \"[\"; x; \"]\""));
  EXPECT_EQ("wwiv\r\n[wwiv]\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_Version) {
  ASSERT_TRUE(Run("print wwiv.version()"));
  EXPECT_EQ(wwiv::core::short_version() + "\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_Version_Import) {
  ASSERT_TRUE(Run("import \"@wwiv\"\nprint version()"));
  EXPECT_EQ(wwiv::core::short_version() + "\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_Eval_Pass) {
  helper.user()->sl(200);
  ASSERT_TRUE(Run(R"(print wwiv.eval("user.sl > 100"))"));
  EXPECT_EQ("1\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_Eval_Fail) {
  helper.user()->sl(200);
  ASSERT_TRUE(Run(R"(print wwiv.eval("user.sl < 200"))"));
  EXPECT_EQ("0\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_Eval_NoArgument) {
  ASSERT_TRUE(Run("print wwiv.eval()"));
  EXPECT_EQ("0\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_Interpret) {
  helper.user()->sl(42);
  ASSERT_TRUE(Run(R"(print wwiv.interpret("S"))"));
  EXPECT_EQ("42\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_Interpret_UsesFirstCharacter) {
  helper.user()->sl(42);
  ASSERT_TRUE(Run(R"(print wwiv.interpret("Sxyz"))"));
  EXPECT_EQ("42\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_Command_NeedsAName) {
  EXPECT_FALSE(Run("wwiv.command()"));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("COMMAND"));
}

TEST_F(WwivBindingsTest, WWIV_IO_PrintFile) {
  helper.files().CreateTempFile("gfiles/file1.msg", "this is file1");
  ASSERT_TRUE(Run("wwiv.io.printfile(\"file1.msg\")"));
  EXPECT_EQ("this is file1\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_IO_PrintFile_DoesNotExist) {
  ASSERT_TRUE(Run("wwiv.io.printfile(\"file1.msg\")"));
  EXPECT_EQ("", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_IO_Outstr) {
  ASSERT_TRUE(Run("wwiv.io.outstr(\"Hello World\")"));
  EXPECT_EQ("Hello World", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_IO_Outstr_SeveralArguments) {
  ASSERT_TRUE(Run("wwiv.io.outstr(\"a\", \"b\", \"c\")"));
  EXPECT_EQ("abc", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_IO_Outstr_Colors) {
  ASSERT_TRUE(Run("wwiv.io.outstr(\"|#3Hello\")"));
  EXPECT_EQ("Hello", helper.io()->captured());
}

// |@N is the caller's name, an MCI macro.
TEST_F(WwivBindingsTest, WWIV_IO_Outstr_RunsMci) {
  helper.user()->set_name("Rushfan");
  ASSERT_TRUE(Run("wwiv.io.outstr(\"a|@Nb\")"));
  EXPECT_EQ("aRushfanb", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_IO_OutstrNoMci) {
  helper.user()->set_name("Rushfan");
  // MCI is off only for OUTSTR_NOMCI; colors still work.
  ASSERT_TRUE(Run("wwiv.io.outstr_nomci(\"|#3a|@Nb\", \"|{user.name}\")\n"
                  "wwiv.io.outstr(\"|@N\")"));
  EXPECT_EQ("ab|{user.name}Rushfan", helper.io()->captured());
  EXPECT_TRUE(bout.mci_enabled());
}

TEST_F(WwivBindingsTest, WWIV_IO_PLNoMci) {
  helper.user()->set_name("Rushfan");
  ASSERT_TRUE(Run("wwiv.io.pl_nomci(\"a|@N\", \"b\")\nwwiv.io.pl(\"|@N\")"));
  EXPECT_EQ("a\r\nb\r\nRushfan\r\n", helper.io()->captured());
  EXPECT_TRUE(bout.mci_enabled());
}

TEST_F(WwivBindingsTest, WWIV_IO_Puts) {
  ASSERT_TRUE(Run("wwiv.io.puts(\"Hello World\")"));
  EXPECT_EQ("Hello World", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_IO_PL) {
  ASSERT_TRUE(Run("wwiv.io.pl(\"Hello World\")"));
  EXPECT_EQ("Hello World\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_IO_PL_SeveralArguments) {
  ASSERT_TRUE(Run("wwiv.io.pl(\"a\", \"b\")"));
  EXPECT_EQ("a\r\nb\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_IO_NL) {
  ASSERT_TRUE(Run("wwiv.io.outstr(\"a\")\nwwiv.io.nl()\nwwiv.io.outstr(\"b\")\nwwiv.io.nl(2)"));
  EXPECT_EQ("a\r\nb\r\n\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_IO_Gets_NoLength_ReadsNothing) {
  ASSERT_TRUE(Run("x = wwiv.io.gets()\nprint \"[\"; x; \"]\""));
  EXPECT_EQ("[]\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_IO_Import_Unqualified) {
  ASSERT_TRUE(Run("import \"@wwiv.io\"\noutstr(\"a\")\npl(\"b\")"));
  EXPECT_EQ("ab\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_IO_GetKey) {
  TypeAhead("x");
  ASSERT_TRUE(Run("print \"[\" + wwiv.io.getkey() + \"]\""));
  EXPECT_EQ("[x]\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_IO_Gets) {
  helper.user()->set_flag(wwiv::sdk::User::flag_ansi, false);
  TypeAhead("hello\r");
  ASSERT_TRUE(Run("x = wwiv.io.gets(10)\nprint \"[\" + x + \"]\""));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("[hello]"));
}

TEST_F(WwivBindingsTest, WWIV_IO_Gets_StopsAtTheLength) {
  helper.user()->set_flag(wwiv::sdk::User::flag_ansi, false);
  TypeAhead("hello world\r");
  ASSERT_TRUE(Run("x = wwiv.io.gets(5)\nprint \"[\" + x + \"]\""));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("[hello]"));
}

TEST_F(WwivBindingsTest, WWIV_IO_Gets_LongerThanALine) {
  // Like the legacy GETS, the length goes to input_text as is; only an ANSI
  // caller's line editor stops at 78 characters.
  helper.user()->set_flag(wwiv::sdk::User::flag_ansi, false);
  const std::string line(100, 'a');
  TypeAhead(line + "\r");
  ASSERT_TRUE(Run("x = wwiv.io.gets(160)\nprint \"[\" + x + \"]\""));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("[" + line + "]"));
}

TEST_F(WwivBindingsTest, WWIV_IO_YN) {
  TypeAhead("Y");
  ASSERT_TRUE(Run("print \"[\"; wwiv.io.yn(\"Sure? \"); \"]\""));
  const auto captured = helper.io()->captured();
  EXPECT_THAT(captured, HasSubstr("Sure? "));
  EXPECT_THAT(captured, HasSubstr("[1]"));
}

TEST_F(WwivBindingsTest, WWIV_IO_YN_DefaultsToNo) {
  TypeAhead("\r");
  ASSERT_TRUE(Run("print \"[\"; wwiv.io.yn(); \"]\""));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("[0]"));
}

TEST_F(WwivBindingsTest, WWIV_IO_NY) {
  TypeAhead("N");
  ASSERT_TRUE(Run("print \"[\"; wwiv.io.ny(\"Sure? \"); \"]\""));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("[0]"));
}

TEST_F(WwivBindingsTest, WWIV_IO_NY_DefaultsToYes) {
  TypeAhead("\r");
  ASSERT_TRUE(Run("print \"[\"; wwiv.io.ny(); \"]\""));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("[1]"));
}

TEST_F(WwivBindingsTest, WWIV_IO_HangUpDuringGetKey_ReturnsEmpty) {
  // getkey returns the typed-ahead key even though the caller hung up during
  // the call; GETKEY returns "" instead, and the script stops.
  TypeAhead("x");
  HangUpDuringNextRead(sess());
  EXPECT_FALSE(Run("k = wwiv.io.getkey()\nprint \"not reached\""));
  EXPECT_THAT(helper.io()->captured(), Not(HasSubstr("not reached")));

  BbsConsole console(bin, bout, [this] { return sess().hangup(); });
  sess().hangup(false);
  TypeAhead("x");
  HangUpDuringNextRead(sess());
  EXPECT_EQ("", console.read_key());
}

TEST_F(WwivBindingsTest, WWIV_IO_HangUpDuringGets_ReturnsEmpty) {
  helper.user()->set_flag(wwiv::sdk::User::flag_ansi, false);
  TypeAhead("abc\r");
  HangUpDuringNextRead(sess());
  EXPECT_FALSE(Run("x = wwiv.io.gets(10)\nprint \"not reached\""));
  EXPECT_THAT(helper.io()->captured(), Not(HasSubstr("not reached")));

  BbsConsole console(bin, bout, [this] { return sess().hangup(); });
  sess().hangup(false);
  TypeAhead("abc\r");
  HangUpDuringNextRead(sess());
  EXPECT_EQ("", console.read_line(10));
}

TEST_F(WwivBindingsTest, WWIV_IO_TypeErrorNamesTheFunction) {
  EXPECT_FALSE(Run("l = list(1)\nwwiv.io.outstr(l)"));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("OUTSTR: expected a STRING, got a LIST"));
}

TEST_F(WwivBindingsTest, WWIV_Time_Format) {
  const auto t = wwiv::core::DateTime::now().to_time_t();
  const auto expected = wwiv::core::DateTime::from_time_t(t).to_string("%Y-%m-%d");
  ASSERT_TRUE(Run(fmt::format(R"(print wwiv.time.format({}, "%Y-%m-%d"))", t)));
  EXPECT_EQ(expected + "\r\n", helper.io()->captured());
}

TEST_F(WwivBindingsTest, WWIV_Time_Format_NeedsAFormat) {
  EXPECT_FALSE(Run("print wwiv.time.format(0)"));
}
