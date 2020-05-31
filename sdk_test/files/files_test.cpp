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
#include "core/datafile.h"
#include "core/fake_clock.h"


#include "gtest/gtest.h"

#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "sdk/config.h"
#include "sdk/files/files.h"
#include "sdk_test/sdk_helper.h"
#include <string>

using namespace std;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::files;
using namespace wwiv::strings;

static uploadsrec ul(const std::string& fn, const std::string& desc, uint32_t size, daten_t daten) {
  uploadsrec u{};
  to_char_array(u.filename, align(fn));
  to_char_array(u.description, desc);
  u.numbytes = size;
  u.daten = daten;
  return u;
}

static uploadsrec ul(const std::string& fn, const std::string& desc, uint32_t size) {
  auto now = DateTime::now();
  return ul(fn, desc, size, now.to_daten_t());
}

class FilesTest : public testing::Test {
public:
  FilesTest() : config_(helper.root()) {
    EXPECT_TRUE(config_.IsInitialized());
    config_.set_paths_for_test(helper.data(), helper.msgs(), helper.gfiles(), helper.menus(),
                               helper.dloads(), helper.scripts());
  }

  void SetUp() override { helper.SetUp(); }

  std::filesystem::path path_for(const std::string& filename) const {
    return PathFilePath(config_.datadir(), StrCat(filename, ".dir"));
  }

  std::vector<uploadsrec> read_dir(const std::string& filename) const {
    DataFile<uploadsrec> file(path_for(filename));
    if (!file) {
      return {};
    }
    std::vector<uploadsrec> v;
    if (!file.ReadVector(v)) {
      return {};
    }
    return v;
  }

  SdkHelper helper;
  Config config_;
};

TEST_F(FilesTest, Smoke) {
  const string name = test_info_->name();

  FileApi api(config_.datadir());
  ASSERT_FALSE(api.Exist(name));
  ASSERT_TRUE(api.Create(name));
  const auto area = api.Open(name);
  ASSERT_TRUE(area);
  EXPECT_EQ(0, area->number_of_files());
}

TEST_F(FilesTest, Create) {
  const string name = test_info_->name();

  FileApi api(config_.datadir());
  ASSERT_FALSE(File::Exists(path_for(name)));
  ASSERT_TRUE(api.Create(name));
  EXPECT_TRUE(File::Exists(path_for(name)));
}

TEST_F(FilesTest, Header) {
  const string name = test_info_->name();

  FileApi api(config_.datadir());
  const auto dt = DateTime::now() - std::chrono::hours(1000);
  api.set_clock(std::make_unique<FakeClock>(dt));
  ASSERT_TRUE(api.Create(name));
  const auto area = api.Open(name);
  auto& h = area->header();
  EXPECT_EQ(h.u().daten, dt.to_daten_t());
  EXPECT_EQ(0u, h.u().numbytes);
}

TEST_F(FilesTest, Add) {
  const string name = test_info_->name();
  FileApi api(config_.datadir());
  ASSERT_TRUE(api.Create(name));
  auto area = api.Open(name);
  ASSERT_TRUE(area);

  FileRecord f{ul("FILENAME.ZIP", "", 1234)};
  ASSERT_TRUE(area->AddFile(f));
  ASSERT_EQ(1, area->number_of_files());
  area->Close();

  auto cu = read_dir(name);
  EXPECT_EQ(2, wwiv::stl::ssize(cu));
  EXPECT_STREQ(f.u().filename, cu.at(1).filename);
}

TEST_F(FilesTest, Add_Order) {
  const string name = test_info_->name();
  FileApi api(config_.datadir());
  ASSERT_TRUE(api.Create(name));
  auto area = api.Open(name);
  ASSERT_TRUE(area);

  FileRecord f1{ul("FILE0001.ZIP", "", 1234)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234)};
  ASSERT_TRUE(area->AddFile(f1));
  ASSERT_TRUE(area->AddFile(f2));
  ASSERT_EQ(2, area->number_of_files());
  area->Save();

  EXPECT_EQ(area->ReadFile(1).aligned_filename(), "FILE0002.ZIP");
  EXPECT_EQ(area->ReadFile(2).aligned_filename(), "FILE0001.ZIP");
}

TEST(FileRecordTest, Smoke) {
  FileRecord f(ul("foo.bar", "desc", 12345));

  EXPECT_EQ("FOO     .BAR", f.aligned_filename());
  EXPECT_EQ("foo.bar", f.unaligned_filename());
}

TEST_F(FilesTest, Add_Sort_FileName_Asc) {
  const string name = test_info_->name();
  FileApi api(config_.datadir());
  ASSERT_TRUE(api.Create(name));
  auto area = api.Open(name);
  ASSERT_TRUE(area);

  FileRecord f1{ul("FILE0001.ZIP", "", 1234)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234)};
  ASSERT_TRUE(area->AddFile(f1));
  ASSERT_TRUE(area->AddFile(f3));
  ASSERT_TRUE(area->AddFile(f2));
  area->Save();

  ASSERT_TRUE(area->Sort(FileAreaSortType::FILENAME_ASC));
  area->Save();

  EXPECT_EQ(area->ReadFile(1).aligned_filename(), "FILE0001.ZIP");
  EXPECT_EQ(area->ReadFile(2).aligned_filename(), "FILE0002.ZIP");
  EXPECT_EQ(area->ReadFile(3).aligned_filename(), "FILE0003.ZIP");
}

TEST_F(FilesTest, Add_Sort_FileName_Desc) {
  const string name = test_info_->name();
  FileApi api(config_.datadir());
  ASSERT_TRUE(api.Create(name));
  auto area = api.Open(name);
  ASSERT_TRUE(area);

  FileRecord f1{ul("FILE0001.ZIP", "", 1234)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234)};
  ASSERT_TRUE(area->AddFile(f1));
  ASSERT_TRUE(area->AddFile(f3));
  ASSERT_TRUE(area->AddFile(f2));
  area->Save();

  ASSERT_TRUE(area->Sort(FileAreaSortType::FILENAME_DESC));
  area->Save();

  EXPECT_EQ(area->ReadFile(1).aligned_filename(), "FILE0003.ZIP");
  EXPECT_EQ(area->ReadFile(2).aligned_filename(), "FILE0002.ZIP");
  EXPECT_EQ(area->ReadFile(3).aligned_filename(), "FILE0001.ZIP");
}

TEST_F(FilesTest, Add_Sort_FileDate_Asc) {
  const string name = test_info_->name();
  FileApi api(config_.datadir());
  ASSERT_TRUE(api.Create(name));
  auto area = api.Open(name);
  ASSERT_TRUE(area);

  auto now = DateTime::now().to_daten_t();

  FileRecord f1{ul("FILE0001.ZIP", "", 1234, now)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234, now - 100)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234, now - 200)};
  ASSERT_TRUE(area->AddFile(f1));
  ASSERT_TRUE(area->AddFile(f3));
  ASSERT_TRUE(area->AddFile(f2));
  area->Save();

  ASSERT_TRUE(area->Sort(FileAreaSortType::DATE_ASC));
  area->Save();

  EXPECT_EQ(area->ReadFile(1).aligned_filename(), "FILE0003.ZIP");
  EXPECT_EQ(area->ReadFile(2).aligned_filename(), "FILE0002.ZIP");
  EXPECT_EQ(area->ReadFile(3).aligned_filename(), "FILE0001.ZIP");
}

TEST_F(FilesTest, Add_Sort_FileDate_Desc) {
  const string name = test_info_->name();
  FileApi api(config_.datadir());
  ASSERT_TRUE(api.Create(name));
  auto area = api.Open(name);
  ASSERT_TRUE(area);

  auto now = DateTime::now().to_daten_t();

  FileRecord f1{ul("FILE0001.ZIP", "", 1234, now)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234, now - 200)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234, now - 100)};
  ASSERT_TRUE(area->AddFile(f1));
  ASSERT_TRUE(area->AddFile(f3));
  ASSERT_TRUE(area->AddFile(f2));
  area->Save();

  ASSERT_TRUE(area->Sort(FileAreaSortType::DATE_DESC));
  area->Save();

  EXPECT_EQ(area->ReadFile(1).aligned_filename(), "FILE0001.ZIP");
  EXPECT_EQ(area->ReadFile(2).aligned_filename(), "FILE0003.ZIP");
  EXPECT_EQ(area->ReadFile(3).aligned_filename(), "FILE0002.ZIP");
}

TEST(FileRecordTest, Set_FileName) {
  FileRecord f(ul("foo.bar", "desc", 12345));
  ASSERT_TRUE(f.set_filename("bar.baz"));

  EXPECT_EQ("BAR     .BAZ", f.aligned_filename());
  EXPECT_EQ("bar.baz", f.unaligned_filename());
}

TEST(FileAlignTest, Align) {
  EXPECT_EQ("        .   ", align(""));
  EXPECT_EQ("X       .   ", align("x"));
  EXPECT_EQ("X       .Z  ", align("x.z"));
  EXPECT_EQ("X       .ZZ ", align("x.zz"));
  EXPECT_EQ("FILENAME.ZIP", align("FILENAME.ZIP"));
  EXPECT_EQ("FILENAME.ZIP", align("filename.zip"));
}

TEST(FileAlignTest, UnAlign) {
  EXPECT_EQ(unalign("        .   "), "");
  EXPECT_EQ(unalign("X       .   "), "x");
  EXPECT_EQ(unalign("X       .Z  "), "x.z");
  EXPECT_EQ(unalign("X       .ZZ "), "x.zz");
  EXPECT_EQ(unalign("FILENAME.ZIP"), "filename.zip");
}
