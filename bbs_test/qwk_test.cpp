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

#include <iostream>
#include <memory>
#include <string>

#include "bbs/bbs.h"
#include "bbs/pause.h"
#include "bbs/qwk.h"
#include "bbs_test/bbs_helper.h"
#include "core/datafile.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "deps/googletest/googletest/include/gtest/gtest.h"
#include "sdk/filenames.h"

using std::cout;
using std::endl;
using std::string;

using wwiv::bbs::TempDisablePause;
using wwiv::sdk::User;
using namespace wwiv::core;
using namespace wwiv::strings;

class QwkTest : public ::testing::Test {
protected:
  void SetUp() override { helper.SetUp(); }

protected:
  BbsHelper helper{};
};

TEST_F(QwkTest, ReadQwkConfig_NoBulletins) {

  {
    const auto datadir = helper.data();
    DataFile<qwk_config_430> f(PathFilePath(helper.data(), QWK_CFG), 
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


TEST_F(QwkTest, ReadQwkConfig_TwoBulletins) {

  const auto filename = PathFilePath(helper.data(), QWK_CFG);
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
