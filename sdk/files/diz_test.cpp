/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2020-2022, WWIV Software Services               */
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
#include "core/log.h"
#include "core/strings.h"

#include "gtest/gtest.h"

#include "core/test/file_helper.h"
#include "sdk/files/diz.h"

class DizTest : public testing::Test {
public:
  DizTest() {}
  wwiv::core::test::FileHelper helper;
};

TEST_F(DizTest, Smoke) {
  const std::string kDIZ = R"(Line1
Line2
Line3)";

  const auto file = helper.CreateTempFile("FILE_ID.DIZ", kDIZ);

  wwiv::sdk::files::DizParser p(true);
  auto o = p.parse(file);
  ASSERT_TRUE(o);

  auto d = o.value();
  EXPECT_EQ(d.description(), "Line1");
  EXPECT_EQ(d.extended_description(), "Line2\nLine3\n");
}

TEST_F(DizTest, Smoke_NotDescription) {
  const std::string kDIZ = R"(Line1
Line2
Line3)";

  const auto file = helper.CreateTempFile("FILE_ID.DIZ", kDIZ);

  wwiv::sdk::files::DizParser p(false);
  auto o = p.parse(file);
  ASSERT_TRUE(o);

  auto d = o.value();
  EXPECT_EQ(d.description(), "");
  EXPECT_EQ(d.extended_description(), "Line1\nLine2\nLine3\n");
}

TEST_F(DizTest, BlockChars) {
  const char kTEXT[] =
"\xDA\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4[ CyberWall v2.0\xE1""L ]\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4Ŀ\n"
"\xB3 Smaller  version  of  the  popular  door \xB3\n"
"\xB3 CyberWall.   Simple  and  easy  to  use, \xB3\n"
"\xB3 really  cool ANSi. Not  as many features \xB3\n"
"\xB3 as  the  full  version,  but  this  is a \xB3\n"
"\xB3 freeware copy if you don't wanna pay for \xB3\n"
"\xB3 the   full   version.   Freeware   Copy. \xB3\n"
"\xB3\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4ĳ\n"
"\xB3        --- LiQUID pRODUCTIoNS ---        \xB3\n"
"\xB3 MegaStuff BBS % (317) 873-5173 % 8am-5pm \xB3\n"
"\xB3\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xB3\n";

  const auto file = helper.CreateTempFile("FILE_ID.DIZ", kTEXT);

  wwiv::sdk::files::DizParser p(true);
  auto o = p.parse(file);
  ASSERT_TRUE(o);

  auto d = o.value();
  EXPECT_EQ(d.description(), "[ CyberWall v2.0\xE1L ]");

  auto v = wwiv::strings::SplitString(d.extended_description(), "\n");
  EXPECT_EQ(10u, v.size());
}

TEST_F(DizTest, EmptyLinesAtStart) {
  const auto* kTEXT = R"(
  <<< null e-magazine x00A (exec edition) >>>
      ____  _____          _____   _____
  ___/.   \/    /_________/.   /__/.   /__jp!_
 //_       \    .   /         /   \   /    _//
   /____/\____/    /    /__________________\
    +--- ---- \________/ --- -------- ----+
    : bbs scene and retro computing e-mag .
    :     https://github.com/xqtr/null    :
    '  telnet // andr01d.zapto.org:9999   :
    +---- -  ---------  ---------- -------+)";

  const auto file = helper.CreateTempFile("FILE_ID.DIZ", kTEXT);

  wwiv::sdk::files::DizParser p(true);
  auto o = p.parse(file);
  ASSERT_TRUE(o);

  const auto& d = o.value();
  EXPECT_EQ(d.description(), "<<< null e-magazine x00A (exec edition) >>>");
}

TEST_F(DizTest, IsDizFilename) {
  using wwiv::sdk::files::is_diz_filename;
  EXPECT_TRUE(is_diz_filename("FILE_ID.DIZ"));
  EXPECT_TRUE(is_diz_filename("file_id.diz"));
  EXPECT_TRUE(is_diz_filename("File_Id.Diz"));
  EXPECT_TRUE(is_diz_filename("DESC.SDI"));
  EXPECT_TRUE(is_diz_filename("desc.sdi"));
  EXPECT_FALSE(is_diz_filename("FILE_ID.TXT"));
  EXPECT_FALSE(is_diz_filename("README.DIZ"));
  EXPECT_FALSE(is_diz_filename(""));
}

TEST_F(DizTest, RemoveDizFiles_AnyCase) {
  helper.CreateTempFile("file_id.diz", "old");
  helper.CreateTempFile("DESC.SDI", "old");
  helper.CreateTempFile("keep.txt", "keep");

  EXPECT_EQ(2, wwiv::sdk::files::remove_diz_files(helper.TempDir()));
  EXPECT_FALSE(wwiv::sdk::files::find_diz_file(helper.TempDir()).has_value());
  EXPECT_TRUE(std::filesystem::exists(helper.TempDir() / "keep.txt"));
  EXPECT_EQ(0, wwiv::sdk::files::remove_diz_files(helper.TempDir()));
}

TEST_F(DizTest, FindDizFile_LowerCase) {
  helper.CreateTempFile("file_id.diz", "lower");
  const auto o = wwiv::sdk::files::find_diz_file(helper.TempDir());
  ASSERT_TRUE(o.has_value());
  EXPECT_EQ("file_id.diz", o->filename().string());
}

TEST_F(DizTest, FindDizFile_PrefersFileIdDiz) {
  helper.CreateTempFile("desc.sdi", "sdi");
  helper.CreateTempFile("FILE_ID.DIZ", "diz");
  const auto o = wwiv::sdk::files::find_diz_file(helper.TempDir());
  ASSERT_TRUE(o.has_value());
  EXPECT_EQ("FILE_ID.DIZ", o->filename().string());
}

TEST_F(DizTest, FindDizFile_None) {
  helper.CreateTempFile("readme.txt", "nope");
  EXPECT_FALSE(wwiv::sdk::files::find_diz_file(helper.TempDir()).has_value());
}

// The //READIDZ bug: a lower case file_id.diz from one archive was left in
// the temp directory and read again for every following file.
TEST_F(DizTest, StaleLowerCaseDiz_NotReused) {
  helper.CreateTempFile("file_id.diz", "First file description");
  ASSERT_TRUE(wwiv::sdk::files::find_diz_file(helper.TempDir()).has_value());

  // What get_file_idz does before extracting the next archive.
  wwiv::sdk::files::remove_diz_files(helper.TempDir());

  // The next archive has no description, so nothing is extracted.
  EXPECT_FALSE(wwiv::sdk::files::find_diz_file(helper.TempDir()).has_value());
}

/**


 */