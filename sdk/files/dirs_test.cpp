/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2021, WWIV Software Services               */
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
#include "gtest/gtest.h"

#include "core/datafile.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/file_helper.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"
#include "sdk/files/dirs.h"
#include "sdk/sdk_helper.h"
#include <string>
#include <vector>
#include "cereal/external/rapidjson/document.h"

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

class DirsTest: public testing::Test {
protected:
  void SetUp() override { 
    helper.SetUp(); 
  }
  SdkHelper helper;
};

TEST_F(DirsTest, Smoke) {
  directoryrec_422_t ld{};
  to_char_array(ld.name, "Sysop Dir");
  to_char_array(ld.filename, "SYSOP");
  to_char_array(ld.path, "/wwiv/dloads/sysop/");
  ld.dsl = 240;

  std::vector<directoryrec_422_t> v;
  v.emplace_back(ld);
  const auto datadir = helper.files_.TempDir();
  {
    DataFile<directoryrec_422_t> f(FilePath(datadir, DIRS_DAT),
                                   File::modeReadWrite | File::modeCreateFile | File::modeBinary |
                                       File::modeTruncate);
    ASSERT_TRUE(f.WriteVectorAndTruncate(v));
  }
  files::Dirs dirs(datadir, 0);
  ASSERT_TRUE(dirs.LoadLegacy());

  EXPECT_EQ(dirs.size(), 1);
  EXPECT_TRUE(dirs.Save());
  EXPECT_TRUE(File::Exists(FilePath(datadir, DIRS_JSON)));

  std::vector<files::directory_t> new_dirs;
  {
    TextFile f(FilePath(datadir, DIRS_JSON), "rt");
    ASSERT_TRUE(f.IsOpen());
    auto s = f.ReadFileIntoVector();
    EXPECT_EQ("{", s.at(0));
    EXPECT_EQ("    \"version\": 1,", s.at(1));
    EXPECT_EQ("    \"dirs\": [", s.at(2));
    EXPECT_NE(s.at(4).find("Sysop Dir"), std::string::npos);
  }
  {
    TextFile f(FilePath(datadir, DIRS_JSON), "rt");
    ASSERT_TRUE(f.IsOpen());
    auto s = f.ReadFileIntoString();
    rapidjson::Document d;
    d.Parse(s.c_str());
    ASSERT_FALSE(d.HasParseError());
    auto dirlist = d["dirs"].GetArray();
    EXPECT_EQ(1u, dirlist.Size());
    const auto& d1 = dirlist[0];
    EXPECT_STREQ(d1["name"].GetString(), "Sysop Dir");
    EXPECT_STREQ(d1["filename"].GetString(), "SYSOP");
  }

}