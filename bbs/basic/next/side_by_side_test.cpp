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
#include "bbs/application.h"
#include "common/common_events.h"
#include "common/context.h"
#include "common/input.h"
#include "common/output.h"
#include "core/eventbus.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/version.h"
#include "sdk/config.h"
#include "side_by_side_scripts.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace wwiv::bbs::basic;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace {

// While set, the test is running a script with typed-ahead keys. If the
// script reads more keys than were typed, the caller "hangs up" instead of
// the test waiting forever for real input.
SessionContext* sessions_to_hang_up[2]{};

void HangUpWhenKeysRunOut(SessionContext& a, SessionContext& b) {
  [[maybe_unused]] static const auto registered = [] {
    // Input sends GiveupTimeslices only while it waits for a key that hasn't
    // been typed: here, that means the typed-ahead keys have run out.
    bus().add_handler<GiveupTimeslices>([] {
      if (!sessions_to_hang_up[0]) {
        return;
      }
      for (auto* s : sessions_to_hang_up) {
        s->hangup(true);
      }
      // Type one more key, so getkey returns it: with nothing to read after a
      // hangup, getkey reaches DLOG(FATAL) and aborts (issue #1758).
      std::strcpy(&bin.charbuffer[1], "\r");
      bin.charbufferpointer_ = 1;
    });
    return true;
  }();
  sessions_to_hang_up[0] = &a;
  sessions_to_hang_up[1] = &b;
}

} // namespace

// The scripts WWIV ships (install/scripts), run on both engines. The output
// must match, apart from the documented PRINT layout (wwivbasic/PORTING.md:
// my_basic put each PRINT item on its own line).
class SideBySideTest : public ::testing::Test {
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

  void TearDown() override {
    sessions_to_hang_up[0] = sessions_to_hang_up[1] = nullptr;
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

  // Runs a script file on an engine and returns what it printed.
  std::string Run(script_engine_t e, const std::string& script) {
    helper.io()->Clear();
    const auto engine = CreateScriptEngine(e, bin, bout, helper.config(), &helper.context());
    EXPECT_TRUE(engine->RunScript(script)) << to_string(e) << ": " << script;
    return helper.io()->captured();
  }

  // Output without line breaks, to compare text whatever the PRINT layout.
  static std::string WithoutLineBreaks(std::string s) {
    std::erase(s, '\r');
    std::erase(s, '\n');
    return s;
  }

  BbsHelper helper{};
};

TEST_F(SideBySideTest, Hello) {
  RemoveDataFiles();
  const auto legacy = Run(script_engine_t::legacy, "hello.bas");
  RemoveDataFiles();
  const auto next = Run(script_engine_t::next, "hello.bas");

  // The same text...
  EXPECT_EQ(WithoutLineBreaks(legacy), WithoutLineBreaks(next));

  // ...laid out by each engine's PRINT rules. Note "len: " and "4", and
  // "l=", "LIST" and "[end]": my_basic printed each item on its own line.
  const auto version = wwiv::core::short_version();
  const std::string tail = "PrintList: \r\nLIST\r\na\r\nb\r\nc\r\nd\r\n"
                           "PrintList: \r\nLIST\r\na\r\nb\r\nc\r\nd\r\n";
  EXPECT_EQ(StrCat("Hello World\r\nwwiv\r\nYou are running version: ", version,
                   "\r\nlen: \r\n4\r\nl=\r\nLIST\r\n[end]\r\n", tail),
            legacy);
  EXPECT_EQ(StrCat("Hello World\r\nwwiv\r\nYou are running version: ", version,
                   "\r\nlen: 4\r\nl=LIST[end]\r\n", tail),
            next);
}

// Adds a oneliner: yes, color 3, the text, not anonymous; then no more.
static std::string AddOneLiner(const std::string& text) {
  return StrCat("Y3", text, "\rNN");
}

TEST_F(SideBySideTest, OneLiners) {
  RemoveDataFiles();
  TypeAhead(AddOneLiner("Hello world"));
  const auto legacy = Run(script_engine_t::legacy, "oneliners.bas");
  RemoveDataFiles();
  TypeAhead(AddOneLiner("Hello world"));
  const auto next = Run(script_engine_t::next, "oneliners.bas");

  EXPECT_EQ(legacy, next);
  EXPECT_THAT(next, testing::HasSubstr("Enter Your One Liner:"));
  EXPECT_THAT(next, testing::HasSubstr("Hello world"));
}

TEST_F(SideBySideTest, OneLiners_EnginesShareTheirData) {
  RemoveDataFiles();
  // Each engine adds a oneliner to the same wwiv.data file...
  TypeAhead(AddOneLiner("from legacy"));
  Run(script_engine_t::legacy, "oneliners.bas");
  TypeAhead(AddOneLiner("from next"));
  Run(script_engine_t::next, "oneliners.bas");

  // ...and both then list both, in order.
  for (const auto e : {script_engine_t::legacy, script_engine_t::next}) {
    SCOPED_TRACE(to_string(e));
    TypeAhead("N");
    const auto out = Run(e, "oneliners.bas");
    const auto first = out.find("from legacy");
    const auto second = out.find("from next");
    EXPECT_NE(std::string::npos, first) << out;
    EXPECT_NE(std::string::npos, second) << out;
    EXPECT_LT(first, second);
  }
}
