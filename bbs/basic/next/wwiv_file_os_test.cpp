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
#include "bbs/basic/script_engine.h"
#include "bbs/bbs.h"
#include "bbs/bbs_helper.h"
#include "common/input.h"
#include "common/output.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/config.h"

#include <filesystem>
#include <string>

using namespace wwiv::bbs::basic;
using namespace wwiv::core;
using namespace wwiv::strings;
using testing::ElementsAre;
using testing::HasSubstr;
using testing::Not;

// wwiv.io.file and wwiv.os. Where the legacy engine has the same behaviour,
// the tests run on both engines.
class WwivFileOsTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    helper.config().scripting_enabled(true);
    helper.config().script_package_file_enabled(true);
    helper.config().script_package_os_enabled(true);
  }

  std::string Run(script_engine_t e, const std::string& text, bool* ok = nullptr) {
    const auto engine = CreateScriptEngine(e, bin, bout, helper.config(), &helper.context());
    const auto r = engine->RunScript("filetest", text);
    if (ok) {
      *ok = r;
    }
    return helper.io()->captured();
  }

  [[nodiscard]] std::filesystem::path GFile(const std::string& name) {
    return FilePath(helper.context().session_context().dirs().gfiles_directory(), name);
  }

  static std::string ReadFile(const std::filesystem::path& p) {
    TextFile f(p, "rt");
    return f.ReadFileIntoString();
  }

  BbsHelper helper{};
};

static constexpr script_engine_t kEngines[] = {script_engine_t::legacy, script_engine_t::next};

TEST_F(WwivFileOsTest, BothEngines_ModuleNames) {
  for (const auto e : kEngines) {
    SCOPED_TRACE(to_string(e));
    EXPECT_EQ("wwiv.io.file\r\nwwiv.os\r\n",
              Run(e, "print wwiv.io.file.module_name()\nprint wwiv.os.module_name()\n"));
  }
}

TEST_F(WwivFileOsTest, BothEngines_FilePackageDisabled) {
  helper.config().script_package_file_enabled(false);
  for (const auto e : kEngines) {
    SCOPED_TRACE(to_string(e));
    bool ok{true};
    const auto out = Run(e, "print wwiv.io.file.module_name()\nprint \"after\"\n", &ok);
    EXPECT_FALSE(ok);
    EXPECT_THAT(out, Not(HasSubstr("wwiv.io.file\r\n")));
    EXPECT_THAT(out, Not(HasSubstr("after")));
  }
}

TEST_F(WwivFileOsTest, BothEngines_OsPackageDisabled) {
  helper.config().script_package_os_enabled(false);
  for (const auto e : kEngines) {
    SCOPED_TRACE(to_string(e));
    bool ok{true};
    const auto out = Run(e, "print wwiv.os.module_name()\nprint \"after\"\n", &ok);
    EXPECT_FALSE(ok);
    EXPECT_THAT(out, Not(HasSubstr("wwiv.os\r\n")));
    EXPECT_THAT(out, Not(HasSubstr("after")));
  }
}

// From basic_test.cpp's WWIV_IO_FILE_ReadIntoString.
TEST_F(WwivFileOsTest, BothEngines_ReadIntoString) {
  helper.files().CreateTempFile("gfiles/file1.msg", "this is file1");
  for (const auto e : kEngines) {
    SCOPED_TRACE(to_string(e));
    EXPECT_EQ("this is file1\r\n", Run(e, R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "file1.msg", "R")
s = wwiv.io.file.readintostring(h)
print s
)"));
  }
}

// From basic_test.cpp's WWIV_IO_FILE_ReadLines; READLINES appends.
TEST_F(WwivFileOsTest, BothEngines_ReadLines) {
  helper.files().CreateTempFile("gfiles/file1.msg", "1\r\n2\r\n3\r\n");
  for (const auto e : kEngines) {
    SCOPED_TRACE(to_string(e));
    const auto out = Run(e, R"(
lines = list("0")
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "file1.msg", "R")
wwiv.io.file.readlines(h, lines)
for x in lines
  print x
next
)");
    EXPECT_THAT(SplitString(out, "\r\n", true), ElementsAre("0", "1", "2", "3"));
  }
}

TEST_F(WwivFileOsTest, BothEngines_OnlyTheFileNameIsUsed) {
  helper.files().CreateTempFile("gfiles/file1.msg", "in gfiles");
  for (const auto e : kEngines) {
    SCOPED_TRACE(to_string(e));
    EXPECT_EQ("in gfiles\r\n", Run(e, R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "../../somewhere/file1.msg", "R")
print wwiv.io.file.readintostring(h)
)"));
  }
}

TEST_F(WwivFileOsTest, BothEngines_MissingFileReadsAsEmpty) {
  for (const auto e : kEngines) {
    SCOPED_TRACE(to_string(e));
    bool ok{};
    EXPECT_EQ("[]\r\n", Run(e, R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "nothere.txt", "R")
print "[" + wwiv.io.file.readintostring(h) + "]"
)", &ok));
    EXPECT_TRUE(ok);
  }
}

TEST_F(WwivFileOsTest, BothEngines_OpenOptions_TempIsAnError) {
  for (const auto e : kEngines) {
    SCOPED_TRACE(to_string(e));
    bool ok{true};
    const auto out = Run(e, "o = wwiv.io.file.open_options(\"TEMP\")\nprint \"after\"\n", &ok);
    EXPECT_FALSE(ok);
    EXPECT_THAT(out, Not(HasSubstr("after")));
  }
}

TEST_F(WwivFileOsTest, BothEngines_ModeW_Truncates) {
  helper.files().CreateTempFile("gfiles/out.txt", "old contents");
  for (const auto e : kEngines) {
    SCOPED_TRACE(to_string(e));
    bool ok{};
    Run(e, R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "out.txt", "W")
)", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ("", ReadFile(GFile("out.txt")));
  }
}

TEST_F(WwivFileOsTest, BothEngines_LastModified) {
  const auto path = helper.files().CreateTempFile("gfiles/file1.msg", "x");
  const auto expected = File::last_write_time(path);
  for (const auto e : kEngines) {
    SCOPED_TRACE(to_string(e));
    EXPECT_EQ(fmt::format("{}\r\n", expected), Run(e, R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "file1.msg", "R")
print wwiv.io.file.last_modified(h)
)"));
  }
}

// WRITE and WRITELINE are new in the new engine.

TEST_F(WwivFileOsTest, Next_WriteAndWriteLine) {
  bool ok{};
  const auto out = Run(script_engine_t::next, R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "scores.txt", "W")
wwiv.io.file.writeline(h, "Alice 10", "Bob 7")
wwiv.io.file.write(h, "no ", "newline")
)", &ok);
  ASSERT_TRUE(ok) << out;
  EXPECT_EQ("Alice 10\nBob 7\nno newline", ReadFile(GFile("scores.txt")));
}

TEST_F(WwivFileOsTest, Next_WriteThenReadBack) {
  bool ok{};
  // The file is written and closed when the first script ends.
  Run(script_engine_t::next, R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "scores.txt", "W")
wwiv.io.file.writeline(h, "one")
wwiv.io.file.writeline(h, 2)
)", &ok);
  ASSERT_TRUE(ok);
  const auto out = Run(script_engine_t::next, R"(
lines = list()
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "scores.txt", "R")
wwiv.io.file.readlines(h, lines)
for x in lines
  print x
next
)", &ok);
  ASSERT_TRUE(ok) << out;
  EXPECT_EQ("one\r\n2\r\n", out);
}

TEST_F(WwivFileOsTest, Next_WriteToFileOpenedForReading_IsAnError) {
  helper.files().CreateTempFile("gfiles/file1.msg", "keep me");
  bool ok{true};
  const auto out = Run(script_engine_t::next, R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "file1.msg", "R")
wwiv.io.file.write(h, "x")
)", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("WRITE: the file was not opened for writing"));
  EXPECT_EQ("keep me", ReadFile(GFile("file1.msg")));
}

TEST_F(WwivFileOsTest, Next_WriteNeedsAFile) {
  bool ok{true};
  const auto out = Run(script_engine_t::next, "wwiv.io.file.write(1, \"x\")\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("WRITE: expected a file from wwiv.io.file.open"));
}

TEST_F(WwivFileOsTest, Next_OpenNeedsOptions) {
  bool ok{true};
  const auto out = Run(script_engine_t::next, "h = wwiv.io.file.open(1, \"x.txt\", \"R\")\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("OPEN: expected options"));
}

TEST_F(WwivFileOsTest, Next_OpenNeedsAFileName) {
  bool ok{true};
  const auto out = Run(script_engine_t::next, R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "..", "W")
)", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("is not a file name"));
}

// MENUS reads from the current menu set's gfiles directory, where printfile
// looks for menu text, and can't be written (legacy accepted MENUS but used
// the BBS's gfiles).
TEST_F(WwivFileOsTest, Next_Menus_ReadsFromTheMenuSetGfiles) {
  const auto& dirs = helper.context().session_context().dirs();
  ASSERT_FALSE(dirs.current_menu_directory().empty());
  const auto dir = dirs.current_menu_gfiles_directory();
  std::filesystem::create_directories(dir);
  {
    TextFile f(FilePath(dir, "menu.txt"), "wt");
    f.Write("from menus");
  }
  helper.files().CreateTempFile("gfiles/menu.txt", "from gfiles");
  EXPECT_EQ("from menus\r\n", Run(script_engine_t::next, R"(
o = wwiv.io.file.open_options("MENUS")
h = wwiv.io.file.open(o, "menu.txt", "R")
print wwiv.io.file.readintostring(h)
)"));
}

TEST_F(WwivFileOsTest, Next_Menus_CantBeWritten) {
  bool ok{true};
  const auto out = Run(script_engine_t::next, R"(
o = wwiv.io.file.open_options("MENUS")
h = wwiv.io.file.open(o, "menu.txt", "W")
)", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("files in MENUS can only be opened for reading"));
}

TEST_F(WwivFileOsTest, Next_ExecOptions) {
  bool ok{};
  const auto out = Run(script_engine_t::next, R"(
o = wwiv.os.exec_options("STDIO", "TEMP", "CHAIN.TXT")
print o
)", &ok);
  EXPECT_TRUE(ok) << out;
  EXPECT_THAT(out, HasSubstr("exec_options"));
}

TEST_F(WwivFileOsTest, Next_ExecNeedsOptions) {
  bool ok{true};
  const auto out = Run(script_engine_t::next, "wwiv.os.exec(1, \"true\")\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("EXEC: expected options"));
}

TEST_F(WwivFileOsTest, Next_WriteCloseThenReadInTheSameScript) {
  bool ok{};
  const auto out = Run(script_engine_t::next, R"(
o = wwiv.io.file.open_options("GFILES")
w = wwiv.io.file.open(o, "scores.txt", "W")
wwiv.io.file.writeline(w, "Alice 10")
wwiv.io.file.close(w)
wwiv.io.file.close(w)
r = wwiv.io.file.open(o, "scores.txt", "R")
print "[" + wwiv.io.file.readintostring(r) + "]"
)", &ok);
  ASSERT_TRUE(ok) << out;
  EXPECT_THAT(out, HasSubstr("[Alice 10"));
}

TEST_F(WwivFileOsTest, Next_OpenAFileTheScriptIsWriting_IsAnError) {
  // Without this check the second OPEN would wait for the first's lock
  // forever on Unix.
  for (const auto* modes : {"W R", "W W", "R W"}) {
    SCOPED_TRACE(modes);
    const auto m = SplitString(modes, " ");
    bool ok{true};
    const auto out = Run(script_engine_t::next, fmt::format(R"(
o = wwiv.io.file.open_options("GFILES")
a = wwiv.io.file.open(o, "scores.txt", "{}")
b = wwiv.io.file.open(o, "scores.txt", "{}")
print "not reached"
)", m.at(0), m.at(1)), &ok);
    EXPECT_FALSE(ok);
    EXPECT_THAT(out, HasSubstr("is already open"));
    EXPECT_THAT(out, Not(HasSubstr("not reached")));
  }
}

TEST_F(WwivFileOsTest, Next_OpenAFileTwiceForReading_IsFine) {
  helper.files().CreateTempFile("gfiles/file1.msg", "x");
  bool ok{};
  const auto out = Run(script_engine_t::next, R"(
o = wwiv.io.file.open_options("GFILES")
a = wwiv.io.file.open(o, "file1.msg", "R")
b = wwiv.io.file.open(o, "file1.msg", "R")
print wwiv.io.file.readintostring(a) + wwiv.io.file.readintostring(b)
)", &ok);
  ASSERT_TRUE(ok) << out;
  EXPECT_EQ("xx\r\n", out);
}

TEST_F(WwivFileOsTest, Next_UseAfterClose_IsAnError) {
  helper.files().CreateTempFile("gfiles/file1.msg", "x");
  bool ok{true};
  const auto out = Run(script_engine_t::next, R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "file1.msg", "R")
wwiv.io.file.close(h)
print wwiv.io.file.readintostring(h)
)", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("READINTOSTRING: 'file1.msg' was closed"));
}

TEST_F(WwivFileOsTest, Next_WriteFails_IsAnError) {
  // A directory where the file should be: it can't be opened for writing.
  std::filesystem::create_directories(GFile("out.txt"));
  bool ok{true};
  const auto out = Run(script_engine_t::next, R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "out.txt", "W")
wwiv.io.file.write(h, "x")
)", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("WRITE: unable to write to 'out.txt'"));
}

TEST_F(WwivFileOsTest, Next_DeviceNamesAreNotFileNames) {
  for (const auto* name : {"NUL", "con.txt", "Com1", "LPT9.log", "PRN", "aux", "a:b", "c:x.txt", "x.txt:s"}) {
    SCOPED_TRACE(name);
    bool ok{true};
    const auto out = Run(script_engine_t::next, fmt::format(R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "{}", "W")
)", name), &ok);
    EXPECT_FALSE(ok);
    EXPECT_THAT(out, HasSubstr("is not a file name"));
  }
  // Names that only look like devices are fine.
  for (const auto* name : {"console.txt", "com10", "nullify.txt"}) {
    SCOPED_TRACE(name);
    bool ok{};
    Run(script_engine_t::next, fmt::format(R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "{}", "W")
)", name), &ok);
    EXPECT_TRUE(ok);
  }
}

TEST_F(WwivFileOsTest, Next_ReadTwice) {
  helper.files().CreateTempFile("gfiles/file1.msg", "1\r\n2\r\n");
  bool ok{};
  const auto out = Run(script_engine_t::next, R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "file1.msg", "R")
s = wwiv.io.file.readintostring(h)
lines = list()
wwiv.io.file.readlines(h, lines)
wwiv.io.file.readlines(h, lines)
print len(lines)
)", &ok);
  ASSERT_TRUE(ok) << out;
  EXPECT_EQ("4\r\n", out);
}

TEST_F(WwivFileOsTest, Next_ReadFromFileOpenedForWriting_IsAnError) {
  for (const auto* fn : {"readintostring(h)", "readlines(h, list())"}) {
    SCOPED_TRACE(fn);
    bool ok{true};
    const auto out = Run(script_engine_t::next, fmt::format(R"(
o = wwiv.io.file.open_options("GFILES")
h = wwiv.io.file.open(o, "out.txt", "W")
x = wwiv.io.file.{}
)", fn), &ok);
    EXPECT_FALSE(ok);
    EXPECT_THAT(out, HasSubstr("was opened for writing"));
  }
}

TEST_F(WwivFileOsTest, Next_ExecWithoutASession_IsAnError) {
  NextEngine engine(bin, bout, helper.config(), nullptr);
  EXPECT_FALSE(engine.RunScript("filetest", R"(
o = wwiv.os.exec_options("STDIO", "TEMP", "CHAIN.TXT")
wwiv.os.exec(o, "true")
)"));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("EXEC: no session"));
}
