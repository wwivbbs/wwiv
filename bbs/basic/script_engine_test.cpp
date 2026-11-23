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

#include "bbs/basic/script_engine.h"
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

class ScriptEngineTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    helper.config().scripting_enabled(true);
  }

  BbsHelper helper{};
};

TEST_F(ScriptEngineTest, ToScriptEngine) {
  EXPECT_EQ(script_engine_t::legacy, to_script_engine_t("legacy"));
  EXPECT_EQ(script_engine_t::next, to_script_engine_t("next"));
  EXPECT_EQ(script_engine_t::next, to_script_engine_t("NeXt"));
  EXPECT_FALSE(to_script_engine_t(""));
  EXPECT_FALSE(to_script_engine_t("my_basic"));
}

TEST_F(ScriptEngineTest, ToString_RoundTrips) {
  for (const auto e : {script_engine_t::legacy, script_engine_t::next}) {
    EXPECT_EQ(e, to_script_engine_t(to_string(e)));
  }
}

TEST_F(ScriptEngineTest, ConfigFlag) {
  auto& config = helper.config();
  EXPECT_FALSE(config.script_engine_next());
  config.script_engine_next(true);
  EXPECT_TRUE(config.script_engine_next());
  // The other script flags are left alone.
  EXPECT_TRUE(config.scripting_enabled());
  EXPECT_FALSE(config.script_package_file_enabled());
  EXPECT_FALSE(config.script_package_os_enabled());
  config.script_engine_next(false);
  EXPECT_FALSE(config.script_engine_next());
}

TEST_F(ScriptEngineTest, Select_DefaultsToLegacy) {
  EXPECT_EQ(script_engine_t::legacy, SelectScriptEngine("", helper.config()));
}

TEST_F(ScriptEngineTest, Select_FromConfig) {
  helper.config().script_engine_next(true);
  const auto expected = script_engine_available(script_engine_t::next) ? script_engine_t::next
                                                                       : script_engine_t::legacy;
  EXPECT_EQ(expected, SelectScriptEngine("", helper.config()));
}

TEST_F(ScriptEngineTest, Select_OverrideWins) {
  helper.config().script_engine_next(true);
  EXPECT_EQ(script_engine_t::legacy, SelectScriptEngine("legacy", helper.config()));
  helper.config().script_engine_next(false);
  EXPECT_EQ(script_engine_t::next, SelectScriptEngine("next", helper.config()));
}

TEST_F(ScriptEngineTest, Legacy_Runs) {
  auto engine = CreateScriptEngine(script_engine_t::legacy, bin, bout, helper.config(),
                                   &helper.context());
  ASSERT_NE(nullptr, engine);
  ASSERT_TRUE(engine->RunScript("ScriptEngineTest", "print \"Hello World\""));
  EXPECT_EQ("Hello World", StringTrim(helper.io()->captured()));
}

TEST_F(ScriptEngineTest, Legacy_ScriptingDisabled) {
  helper.config().scripting_enabled(false);
  auto engine = CreateScriptEngine(script_engine_t::legacy, bin, bout, helper.config(),
                                   &helper.context());
  ASSERT_NE(nullptr, engine);
  EXPECT_FALSE(engine->RunScript("ScriptEngineTest", "print \"Hello World\""));
  EXPECT_THAT(helper.io()->captured(), testing::HasSubstr("not enabled"));
}

TEST_F(ScriptEngineTest, Next_Availability) {
  auto engine = CreateScriptEngine(script_engine_t::next, bin, bout, helper.config(),
                                   &helper.context());
#ifdef WWIV_WWIVBASIC_NEXT
  EXPECT_TRUE(script_engine_available(script_engine_t::next));
  EXPECT_NE(nullptr, engine);
#else
  EXPECT_FALSE(script_engine_available(script_engine_t::next));
  EXPECT_EQ(nullptr, engine);
#endif
}

#ifdef WWIV_WWIVBASIC_NEXT
TEST_F(ScriptEngineTest, Next_ScriptingDisabled) {
  helper.config().scripting_enabled(false);
  auto engine = CreateScriptEngine(script_engine_t::next, bin, bout, helper.config(),
                                   &helper.context());
  ASSERT_NE(nullptr, engine);
  EXPECT_FALSE(engine->RunScript("ScriptEngineTest", "print \"Hello World\""));
  EXPECT_THAT(helper.io()->captured(), testing::HasSubstr("not enabled"));
}
#endif

#ifdef WWIV_WWIVBASIC_NEXT
TEST_F(ScriptEngineTest, Next_ScriptFile_Missing) {
  auto engine = CreateScriptEngine(script_engine_t::next, bin, bout, helper.config(),
                                   &helper.context());
  ASSERT_NE(nullptr, engine);
  EXPECT_FALSE(engine->RunScript("missing.bas"));
  EXPECT_THAT(helper.io()->captured(), testing::HasSubstr("Unable to locate script: missing.bas"));
}

TEST_F(ScriptEngineTest, Next_ScriptFile_IsRead) {
  const auto dir = helper.config().scriptdir();
  std::filesystem::create_directories(dir);
  {
    TextFile f(FilePath(dir, "hello.bas"), "wt");
    ASSERT_TRUE(f.IsOpen());
    f.Write("print \"Hello World\"\n");
  }
  auto engine = CreateScriptEngine(script_engine_t::next, bin, bout, helper.config(),
                                   &helper.context());
  ASSERT_NE(nullptr, engine);
  EXPECT_TRUE(engine->RunScript("hello.bas"));
  EXPECT_EQ("Hello World", StringTrim(helper.io()->captured()));
}
#endif
