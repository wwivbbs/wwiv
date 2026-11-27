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

#include "bbs/basic/next/next_engine.h"
#include "bbs/bbs.h"
#include "bbs/bbs_helper.h"
#include "bbs/application.h"
#include "common/context.h"
#include "common/input.h"
#include "common/output.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/version.h"
#include "sdk/config.h"
#include "bbs/basic/next/type_ahead_test_util.h"
#include "install_scripts.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace wwiv::bbs::basic;
using namespace wwiv::bbs::basic::type_ahead;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::strings;

// The scripts WWIV ships (install/scripts), run as the BBS runs them.
class ShippedScriptsTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    helper.config().scripting_enabled(true);
    // Input is typed ahead; without ANSI the line editor echoes plain text.
    helper.user()->set_flag(wwiv::sdk::User::flag_ansi, false);
    // The scripts, byte for byte, from install/scripts (see bbs/CMakeLists.txt).
    ASSERT_NO_FATAL_FAILURE(WriteScript("hello.bas", k_hello_bas, sizeof(k_hello_bas)));
    ASSERT_NO_FATAL_FAILURE(
        WriteScript("oneliners.bas", k_oneliners_bas, sizeof(k_oneliners_bas)));
    // The script engine checks the context's session; Input checks the BBS's.
    HangUpWhenKeysRunOut(helper.context().session_context(), a()->sess());
  }

  void WriteScript(const std::string& name, const unsigned char* bytes, size_t size) {
    const auto dir = helper.config().scriptdir();
    std::filesystem::create_directories(dir);
    std::ofstream f(FilePath(dir, name), std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(size));
    f.close();
    ASSERT_FALSE(f.fail()) << "unable to write " << name;
  }

  void TearDown() override { StopTypingAhead(); }

  void RemoveDataFiles() {
    // Collect first: removing entries while iterating is unspecified.
    std::vector<std::filesystem::path> files;
    for (const auto& f : std::filesystem::directory_iterator(helper.config().datadir())) {
      if (ends_with(f.path().filename().string(), ".script.json")) {
        files.push_back(f.path());
      }
    }
    for (const auto& f : files) {
      File::Remove(f);
    }
  }

  // Runs a script file and returns what it printed.
  std::string Run(const std::string& script) {
    helper.io()->Clear();
    NextEngine engine(bin, bout, helper.config(), &helper.context());
    EXPECT_TRUE(engine.RunScript(script)) << script;
    return helper.io()->captured();
  }

  BbsHelper helper{};
};

TEST_F(ShippedScriptsTest, Hello) {
  RemoveDataFiles();
  const auto out = Run("hello.bas");
  const auto version = wwiv::core::short_version();
  const std::string tail = "PrintList: \r\nLIST\r\na\r\nb\r\nc\r\nd\r\n"
                           "PrintList: \r\nLIST\r\na\r\nb\r\nc\r\nd\r\n";
  EXPECT_EQ(StrCat("Hello World\r\nwwiv\r\nYou are running version: ", version,
                   "\r\nlen: 4\r\nl=LIST[end]\r\n", tail),
            out);
}

// Adds a oneliner: yes, color 3, the text, not anonymous; then no more.
static std::string AddOneLiner(const std::string& text) {
  return StrCat("Y3", text, "\rNN");
}

TEST_F(ShippedScriptsTest, OneLiners) {
  RemoveDataFiles();
  TypeAhead(AddOneLiner("Hello world"));
  const auto out = Run("oneliners.bas");
  EXPECT_THAT(out, testing::HasSubstr("Enter Your One Liner:"));
  EXPECT_THAT(out, testing::HasSubstr("Hello world"));
}

TEST_F(ShippedScriptsTest, OneLiners_KeepsItsData) {
  RemoveDataFiles();
  // Each run adds a oneliner to the same wwiv.data file...
  TypeAhead(AddOneLiner("the first"));
  Run("oneliners.bas");
  TypeAhead(AddOneLiner("the second"));
  Run("oneliners.bas");

  // ...and the next lists both, in order.
  TypeAhead("N");
  const auto out = Run("oneliners.bas");
  const auto first = out.find("the first");
  const auto second = out.find("the second");
  EXPECT_NE(std::string::npos, first) << out;
  EXPECT_NE(std::string::npos, second) << out;
  EXPECT_LT(first, second);
}
