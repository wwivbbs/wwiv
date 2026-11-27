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
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "bbs/application.h"
#include "bbs/basic/next/bbs_file_base.h"
#include "bbs/basic/next/next_engine.h"
#include "bbs/bbs.h"
#include "bbs/bbs_helper.h"
#include "common/input.h"
#include "common/output.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/files/dirs.h"
#include "sdk/files/file_record.h"
#include "sdk/files/files.h"
#include "sdk/vardec.h"

#include <functional>
#include <memory>
#include <string>

using namespace wwiv::bbs::basic;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::files;
using testing::HasSubstr;
using testing::Not;

namespace {

// The BBS's file areas, counting how often a directory's file list is read,
// and calling on_user_dirs each time a script checks an area.
class HookedFileBase final : public FileBase {
public:
  explicit HookedFileBase(Application& app) : base_(app) {}

  [[nodiscard]] std::vector<UserDir> user_dirs() const override {
    if (on_user_dirs) {
      on_user_dirs();
    }
    return base_.user_dirs();
  }
  [[nodiscard]] std::optional<UserDir> current_user_dir() const override {
    return base_.current_user_dir();
  }
  [[nodiscard]] const Dirs& dirs() const override { return base_.dirs(); }
  [[nodiscard]] std::unique_ptr<FileArea> open(int dirnum) override {
    ++opens;
    return base_.open(dirnum);
  }
  [[nodiscard]] std::filesystem::path path(int dirnum) const override { return base_.path(dirnum); }

  int opens{0};
  std::function<void()> on_user_dirs;

private:
  BbsFileBase base_;
};

} // namespace

class WwivFilesTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    helper.config().scripting_enabled(true);
    a()->init_file_areas_for_test();
  }

  // Adds a directory to the BBS's dirs, with its files in dl_filename,
  // and its file list if create.
  int AddDir(const std::string& filename, bool create = true, uint16_t mask = 0) {
    directory_t d{};
    d.name = "The " + filename + " area";
    d.filename = filename;
    EXPECT_TRUE(helper.files().Mkdir("dl_" + filename));
    d.path = helper.files().DirName("dl_" + filename);
    d.mask = mask;
    const auto n = a()->dirs().size();
    EXPECT_TRUE(a()->dirs().insert(n, d));
    if (create) {
      EXPECT_TRUE(a()->fileapi()->Create(d));
    }
    return n;
  }

  // Adds a file to directory dirnum's list (at the top, as uploads are).
  void AddFile(int dirnum, const std::string& name, const std::string& desc, uint32_t size,
               const std::string& ext = "", int downloads = 0) {
    FileRecord f;
    ASSERT_TRUE(f.set_filename(name));
    ASSERT_TRUE(f.set_description(desc));
    ASSERT_TRUE(f.set_uploaded_by("RUSHFAN #1"));
    f.set_numbytes(static_cast<int>(size));
    f.u().numbytes = size;
    f.u().daten = kDaten;
    f.u().numdloads = static_cast<uint16_t>(downloads);
    auto area = a()->fileapi()->Open(a()->dirs()[dirnum]);
    ASSERT_NE(nullptr, area);
    if (ext.empty()) {
      ASSERT_TRUE(area->AddFile(f));
    } else {
      ASSERT_TRUE(area->AddFile(f, ext));
    }
    ASSERT_TRUE(area->Close());
  }

  std::string Run(const std::string& text, bool* ok = nullptr) {
    return Run(text, std::make_unique<BbsFileBase>(*a()), ok);
  }

  std::string Run(const std::string& text, std::unique_ptr<FileBase> files, bool* ok = nullptr) {
    NextEngine engine(bin, bout, helper.config(), &helper.context());
    engine.set_file_base(std::move(files));
    const auto r = engine.RunScript("filestest", text);
    if (ok) {
      *ok = r;
    }
    return helper.io()->captured();
  }

  // Two areas the user can list, "1" (two files) and "2" (empty), and one
  // they can't.
  void SetUpAreas() {
    const auto one = AddDir("one");
    AddDir("hidden");
    const auto two = AddDir("two");
    AddFile(one, "first.zip", "The first file", 1000);
    AddFile(one, "second.txt", "The second file", 2000, "Line one\r\nLine two", 3);
    a()->udir = {usersubrec{"1", static_cast<int16_t>(one)},
                 usersubrec{"2", static_cast<int16_t>(two)}};
  }

  static constexpr daten_t kDaten = 1700000000;
  BbsHelper helper{};
};

TEST_F(WwivFilesTest, ModuleName) {
  EXPECT_EQ("wwiv.files\r\n", Run("print wwiv.files.module_name()\n"));
}

TEST_F(WwivFilesTest, EnabledByDefault) {
  EXPECT_TRUE(helper.config().script_package_files_enabled());
}

TEST_F(WwivFilesTest, Disabled) {
  helper.config().script_package_files_enabled(false);
  EXPECT_FALSE(helper.config().script_package_files_enabled());
  bool ok{true};
  const auto out = Run("print wwiv.files.module_name()\nprint \"after\"\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, Not(HasSubstr("after")));
}

// Without a FileBase there's no wwiv.files module.
TEST_F(WwivFilesTest, NoFileBase) {
  NextEngine engine(bin, bout, helper.config(), &helper.context());
  EXPECT_FALSE(engine.RunScript("filestest", "print wwiv.files.module_name()\nprint \"after\"\n"));
  EXPECT_THAT(helper.io()->captured(), Not(HasSubstr("after")));
}

TEST_F(WwivFilesTest, Areas) {
  SetUpAreas();
  EXPECT_EQ("1 The one area 2\r\n2 The two area 0\r\n", Run(R"(
import "@wwiv.files"
for a in areas()
  print a.key; " "; a.name; " "; a.count
next
)"));
}

TEST_F(WwivFilesTest, Area) {
  SetUpAreas();
  EXPECT_EQ("The two area\r\n", Run("print wwiv.files.area(\"2\").name\n"));
}

TEST_F(WwivFilesTest, Area_NotTheUsers) {
  SetUpAreas();
  bool ok{true};
  const auto out = Run("print wwiv.files.area(\"3\").name\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("there is no file area '3'"));
}

TEST_F(WwivFilesTest, CurrentArea) {
  SetUpAreas();
  a()->set_current_user_dir_num(1);
  EXPECT_EQ("2\r\n", Run("print wwiv.files.current_area().key\n"));
}

TEST_F(WwivFilesTest, CurrentArea_None) {
  bool ok{true};
  const auto out = Run("print wwiv.files.current_area().key\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("the user isn't in a file area"));
}

// Newest first, as the BBS lists them.
TEST_F(WwivFilesTest, Files) {
  SetUpAreas();
  EXPECT_EQ("1 second.txt The second file RUSHFAN #1 2000 3 1700000000\r\n"
            "2 first.zip The first file RUSHFAN #1 1000 0 1700000000\r\n",
            Run(R"(
for f in wwiv.files.area("1").files()
  print f.number; " "; f.name; " "; f.description; " "; f.uploaded_by; " "; f.size; " "; f.downloads; " "; f.date
next
)"));
}

TEST_F(WwivFilesTest, Files_Mask) {
  SetUpAreas();
  EXPECT_EQ("first.zip\r\n-\r\nsecond.txt\r\n-\r\n", Run(R"(
a = wwiv.files.area("1")
for f in a.files("*.zip")
  print f.name
next
print "-"
for f in a.files("SEC*")
  print f.name
next
print "-"
for f in a.files("nope.*")
  print f.name
next
)"));
}

TEST_F(WwivFilesTest, Files_MaskTooLong) {
  SetUpAreas();
  bool ok{true};
  const auto out = Run("x = wwiv.files.area(\"1\").files(\"averylongname.zip\")\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("longer than 12 characters"));
}

TEST_F(WwivFilesTest, File) {
  SetUpAreas();
  EXPECT_EQ("first.zip\r\n", Run("print wwiv.files.area(\"1\").file(2).name\n"));
}

TEST_F(WwivFilesTest, File_OutOfRange) {
  SetUpAreas();
  bool ok{true};
  const auto out = Run("print wwiv.files.area(\"1\").file(3).name\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("there is no file 3 in 'The one area' (it has 2)"));
}

TEST_F(WwivFilesTest, Find) {
  SetUpAreas();
  EXPECT_EQ("2 2 0 0 0\r\n", Run(R"(
a = wwiv.files.area("1")
print a.find("first.zip"); " "; a.find("FIRST.ZIP"); " "; a.find("missing.zip"); " "; a.find("*.zip"); " "; a.find("averylongname.zip")
)"));
}

// FIND(f) looks a file up by its name: its number now in this area, or 0
// if this area doesn't have it.
TEST_F(WwivFilesTest, Find_File) {
  SetUpAreas();
  EXPECT_EQ("2 0\r\n", Run(R"(
a = wwiv.files.area("1")
f = a.file(2)
print a.find(f); " "; wwiv.files.area("2").find(f)
)"));
}

// FIND(f) finds f by its name as stored, even an old record's name in lower
// case, which the name scripts see doesn't align back to.
TEST_F(WwivFilesTest, Find_File_StoredLowerCase) {
  SetUpAreas();
  FileRecord f;
  ASSERT_TRUE(f.set_filename("old.zip"));
  wwiv::strings::to_char_array(f.u().filename, "old     .zip");
  ASSERT_TRUE(f.set_description("An old file"));
  auto area = a()->fileapi()->Open(a()->dirs()[0]);
  ASSERT_NE(nullptr, area);
  ASSERT_TRUE(area->AddFile(f));
  ASSERT_TRUE(area->Close());
  EXPECT_EQ("old.zip 1\r\n", Run(R"(
a = wwiv.files.area("1")
f = a.file(1)
print f.name; " "; a.find(f)
)"));
}

TEST_F(WwivFilesTest, Find_NotAName) {
  SetUpAreas();
  bool ok{true};
  const auto out = Run("print wwiv.files.area(\"1\").find(list())\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("expected a file name or a file"));
}

TEST_F(WwivFilesTest, ExtendedDescription) {
  SetUpAreas();
  EXPECT_EQ("2: Line one|Line two|\r\n0: \r\n", Run(R"(
a = wwiv.files.area("1")
for f in a.files()
  l = f.extended_description()
  print len(l); ": ";
  for x in l
    print x; "|";
  next
  print
next
)"));
}

TEST_F(WwivFilesTest, Available) {
  SetUpAreas();
  helper.files().CreateTempFile("dl_one/first.zip", "zip");
  EXPECT_EQ("FALSE TRUE\r\n", Run(R"(
a = wwiv.files.area("1")
print a.file(1).available; " "; a.file(2).available
)"));
}

// The BBS doesn't check that files in CD-ROM areas exist.
TEST_F(WwivFilesTest, Available_CdRom) {
  const auto cd = AddDir("cd", true, mask_cdrom);
  AddFile(cd, "disc.zip", "On the disc", 10);
  a()->udir = {usersubrec{"1", static_cast<int16_t>(cd)}};
  EXPECT_EQ("TRUE\r\n", Run("print wwiv.files.area(\"1\").file(1).available\n"));
}

TEST_F(WwivFilesTest, Size_Large) {
  const auto d = AddDir("big");
  AddFile(d, "big.iso", "Big", 3000000000u);
  a()->udir = {usersubrec{"1", static_cast<int16_t>(d)}};
  EXPECT_EQ("3000000000\r\n", Run("print wwiv.files.area(\"1\").file(1).size\n"));
}

// An area without a file list yet has no files, and listing it doesn't
// create one.
TEST_F(WwivFilesTest, NoFileList) {
  const auto d = AddDir("new", false);
  a()->udir = {usersubrec{"1", static_cast<int16_t>(d)}};
  EXPECT_EQ("0 0 0\r\n", Run(R"(
a = wwiv.files.area("1")
print a.count; " "; len(a.files()); " "; a.find("x.zip")
)"));
  EXPECT_FALSE(a()->fileapi()->Exist("new"));
}

// BbsFileBase follows the BBS's udir.
class BbsFileBaseTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    a()->init_file_areas_for_test();
    for (auto i = 0; i < 8; i++) {
      directory_t d{};
      d.name = fmt::format("dir{}", i);
      d.filename = d.name;
      d.path = "dloads/" + d.name;
      EXPECT_TRUE(a()->dirs().insert(i, d));
    }
  }

  BbsHelper helper{};
};

TEST_F(BbsFileBaseTest, UserDirs_SkipsDirsThatDontExist) {
  a()->udir = {usersubrec{"1", 0}, usersubrec{"2", 50}, usersubrec{"3", -1}};
  const BbsFileBase f(*a());
  const auto dirs = f.user_dirs();
  ASSERT_EQ(1u, dirs.size());
  EXPECT_EQ(0, dirs[0].dirnum);
  EXPECT_EQ("1", dirs[0].key);
}

TEST_F(BbsFileBaseTest, CurrentUserDir) {
  a()->udir = {usersubrec{"1", 99}, usersubrec{"G", 7}};
  const BbsFileBase f(*a());
  a()->set_current_user_dir_num(1);
  ASSERT_TRUE(f.current_user_dir());
  EXPECT_EQ(7, f.current_user_dir()->dirnum);
  EXPECT_EQ("G", f.current_user_dir()->key);
  // The current entry isn't a directory.
  a()->set_current_user_dir_num(0);
  EXPECT_EQ(std::nullopt, f.current_user_dir());
  a()->udir.clear();
  EXPECT_EQ(std::nullopt, f.current_user_dir());
}

TEST_F(BbsFileBaseTest, Open) {
  BbsFileBase f(*a());
  ASSERT_TRUE(a()->fileapi()->Create(a()->dirs()[2]));
  EXPECT_NE(nullptr, f.open(2));
  // No file list yet.
  EXPECT_EQ(nullptr, f.open(3));
  EXPECT_FALSE(a()->fileapi()->Exist("dir3"));
  EXPECT_EQ(nullptr, f.open(-1));
  EXPECT_EQ(nullptr, f.open(8));
}

TEST_F(BbsFileBaseTest, Path) {
  const BbsFileBase f(*a());
  EXPECT_EQ(File::absolute(a()->bbspath(), "dloads/dir1"), f.path(1));
  EXPECT_TRUE(f.path(8).empty());
}

// Going through an area file by file reads its file list once.
TEST_F(WwivFilesTest, FileList_ReadOnce) {
  SetUpAreas();
  auto files = std::make_unique<HookedFileBase>(*a());
  auto* hooked = files.get();
  EXPECT_EQ("first.zip second.txt 1 2 2\r\n", Run(R"(
a = wwiv.files.area("1")
n = a.count
print a.file(2).name; " "; a.file(1).name; " "; a.find("second.txt"); " "; len(a.file(1).extended_description()); " "; len(a.files())
)",
                                                  std::move(files)));
  EXPECT_EQ(1, hooked->opens);
}

// A file list is read again once it changes, so scripts see new uploads.
TEST_F(WwivFilesTest, FileList_SeesNewUploads) {
  SetUpAreas();
  auto files = std::make_unique<HookedFileBase>(*a());
  auto* hooked = files.get();
  int checks = 0;
  // AREA() looks at the user's areas once, then each COUNT once.
  hooked->on_user_dirs = [&] {
    if (++checks == 3) {
      AddFile(0, "new.zip", "Just uploaded", 10);
    }
  };
  EXPECT_EQ("2\r\n3\r\n", Run(R"(
a = wwiv.files.area("1")
print a.count
print a.count
)",
                              std::move(files)));
  EXPECT_EQ(2, hooked->opens);
}

// If directories are inserted while a script holds an area, so that its
// number is now another directory, the area no longer exists.
TEST_F(WwivFilesTest, Area_DirectoryMoved) {
  SetUpAreas();
  auto files = std::make_unique<HookedFileBase>(*a());
  int checks = 0;
  files->on_user_dirs = [&] {
    if (++checks == 2) {
      directory_t d{};
      d.name = "Sysop only";
      d.filename = "sysop";
      ASSERT_TRUE(a()->dirs().insert(0, d));
    }
  };
  bool ok{true};
  const auto out = Run(R"(
a = wwiv.files.area("1")
print a.name
print "after"
)",
                       std::move(files), &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("no longer exists"));
  EXPECT_THAT(out, Not(HasSubstr("Sysop only")));
}
