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
#include "gtest/gtest.h"

#include "bbs/bbs.h"
#include "bbs/qwk/qwk.h"
#include "bbs/qwk/qwk_config.h"
#include "bbs/qwk/qwk_struct.h"
#include "bbs/qwk/qwk_text.h"
#include "bbs_test/bbs_helper.h"
#include "core/datafile.h"
#include "core/strings.h"
#include "sdk/filenames.h"
#include <iostream>
#include <string>

using std::cout;
using std::endl;
using std::string;

using wwiv::sdk::User;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::bbs::qwk;

class QwkTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    filename = FilePath(helper.data(), QWK_CFG);
  }

  BbsHelper helper{};
  std::filesystem::path filename;
};

TEST_F(QwkTest, ReadQwkConfig_Read_NoBulletins) {

  {
    const auto datadir = helper.data();
    DataFile<qwk_config_430> f(FilePath(helper.data(), QWK_CFG), 
	File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate);
    ASSERT_TRUE(f);
    qwk_config_430 qc{};
    to_char_array(qc.packet_name, "RUSHFAN");
    EXPECT_TRUE(f.Write(&qc));
    f.Close();
  }

  auto q = read_qwk_cfg();
  EXPECT_EQ("RUSHFAN", q.packet_name);
  EXPECT_EQ(0, q.amount_blts);
  EXPECT_EQ(0u, q.bulletins.size());
}


TEST_F(QwkTest, ReadQwkConfig_Read_TwoBulletins) {
  {
    const auto datadir = helper.data();
    DataFile<qwk_config_430> f(filename, 
	File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate);
    ASSERT_TRUE(f);
    qwk_config_430 qc{};
    to_char_array(qc.packet_name, "RUSHFAN");
    qc.amount_blts = 2;
    EXPECT_TRUE(f.Write(&qc));
    f.Close();
  }

  {
    File f(filename);
    ASSERT_TRUE(f.Open(File::modeCreateFile | File::modeWriteOnly));
    f.Seek(656 /* sizeof (qwk_config_430) */, File::Whence::begin);
    char path[BULL_SIZE];
    char name[BNAME_SIZE];
    to_char_array(path, "path1");
    f.Write(path, BULL_SIZE);
    to_char_array(path, "path2");
    f.Write(path, BULL_SIZE);
    to_char_array(name, "name1");
    f.Write(name, BNAME_SIZE);
    to_char_array(name, "name2");
    f.Write(name, BNAME_SIZE);
  }

  auto q = read_qwk_cfg();
  EXPECT_EQ("RUSHFAN", q.packet_name);
  EXPECT_EQ(2, q.amount_blts) << filename;
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

  ASSERT_FALSE(File::Exists(filename));
  write_qwk_cfg(c);
  ASSERT_TRUE(File::Exists(filename));

  {
    File f(filename);
    EXPECT_EQ(656, f.length());
  }

  {
    DataFile<qwk_config_430> f(filename, File::modeReadOnly | File::modeBinary);
    ASSERT_TRUE(f);
    qwk_config_430 q4{};
    ASSERT_TRUE(f.Read(&q4));

    EXPECT_STREQ("TESTPAKT", q4.packet_name);
  }
}

TEST_F(QwkTest, ReadQwkConfig_Write_TwoBulletins) {
  qwk_config c{};
  c.packet_name = "TESTPAKT";
  qwk_bulletin b{"name1", "path1"};
  c.bulletins.emplace_back(b);

  ASSERT_FALSE(File::Exists(filename));
  write_qwk_cfg(c);

  ASSERT_TRUE(File::Exists(filename));
  File f(filename);
  EXPECT_EQ(656 + BULL_SIZE + BNAME_SIZE, f.length());
}

TEST(Qwk1Test, TestGetQwkFromMessage) {
  const auto* const QWKFrom = "\x04""0QWKFrom:";

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
  const auto* const QWKFrom = "\x04""0QWKFrom:";
  const auto message = StrCat("\x3", "0Thisis a test\r\n",
                              " Rushfan #1 @561\r\nTitle\r\nDate\r\nThis is the message", QWKFrom);
  const auto opt_to = get_qwk_from_message(message);
  ASSERT_FALSE(opt_to.has_value());
}