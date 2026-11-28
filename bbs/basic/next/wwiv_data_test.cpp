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
#include "common/input.h"
#include "common/output.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/config.h"

#include <filesystem>
#include <string>
#include <vector>

using namespace wwiv::bbs::basic;
using namespace wwiv::core;
using namespace wwiv::strings;
using testing::HasSubstr;

// wwiv.data. It must read and write the same files, in the same format, as
// the legacy (my_basic) engine did, so scripts keep their data.
class WwivDataTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    helper.config().scripting_enabled(true);
  }

  // Runs text as module "datatest" and returns what it printed.
  // Sets *ok to whether the script succeeded.
  std::string Run(const std::string& text, bool* ok = nullptr) {
    NextEngine engine(bin, bout, helper.config(), &helper.context());
    const auto r = engine.RunScript("datatest", text);
    if (ok) {
      *ok = r;
    }
    return helper.io()->captured();
  }

  [[nodiscard]] std::filesystem::path DataFile(const std::string& name) const {
    return FilePath(helper.config().datadir(), name);
  }

  // Reads and removes a data file.
  std::string TakeFile(const std::string& name) {
    const auto path = DataFile(name);
    std::string s;
    {
      TextFile f(path, "r");
      s = f.ReadFileIntoString();
    }
    File::Remove(path);
    return s;
  }

  // Removes the data files written so far.
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

  BbsHelper helper{};
};

// Saves a little of everything; 0.1 checks that REALs are stored as 32-bit
// floats, like my_basic's.
static const char kSaveAll[] =
    "l = list(1, 2.5, \"abc\", 0.1, -7)\nwwiv.data.save(\"GLOBAL\", l)\n";
// Prints each loaded item on its own line.
static const char kPrintLoaded[] = R"(l = list()
wwiv.data.load("GLOBAL", l)
for x in l
  print x
next
)";

// The files, as the legacy (my_basic) engine wrote them; until it was removed,
// these tests checked that both engines wrote the same bytes.
static const char kSaveAllFile[] = R"({
    "data": [
        {
            "type": 1,
            "i": 1
        },
        {
            "type": 2,
            "r": 2.5
        },
        {
            "type": 0,
            "s": "abc"
        },
        {
            "type": 2,
            "r": 0.10000000149011612
        },
        {
            "type": 1,
            "i": -7
        }
    ]
})";
static const char kBooleansFile[] = R"({
    "data": [
        {
            "type": 1,
            "i": 1
        },
        {
            "type": 1,
            "i": 0
        }
    ]
})";
static const char kNestedListFile[] = R"({
    "data": [
        {
            "type": 1,
            "i": 1
        },
        {
            "type": 0,
            "s": ""
        }
    ]
})";

TEST_F(WwivDataTest, SameFileFormat) {
  bool ok{};
  Run(kSaveAll, &ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(kSaveAllFile, TakeFile("datatest.script.json"));
}

TEST_F(WwivDataTest, BooleansAreSavedAsIntegers) {
  // my_basic had no booleans: TRUE was the integer 1.
  bool ok{};
  Run("wwiv.data.save(\"GLOBAL\", list(TRUE, FALSE))\n", &ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(kBooleansFile, TakeFile("datatest.script.json"));
}

TEST_F(WwivDataTest, NestedListIsSavedAsEmptyString) {
  bool ok{};
  Run("wwiv.data.save(\"GLOBAL\", list(1, list(2)))\n", &ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(kNestedListFile, TakeFile("datatest.script.json"));
}

TEST_F(WwivDataTest, ReadsLegacyFile) {
  {
    TextFile f(DataFile("datatest.script.json"), "wt");
    f.Write(kSaveAllFile);
  }
  bool ok{};
  EXPECT_EQ("1\r\n2.5\r\nabc\r\n0.1\r\n-7\r\n", Run(kPrintLoaded, &ok));
  EXPECT_TRUE(ok);
}

TEST_F(WwivDataTest, ReadsWhatItSaved) {
  bool ok{};
  Run(kSaveAll, &ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ("1\r\n2.5\r\nabc\r\n0.1\r\n-7\r\n", Run(kPrintLoaded, &ok));
  EXPECT_TRUE(ok);
}

TEST_F(WwivDataTest, ModuleName) {
  RemoveDataFiles();
  EXPECT_EQ("wwiv.data\r\n", Run("print wwiv.data.module_name()\n"));
}

TEST_F(WwivDataTest, LoadAppends) {
  RemoveDataFiles();
  Run("wwiv.data.save(\"GLOBAL\", list(1, 2))\n");
  EXPECT_EQ("3\r\n", Run(R"(l = list("x")
wwiv.data.load("GLOBAL", l)
print len(l)
)"));
}

TEST_F(WwivDataTest, LoadMissingFile_LeavesListAlone) {
  RemoveDataFiles();
  EXPECT_EQ("1\r\n", Run(R"(l = list("x")
wwiv.data.load("GLOBAL", l)
print len(l)
)"));
}

TEST_F(WwivDataTest, UserScope_FileNamedForTheUser) {
  RemoveDataFiles();
  helper.context().session_context().user_num(12);
  bool ok{};
  // The scope is case-insensitive.
  Run("wwiv.data.save(\"user\", list(\"mine\"))\n", &ok);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(File::Exists(DataFile("datatest.user.12.script.json")));
  EXPECT_FALSE(File::Exists(DataFile("datatest.script.json")));
  EXPECT_EQ("mine\r\n", Run(R"(l = list()
wwiv.data.load("USER", l)
for x in l
  print x
next
)"));
}

TEST_F(WwivDataTest, OtherScopesAreGlobal) {
  RemoveDataFiles();
  bool ok{};
  Run("wwiv.data.save(\"whatever\", list(1))\n", &ok);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(File::Exists(DataFile("datatest.script.json")));
}

TEST_F(WwivDataTest, ModuleNameIsLowerCase) {
  RemoveDataFiles();
  NextEngine engine(bin, bout, helper.config(), &helper.context());
  EXPECT_TRUE(engine.RunScript("MixedCase", "wwiv.data.save(\"GLOBAL\", list(1))\n"));
  EXPECT_TRUE(File::Exists(DataFile("mixedcase.script.json")));
}

TEST_F(WwivDataTest, ScriptFileModuleName) {
  RemoveDataFiles();
  const auto dir = helper.config().scriptdir();
  std::filesystem::create_directories(dir);
  {
    TextFile f(FilePath(dir, "Hello.bas"), "wt");
    f.Write("wwiv.data.save(\"GLOBAL\", list(1))\n");
  }
  NextEngine engine(bin, bout, helper.config(), &helper.context());
  EXPECT_TRUE(engine.RunScript("Hello.bas"));
  EXPECT_TRUE(File::Exists(DataFile("hello.script.json")));
}

TEST_F(WwivDataTest, SaveNotAList_StopsTheScript) {
  RemoveDataFiles();
  bool ok{true};
  const auto out = Run("wwiv.data.save(\"GLOBAL\", 1)\nprint \"after\"\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("Only saving a LIST is currently supported"));
  EXPECT_THAT(out, testing::Not(HasSubstr("after")));
  EXPECT_FALSE(File::Exists(DataFile("datatest.script.json")));
}

TEST_F(WwivDataTest, LoadNotAList_StopsTheScript) {
  RemoveDataFiles();
  bool ok{true};
  const auto out = Run("x = 1\nwwiv.data.load(\"GLOBAL\", x)\nprint \"after\"\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("Only loading a LIST is currently supported"));
  EXPECT_THAT(out, testing::Not(HasSubstr("after")));
}

TEST_F(WwivDataTest, ScopeMustBeAString) {
  RemoveDataFiles();
  bool ok{true};
  Run("wwiv.data.save(1, list(1))\nprint \"after\"\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_FALSE(File::Exists(DataFile("datatest.script.json")));
}

// my_basic couldn't hold such REALs, and didn't check module names or the
// context.

// A REAL bigger than a 32-bit float can hold (about 3.4e38): 1/3 times
// 10^40, about 3.3e39.
static const char kHugeReal[] = R"(x = 1 / 3
for i = 1 to 40
  x = x * 10
next
)";

TEST_F(WwivDataTest, SaveRealTooLargeForAFloat_IsAnError) {
  bool ok{true};
  const auto out = Run(StrCat(kHugeReal, "wwiv.data.save(\"GLOBAL\", list(1, x))\n"), &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("SAVE: can't save"));
  EXPECT_FALSE(File::Exists(DataFile("datatest.script.json")));
}

TEST_F(WwivDataTest, SaveInfinity_IsAnError) {
  bool ok{true};
  const auto out =
      Run(StrCat(kHugeReal, "for i = 1 to 300\n  x = x * 10\nnext\n",
                 "wwiv.data.save(\"GLOBAL\", list(x))\n"),
          &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("SAVE: can't save"));
  EXPECT_FALSE(File::Exists(DataFile("datatest.script.json")));
}

TEST_F(WwivDataTest, SaveLargestFloat_IsFine) {
  bool ok{};
  // 3.4e38 is just under the largest float.
  Run("x = 34 / 10\nfor i = 1 to 38\n  x = x * 10\nnext\nwwiv.data.save(\"GLOBAL\", list(x))\n",
      &ok);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(File::Exists(DataFile("datatest.script.json")));
}

TEST_F(WwivDataTest, ModuleOutsideDataDir_IsAnError) {
  NextEngine engine(bin, bout, helper.config(), &helper.context());
  EXPECT_FALSE(engine.RunScript("../escape", "wwiv.data.save(\"GLOBAL\", list(1))\n"));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("can't use data files for the module"));
  const auto outside = FilePath(helper.config().datadir(), "../escape.script.json");
  EXPECT_FALSE(File::Exists(outside));
}

TEST_F(WwivDataTest, UserScopeWithoutContext_IsAnError) {
  NextEngine engine(bin, bout, helper.config(), nullptr);
  EXPECT_FALSE(engine.RunScript("datatest", "wwiv.data.save(\"USER\", list(1))\n"));
  EXPECT_THAT(helper.io()->captured(), HasSubstr("no current user"));
  EXPECT_FALSE(File::Exists(DataFile("datatest.user.0.script.json")));
}

TEST_F(WwivDataTest, GlobalScopeWithoutContext_Works) {
  NextEngine engine(bin, bout, helper.config(), nullptr);
  EXPECT_TRUE(engine.RunScript("datatest", "wwiv.data.save(\"GLOBAL\", list(1))\n"));
  EXPECT_TRUE(File::Exists(DataFile("datatest.script.json")));
}
