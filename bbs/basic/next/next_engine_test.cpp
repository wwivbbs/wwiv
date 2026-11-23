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
#include "common/input.h"
#include "common/output.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/config.h"

using namespace wwiv::bbs::basic;
using namespace wwiv::core;
using namespace wwiv::strings;
using testing::HasSubstr;
using testing::Not;

// Tests for the new engine (wwivbasic) running scripts with a BbsConsole.
// Input isn't covered: the test helpers have no way to feed keys to bin.
class NextEngineTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    helper.config().scripting_enabled(true);
    std::filesystem::create_directories(helper.config().scriptdir());
  }

  void CreateScript(const std::string& name, const std::string& text) {
    TextFile f(FilePath(helper.config().scriptdir(), name), "wt");
    ASSERT_TRUE(f.IsOpen());
    f.Write(text);
  }

  bool Run(const std::string& text) {
    NextEngine engine(bin, bout, helper.config(), &helper.context());
    return engine.RunScript("test", text);
  }

  BbsHelper helper{};
};

TEST_F(NextEngineTest, Console_Write) {
  BbsConsole console(bin, bout);
  console.write("Hello");
  console.write(" World");
  EXPECT_EQ("Hello World", helper.io()->captured());
}

TEST_F(NextEngineTest, Console_Newline) {
  BbsConsole console(bin, bout);
  console.write("a");
  console.newline();
  console.write("b");
  EXPECT_EQ("a\r\nb", helper.io()->captured());
}

TEST_F(NextEngineTest, Console_PipeColorsAreInterpreted) {
  BbsConsole console(bin, bout);
  console.write("|#1Hello");
  EXPECT_EQ("Hello", helper.io()->captured());
}

TEST_F(NextEngineTest, Print) {
  ASSERT_TRUE(Run("print \"Hello World\""));
  EXPECT_EQ("Hello World\r\n", helper.io()->captured());
}

TEST_F(NextEngineTest, Print_Separators) {
  ASSERT_TRUE(Run("a = 2\nprint \"a=\"; a\nprint \"b\";\nprint \"c\""));
  EXPECT_EQ("a=2\r\nbc\r\n", helper.io()->captured());
}

TEST_F(NextEngineTest, Print_Colors) {
  ASSERT_TRUE(Run("print \"|#3Hello\""));
  EXPECT_EQ("Hello\r\n", helper.io()->captured());
}

TEST_F(NextEngineTest, Error_ShowsScriptRelativeName) {
  ASSERT_FALSE(Run("print \"one\"\nnosuchfunction(1)"));
  const auto captured = helper.io()->captured();
  EXPECT_THAT(captured, HasSubstr("test:2: error: "));
  EXPECT_THAT(captured, HasSubstr("nosuchfunction"));
  // The BBS's directories aren't shown to the caller.
  EXPECT_THAT(captured, Not(HasSubstr(helper.config().scriptdir().string())));
}

TEST_F(NextEngineTest, ParseError) {
  ASSERT_FALSE(Run("print (\"unclosed\""));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("test:1:"));
}

TEST_F(NextEngineTest, ScriptFile_Error_ShowsFileName) {
  CreateScript("bad.bas", "print 1\nnosuchfunction()\n");
  NextEngine engine(bin, bout, helper.config(), &helper.context());
  ASSERT_FALSE(engine.RunScript("bad.bas"));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("bad.bas:2: error: "));
}

TEST_F(NextEngineTest, ScriptFile_InvalidName) {
  CreateScript("x.bas", "print 1\n");
  NextEngine engine(bin, bout, helper.config(), &helper.context());
  // A name that leads back into scriptdir, so the file exists.
  const auto dir = FilePath(helper.config().scriptdir(), "x.bas").parent_path().filename();
  ASSERT_FALSE(engine.RunScript(StrCat("../", dir.string(), "/x.bas")));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("Invalid script name"));
}

TEST_F(NextEngineTest, ScriptFile_InvalidName_DoesNotRevealMissingFiles) {
  // ".." is rejected before looking for the file, so a file that doesn't
  // exist gets the same message as one that does.
  NextEngine engine(bin, bout, helper.config(), &helper.context());
  ASSERT_FALSE(engine.RunScript("../nothere.bas"));
  const auto captured = helper.io()->captured();
  EXPECT_THAT(captured, HasSubstr("Invalid script name"));
  EXPECT_THAT(captured, Not(HasSubstr("Unable to locate")));
}

TEST_F(NextEngineTest, Import_OutsideScriptDir) {
  ASSERT_FALSE(Run("import \"../secret.bas\"\nprint 1"));
  const auto captured = helper.io()->captured();
  EXPECT_THAT(captured, HasSubstr("Can't import '../secret.bas'"));
  EXPECT_THAT(captured, Not(HasSubstr("1\r\n")));
}

TEST_F(NextEngineTest, Import_ErrorInImportedFile_EveryLineRelative) {
  CreateScript("broken.bas", "print (\"unclosed\"\n");
  ASSERT_FALSE(Run("import \"broken.bas\""));
  const auto captured = helper.io()->captured();
  EXPECT_THAT(captured, HasSubstr("Unable to import 'broken.bas'"));
  EXPECT_THAT(captured, HasSubstr("broken.bas:1:"));
  EXPECT_THAT(captured, Not(HasSubstr(helper.config().scriptdir().string())));
  // Every line ends with CR LF, so it doesn't stair-step on a terminal.
  for (size_t i = captured.find('\n'); i != std::string::npos; i = captured.find('\n', i + 1)) {
    ASSERT_GT(i, 0u);
    EXPECT_EQ('\r', captured[i - 1]) << captured;
  }
}

TEST_F(NextEngineTest, Error_PipeCodesShownAsText) {
  // The file doesn't exist, so the error repeats the name, pipe code and all.
  ASSERT_FALSE(Run("import \"|#3x.bas\""));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("'|#3x.bas'"));
}

TEST_F(NextEngineTest, HungUp_ScriptDoesNotRun) {
  helper.context().session_context().hangup(true);
  ASSERT_FALSE(Run("print \"Hello\""));
  EXPECT_EQ("", helper.io()->captured());
}

TEST_F(NextEngineTest, HungUp_LoopStops) {
  helper.context().session_context().hangup(true);
  ASSERT_FALSE(Run("while 1\nwend"));
  EXPECT_EQ("", helper.io()->captured());
}

TEST_F(NextEngineTest, Console_HungUp_InputIsAtItsEnd) {
  BbsConsole console(bin, bout, [] { return true; });
  EXPECT_EQ("", console.read_key());
  EXPECT_EQ("", console.read_line(10));
  EXPECT_EQ("", console.read_line(0));
}

TEST_F(NextEngineTest, Import_FromScriptDir) {
  CreateScript("lib.bas", "def greet(n)\n  return \"Hi \" + n\nenddef\n");
  ASSERT_TRUE(Run("import \"lib.bas\"\nprint greet(\"Bob\")"));
  EXPECT_EQ("Hi Bob\r\n", helper.io()->captured());
}

TEST_F(NextEngineTest, EachRunIsFresh) {
  NextEngine engine(bin, bout, helper.config(), &helper.context());
  ASSERT_TRUE(engine.RunScript("one", "x = 1\nprint x"));
  ASSERT_TRUE(engine.RunScript("two", "print x"));
  EXPECT_EQ("1\r\n\r\n", helper.io()->captured());
}
