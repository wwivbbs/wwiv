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

/**


 */