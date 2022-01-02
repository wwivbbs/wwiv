/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2022, WWIV Software Services               */
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

#include "bbs/bbs.h"
#include "bbs/qwk/qwk_text.h"
#include "bbs/bbs_helper.h"
#include "core/datafile.h"
#include "core/strings.h"
#include "sdk/filenames.h"
#include "sdk/qwk_config.h"
#include <string>

using wwiv::sdk::User;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::bbs::qwk;
using namespace wwiv::sdk;

class QwkTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    qwk_config_filename = FilePath(helper.data(), QWK_JSON);
  }

  BbsHelper helper{};
  std::filesystem::path qwk_config_filename;
};

TEST_F(QwkTest, ReadQwkConfig_Read_NoBulletins) {

  qwk_config qc{};
  qc.packet_name = "PAKTNAME";
  write_qwk_cfg(helper.config(), qc);

  auto q = read_qwk_cfg(helper.config());
  EXPECT_EQ("PAKTNAME", q.packet_name);
  EXPECT_EQ(0u, q.bulletins.size());
}

TEST_F(QwkTest, ReadQwkConfig_Read_TwoBulletins) {
  {
    qwk_config qc{};
    qc.packet_name = "PAKTNAME";
    qc.bulletins.emplace_back(qwk_bulletin{"name1", "path1"});
    qc.bulletins.emplace_back(qwk_bulletin{"name2", "path2"});
    write_qwk_cfg(helper.config(), qc);
  }

  auto q = read_qwk_cfg(helper.config());
  EXPECT_EQ("PAKTNAME", q.packet_name);
  EXPECT_EQ(2u, q.bulletins.size());

  auto b = q.bulletins.begin();
  EXPECT_EQ(b->name, "name1");
  EXPECT_EQ(b->path, "path1");
  ++b;
  EXPECT_EQ(b->name, "name2");
  EXPECT_EQ(b->path, "path2");
}

TEST_F(QwkTest, ReadQwkConfig_Write_NoBulletins) {
  qwk_config c{};
  c.packet_name = "TESTPAKT";

  ASSERT_FALSE(File::Exists(qwk_config_filename));
  write_qwk_cfg(helper.config(), c);
  ASSERT_TRUE(File::Exists(qwk_config_filename));

  auto q = read_qwk_cfg(helper.config());
  EXPECT_EQ("TESTPAKT", q.packet_name);
}

TEST_F(QwkTest, ReadQwkConfig_Write_OneBulletins) {
  qwk_config c{};
  c.packet_name = "TESTPAKT";
  qwk_bulletin b{"name1", "path1"};
  c.bulletins.emplace_back(b);

  ASSERT_FALSE(File::Exists(qwk_config_filename));
  write_qwk_cfg(helper.config(), c);
  ASSERT_TRUE(File::Exists(qwk_config_filename));

  auto q = read_qwk_cfg(helper.config());
  EXPECT_EQ(1u, q.bulletins.size());
}

TEST(Qwk1Test, TestGetQwkFromMessage) {
  const auto* const QWKFrom = "\x04"
                              "0QWKFrom:";

  const auto message = StrCat("\x3", "0Thisis a test\r\n", QWKFrom,
                              " Rushfan #1 @561\r\nTitle\r\nDate\r\nThis is the message");
  auto opt_to = get_qwk_from_message(message);
  ASSERT_TRUE(opt_to.has_value());

  const auto o = opt_to.value();
  EXPECT_STREQ("RUSHFAN #1 @561", o.c_str());
}

TEST(Qwk1Test, TestGetQwkFromMessage_NotFound) {
  const auto message = StrCat("\x3", "0Thisis a test\r\n",
                              " Rushfan #1 @561\r\nTitle\r\nDate\r\nThis is the message");
  const auto opt_to = get_qwk_from_message(message);
  ASSERT_FALSE(opt_to.has_value());
}

TEST(Qwk1Test, TestGetQwkFromMessage_Malformed_AtEndOfLine) {
  const auto* const QWKFrom = "\x04"
                              "0QWKFrom:";
  const auto message = StrCat("\x3", "0Thisis a test\r\n",
                              " Rushfan #1 @561\r\nTitle\r\nDate\r\nThis is the message", QWKFrom);
  const auto opt_to = get_qwk_from_message(message);
  ASSERT_FALSE(opt_to.has_value());
}