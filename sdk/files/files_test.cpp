/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2020-2021, WWIV Software Services               */
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
#include "core/file_helper.h"
#include "sdk/config.h"
#include "sdk/files/files.h"
#include "sdk/sdk_helper.h"
#include "sdk/files/filesapi_helper.h"
#include <string>

using namespace std;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::files;
using namespace wwiv::strings;


class FilesTest : public testing::Test {
public:
  FilesTest() : config_(helper.root()), api_(helper.data()), api_helper_(&api_) {
    EXPECT_TRUE(config_.IsInitialized());
    config_.set_paths_for_test(helper.data(), helper.msgs(), helper.gfiles(), helper.menus(),
                               helper.dloads(), helper.scripts());

    files_.emplace_back(ul("FILE0001.ZIP", "", 1234));
    files_.emplace_back(ul("FILE0002.ZIP", "", 2345));
    files_.emplace_back(ul("FILE0003.ZIP", "", 3456));
  }

  void SetUp() override {
    helper.SetUp();
  }

  [[nodiscard]] std::filesystem::path path_for(const std::string& filename) const {
    return FilePath(config_.datadir(), StrCat(filename, ".dir"));
  }

  [[nodiscard]] std::vector<uploadsrec> read_dir(const std::string& filename) const {
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
  FileApi api_;
  FilesApiHelper api_helper_;
  std::vector<FileRecord> files_;
};

TEST_F(FilesTest, Smoke) {
  const string name = test_info_->name();
  auto area = api_helper_.CreateAndPopulate(name, {});
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

  const auto dt = DateTime::now() - std::chrono::hours(1000);
  api_.set_clock(std::make_unique<FakeClock>(dt));
  auto area = api_helper_.CreateAndPopulate(name, {});
  auto& h = area->header();
  EXPECT_EQ(h.u().daten, dt.to_daten_t());
  EXPECT_EQ(0u, h.u().numbytes);
}

TEST_F(FilesTest, Add) {
  const string name = test_info_->name();
  FileRecord f{ul("FILENAME.ZIP", "", 1234)};
  {
    auto area = api_helper_.CreateAndPopulate(name, {f});
    ASSERT_EQ(1, area->number_of_files());
    area->Close();
  }

  auto cu = read_dir(name);
  EXPECT_EQ(2, wwiv::stl::ssize(cu));
  EXPECT_STREQ(f.u().filename, cu.at(1).filename);
}

TEST_F(FilesTest, Add_ExtendedDescription) {
  const string name = test_info_->name();
  const auto now = DateTime::now().to_daten_t();
  FileRecord f1{ul("FILE0001.ZIP", "", 1234, now)};
  auto area = api_helper_.CreateAndPopulate(name, {});

  ASSERT_TRUE(area->AddFile(f1, "Hello"));
  auto opos = area->FindFile(f1);
  ASSERT_TRUE(opos);
  auto af = area->ReadFile(opos.value());
  EXPECT_TRUE(af.has_extended_description());
  EXPECT_STREQ("Hello", area->ReadExtendedDescriptionAsString(af).value().c_str());
}

TEST_F(FilesTest, Add_Order) {
  const string name = test_info_->name();
  FileRecord f1{ul("FILE0001.ZIP", "", 1234)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234)};
  auto area = api_helper_.CreateAndPopulate(name, {f1, f2});
  ASSERT_EQ(2, area->number_of_files());

  EXPECT_EQ(area->ReadFile(1).aligned_filename(), "FILE0002.ZIP");
  EXPECT_EQ(area->ReadFile(2).aligned_filename(), "FILE0001.ZIP");
}


TEST_F(FilesTest, Add_Sort_FileName_Asc) {
  const string name = test_info_->name();
  FileRecord f1{ul("FILE0001.ZIP", "", 1234)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234)};
  auto area = api_helper_.CreateAndPopulate(name, {f1, f3, f2});

  ASSERT_TRUE(area->Sort(FileAreaSortType::FILENAME_ASC));
  area->Save();

  EXPECT_EQ(area->ReadFile(1).aligned_filename(), "FILE0001.ZIP");
  EXPECT_EQ(area->ReadFile(2).aligned_filename(), "FILE0002.ZIP");
  EXPECT_EQ(area->ReadFile(3).aligned_filename(), "FILE0003.ZIP");
}

TEST_F(FilesTest, Add_Sort_FileName_Desc) {
  const string name = test_info_->name();

  FileRecord f1{ul("FILE0001.ZIP", "", 1234)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234)};
  auto area = api_helper_.CreateAndPopulate(name, {f1, f3, f2});

  ASSERT_TRUE(area->Sort(FileAreaSortType::FILENAME_DESC));
  EXPECT_TRUE(area->Save());

  EXPECT_EQ(area->ReadFile(1).aligned_filename(), "FILE0003.ZIP");
  EXPECT_EQ(area->ReadFile(2).aligned_filename(), "FILE0002.ZIP");
  EXPECT_EQ(area->ReadFile(3).aligned_filename(), "FILE0001.ZIP");
}

TEST_F(FilesTest, Add_Sort_FileDate_Asc) {
  const string name = test_info_->name();

  const auto now = DateTime::now().to_daten_t();
  FileRecord f1{ul("FILE0001.ZIP", "", 1234, now)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234, now - 100)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234, now - 200)};
  auto area = api_helper_.CreateAndPopulate(name, {f2, f1, f3});

  ASSERT_TRUE(area->Sort(FileAreaSortType::DATE_ASC));
  EXPECT_TRUE(area->Save());

  EXPECT_EQ(area->ReadFile(1).aligned_filename(), "FILE0003.ZIP");
  EXPECT_EQ(area->ReadFile(2).aligned_filename(), "FILE0002.ZIP");
  EXPECT_EQ(area->ReadFile(3).aligned_filename(), "FILE0001.ZIP");
}

TEST_F(FilesTest, Add_Sort_FileDate_Desc) {
  const string name = test_info_->name();
  const auto now = DateTime::now().to_daten_t();
  FileRecord f1{ul("FILE0001.ZIP", "", 1234, now)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234, now - 200)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234, now - 100)};
  auto area = api_helper_.CreateAndPopulate(name, {f1, f2, f3});

  ASSERT_TRUE(area->Sort(FileAreaSortType::DATE_DESC));
  area->Save();

  EXPECT_EQ(area->ReadFile(1).aligned_filename(), "FILE0001.ZIP");
  EXPECT_EQ(area->ReadFile(2).aligned_filename(), "FILE0003.ZIP");
  EXPECT_EQ(area->ReadFile(3).aligned_filename(), "FILE0002.ZIP");
}

TEST_F(FilesTest, DeleteFile) {
  const string name = test_info_->name();
  const auto now = DateTime::now().to_daten_t();
  FileRecord f1{ul("FILE0001.ZIP", "", 1234, now)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234, now - 200)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234, now - 100)};
  auto area = api_helper_.CreateAndPopulate(name, {f1, f2, f3});

  auto pos = area->FindFile(f1);
  ASSERT_EQ(3, pos.value());
  EXPECT_TRUE(area->DeleteFile(f1, pos.value()));
}

TEST_F(FilesTest, DeleteFile_WrongFile) {
  const string name = test_info_->name();
  const auto now = DateTime::now().to_daten_t();
  FileRecord f1{ul("FILE0001.ZIP", "", 1234, now)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234, now - 200)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234, now - 100)};
  auto area = api_helper_.CreateAndPopulate(name, {f1, f2, f3});

  auto pos = area->FindFile(f1);
  EXPECT_FALSE(area->DeleteFile(f3, pos.value()));
}

TEST_F(FilesTest, DeleteFile_Ext) {
  const string name = test_info_->name();
  const auto now = DateTime::now().to_daten_t();
  FileRecord f1{ul("FILE0001.ZIP", "", 1234, now)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234, now - 200)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234, now - 100)};
  auto area = api_helper_.CreateAndPopulate(name, {f1, f2, f3});

  auto pos = area->FindFile(f1);
  ASSERT_EQ(3, pos.value());
  auto f = area->ReadFile(pos.value());
  EXPECT_FALSE(f.has_extended_description());

  EXPECT_TRUE(area->AddExtendedDescription(f, pos.value(), "Hello"));
  f = area->ReadFile(pos.value());
  EXPECT_TRUE(f.has_extended_description());
  auto o = area->ext_desc().value()->ReadExtended(f);
  EXPECT_TRUE(o.has_value());
  EXPECT_STREQ("Hello", o.value().c_str());

  // Try to delete the wrong one
  EXPECT_FALSE(area->DeleteExtendedDescription(f3, pos.value()));

  EXPECT_TRUE(area->DeleteFile(pos.value()));

  EXPECT_FALSE(area->ext_desc().value()->ReadExtended("FILE0001.ZIP"));
}

TEST_F(FilesTest, DeleteExtendedDescription) {
  const string name = test_info_->name();
  const auto now = DateTime::now().to_daten_t();
  FileRecord f1{ul("FILE0001.ZIP", "", 1234, now)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234, now - 200)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234, now - 100)};
  auto area = api_helper_.CreateAndPopulate(name, {f1, f2, f3});

  auto pos = area->FindFile(f1);
  EXPECT_TRUE(area->AddExtendedDescription(f1, pos.value(), "Hello"));
  EXPECT_STREQ("Hello", area->ext_desc().value()->ReadExtended(f1).value().c_str());

  // Try to delete the wrong one
  EXPECT_FALSE(area->DeleteExtendedDescription(f3, pos.value()));
  // Delete the right one
  EXPECT_TRUE(area->DeleteExtendedDescription(f1, pos.value()));
  EXPECT_FALSE(area->ext_desc().value()->ReadExtended(f1).has_value());
}

TEST_F(FilesTest, FindFile) {
  const string name = test_info_->name();
  const auto now = DateTime::now().to_daten_t();
  FileRecord f1{ul("FILE0001.ZIP", "", 1234, now)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234, now - 200)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234, now - 100)};
  auto area = api_helper_.CreateAndPopulate(name, {f1, f2, f3});

  EXPECT_EQ(3, area->FindFile(f1).value());
  EXPECT_EQ(2, area->FindFile(f2).value());
}

TEST_F(FilesTest, SearchFile) {
  const string name = test_info_->name();
  const auto now = DateTime::now().to_daten_t();
  FileRecord f1{ul("FILE0001.ZIP", "", 1234, now)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234, now - 200)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234, now - 100)};
  auto area = api_helper_.CreateAndPopulate(name, {f1, f2, f3});

  EXPECT_EQ(1, area->SearchFile("F???????.ZIP").value());
  EXPECT_EQ(1, area->SearchFile("F???????.ZIP", 1).value());
  EXPECT_EQ(3, area->SearchFile("FILE0001.ZIP").value());
  EXPECT_EQ(2, area->SearchFile("F???????.ZIP", 2).value());
  EXPECT_FALSE(area->SearchFile("F???????.ZIP", 4).has_value());
}

TEST_F(FilesTest, UpdateFile) {
  const string name = test_info_->name();
  const auto now = DateTime::now().to_daten_t();
  FileRecord f1{ul("FILE0001.ZIP", "", 1234, now)};
  FileRecord f2{ul("FILE0002.ZIP", "", 1234, now - 200)};
  FileRecord f3{ul("FILE0003.ZIP", "", 1234, now - 100)};
  auto area = api_helper_.CreateAndPopulate(name, {f1, f2, f3});

  auto pos = area->FindFile(f1);
  f1.set_filename("foo.zip");
  EXPECT_TRUE(area->UpdateFile(f1, pos.value()));

  const auto f = area->ReadFile(pos.value());
  EXPECT_EQ(f.aligned_filename(), "FOO     .ZIP");
}

TEST_F(FilesTest, Update_ExtendedDescription) {
  const string name = test_info_->name();
  const auto now = DateTime::now().to_daten_t();
  FileRecord f1{ul("FILE0001.ZIP", "", 1234, now)};
  auto area = api_helper_.CreateAndPopulate(name, {f1});

  auto opos = area->FindFile(f1);
  const auto pos = opos.value();
  f1.set_numbytes(4321);
  ASSERT_TRUE(area->UpdateFile(f1, pos, "Hello"));
  ASSERT_TRUE(opos);
  auto af = area->ReadFile(opos.value());
  EXPECT_TRUE(af.has_extended_description());
  EXPECT_STREQ("Hello", area->ReadExtendedDescriptionAsString(af).value().c_str());
}

/////////////////////////////////////////////////////////////////////////////
//
// FileRecordTest
//

TEST(FileRecordTest, Smoke) {
  FileRecord f(ul("foo.bar", "desc", 12345));

  EXPECT_EQ("FOO     .BAR", f.aligned_filename());
  EXPECT_EQ("foo.bar", f.unaligned_filename());
}

TEST(FileRecordTest, Set_FileName) {
  FileRecord f(ul("foo.bar", "desc", 12345));
  ASSERT_TRUE(f.set_filename("bar.baz"));

  EXPECT_EQ("BAR     .BAZ", f.aligned_filename());
  EXPECT_EQ("bar.baz", f.unaligned_filename());
}

/////////////////////////////////////////////////////////////////////////////
//
// FileAlignTest
//

TEST(FileAlignTest, Align) {
  EXPECT_EQ("        .   ", align(""));
  EXPECT_EQ("X       .   ", align("x"));
  EXPECT_EQ("X       .Z  ", align("x.z"));
  EXPECT_EQ("X       .ZZ ", align("x.zz"));
  EXPECT_EQ("X       .ZZ ", align("x  x.zz"));
  EXPECT_EQ("FILENAME.ZIP", align("FILENAME.ZIP"));
  EXPECT_EQ("FILENAME.ZIP", align("filename.zip"));
}

TEST(FileAlignTest, Align_WildCard) {
  EXPECT_EQ("X???????.Z  ", align("x*.z"));
  EXPECT_EQ("X?      .Z  ", align("x?.z"));
  EXPECT_EQ("FILENAME.Z??", align("filename.z*"));
  EXPECT_EQ("FILENAME.Z? ", align("filename.z?"));
}

TEST(FileAlignTest, UnAlign) {
  EXPECT_EQ(unalign("        .   "), "");
  EXPECT_EQ(unalign("X       .   "), "x");
  EXPECT_EQ(unalign("X       .Z  "), "x.z");
  EXPECT_EQ(unalign("X       .ZZ "), "x.zz");
  EXPECT_EQ(unalign("FILENAME.ZIP"), "filename.zip");
}

/////////////////////////////////////////////////////////////////////////////
//
// FileName
//

TEST(FileNameTest, Smoke) {
  const FileName f("FOO     .BAR");

  EXPECT_EQ("FOO     .BAR", f.aligned_filename());
  EXPECT_EQ("foo.bar", f.unaligned_filename());
  EXPECT_EQ("FOO", f.basename());
  EXPECT_EQ("BAR", f.extension());
}