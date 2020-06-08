/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2020, WWIV Software Services               */
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
#include "filesapi_helper.h"
#include "core/datafile.h"
#include "core/fake_clock.h"


#include "gtest/gtest.h"

#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "sdk/config.h"
#include "sdk/files/files.h"
#include "sdk/files/files_ext.h"
#include "sdk_test/sdk_helper.h"
#include <string>

using namespace std;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::files;
using namespace wwiv::strings;


class FilesExtTest : public testing::Test {
public:
  FilesExtTest() : config_(helper.root()), api_(helper.data()), api_helper_(&api_) {
    EXPECT_TRUE(config_.IsInitialized());
    config_.set_paths_for_test(helper.data(), helper.msgs(), helper.gfiles(), helper.menus(),
                               helper.dloads(), helper.scripts());
  }

  void SetUp() override { helper.SetUp(); }

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
};

TEST_F(FilesExtTest, Smoke) {
  const string name = test_info_->name();

  const FileRecord f1{ul("FILE0001.ZIP", "", 1234)};
  const std::string s1 = "This\r\nIs\r\nF1";
  const FileRecord f2{ul("FILE0002.ZIP", "", 1234)};
  const std::string s2 = "This\r\nIs\r\nF2";
  auto area = api_helper_.CreateAndPopulate(name, {f1, f2});
  ASSERT_TRUE(area);
  ASSERT_EQ(2, area->number_of_files());

  auto* e = area->ext_desc().value();
  EXPECT_EQ(0, e->number_of_ext_descriptions());
  EXPECT_TRUE(e->AddExtended(f1, s1));
  EXPECT_TRUE(e->AddExtended(f2, s2));
  EXPECT_EQ(2, e->number_of_ext_descriptions());

  EXPECT_EQ(s1, e->ReadExtended(f1).value());
  EXPECT_EQ(s2, e->ReadExtended(f2).value());
}
