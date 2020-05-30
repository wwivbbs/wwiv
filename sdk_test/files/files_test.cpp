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

static uploadsrec ul(const std::string& fn, const std::string& desc, uint32_t size) {
  uploadsrec u{};
  to_char_array(u.filename, align(fn));
  to_char_array(u.description, desc);
  u.numbytes = size;
  return u;
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
