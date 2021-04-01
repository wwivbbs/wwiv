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
#include "gtest/gtest.h"

#include "core/datafile.h"
#include "core/file.h"
#include "core_test/file_helper.h"
#include "sdk/net/net.h"
#include "sdk/files/tic.h"

using namespace wwiv::core;
using namespace wwiv::sdk::net;

TEST(TicTest, Smoke) {
  FileHelper helper;

  const std::string kFSXINFO = R"(
Created by HTick, written by Gabriel Plutzar
File fsxinfo.zip
Area fsx_info
Areadesc Weekly Infopacks (fsxNet, etc.)
Desc Weekly infopack for fsxNet
LDesc             ‹‹‹              ‹‹‹
LDesc         ±∞∞ﬂ∞∞± ‹‹∞ﬂﬂ∞€€ ±∞∞ ∞∞±
LDesc     ≤   ∞∞€ €∞∞‹€∞±‹‹‹‹‹‹∞∞€ €∞∞ net ±
LDesc  €ﬂﬂﬂﬂ  ∞∞€ ﬂﬂﬂ ‹‹‹  ∞€∞ ﬂﬂﬂ≤ﬂﬂﬂ  ﬂﬂﬂﬂ€
LDesc  €  ∞∞  ∞€€ﬂﬂ   ∞∞±  ±∞∞ ∞€€ﬂ€∞±  ∞∞∞ €
LDesc  ≤ ∞∞∞∞ ±∞∞     ∞€∞∞ ∞∞± ±∞∞ ∞€∞ ∞∞∞  €
LDesc  ± ∞∞∞∞ ∞ﬂﬂ     ±∞∞‹‹ﬂﬂ  ∞ﬂﬂ ±∞€  ∞   €
LDesc  ∞ ∞∞           ﬂﬂﬂ           ﬂﬂ      €
LDesc  ∞ ∞  fsxnet.txt  -- Info & App       €
LDesc  ± ∞  fsxnet.xxx  -- Nodelist         ≤
LDesc  ≤    fsxnet.zaa  -- Nodelist (ZIP)   ±
LDesc  ± ∞  history.txt -- History          ≤
LDesc  ≤    systems.txt -- BBS in fsxNet    ±
LDesc  €    fsxnet.na   -- Echo list        ∞
LDesc  ≤    fsx_file.na -- File echo list   ±
LDesc  ± ˙                                  ≤
LDesc  ∞ ˙˙                    ‹‹‹ infopack €
LDesc  ﬂﬂﬂﬂﬂﬂ˛ﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂ˛ﬂﬂﬂﬂﬂﬂﬂ‹±ﬂﬂﬂﬂﬂﬂﬂﬂ˛ﬂﬂ
Replaces fsxinfo.zip
From 21:2/100
To 21:2/115
Origin 21:1/1
Size 12
Crc AF083B2D
Path 21:1/1 1591287303 Thu Jun 04 16:15:03 2020 UTC htick/w32-mvcdll 1.9.0-cur 17-08-15
Seenby 21:1/1
Seenby 21:1/2
Pw WELCOME
Path 21:1/100 1591287395 Thu Jun 04 16:16:35 2020 UTC Mystic/1.12 A46
Seenby 21:1/100
Seenby 21:1/101
Seenby 21:1/151
Path 21:2/100 1591287460 Thu Jun 04 16:17:40 2020 UTC Mystic/1.12 A46
  )";

  const auto file = helper.CreateTempFile("fsxinfo.tic", kFSXINFO);
  const auto arc_path = wwiv::core::FilePath(helper.TempDir(), "fsxinfo.zip");
  {
    File af(arc_path);
    ASSERT_TRUE(af.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite));
    ASSERT_EQ(12, af.Write("hello world\n"));
    af.Close();
  }

  wwiv::sdk::files::TicParser p(file.parent_path());
  auto o = p.parse("fsxinfo.tic");
  ASSERT_TRUE(o);

  auto t = o.value();
  EXPECT_EQ(t.area, "fsx_info");
  EXPECT_EQ(18u, t.ldesc.size());
  EXPECT_EQ("Weekly infopack for fsxNet", t.desc);
  EXPECT_EQ("fsxinfo.zip", t.file);
  EXPECT_EQ("fsxinfo.zip", t.replaces);
  EXPECT_TRUE(t.exists());
  EXPECT_TRUE(t.crc_valid());
  EXPECT_TRUE(t.size_valid());
  EXPECT_TRUE(t.IsValid());
}


TEST(TicTest, Malformed_Addresses) {
  FileHelper helper;

  const std::string kFSXINFO = R"(
Created by PXTIC/Win v7.0 (c) 2018 Santronics
Area [HNET] INFO
Origin Me, 954:895/1
From Me, 954:895/1
To Morgul, 954:895/5
File HOBBYNET.ZIP
  )";

  const auto file = helper.CreateTempFile("HOBBYNET.tic", kFSXINFO);

  wwiv::sdk::files::TicParser p(file.parent_path());
  auto o = p.parse("HOBBYNET.tic");
  ASSERT_TRUE(o);

  const auto& t = o.value();
  EXPECT_EQ("954:895/5", t.to.as_string(false, false));
  EXPECT_EQ("954:895/1", t.origin.as_string(false, false));
  EXPECT_EQ("954:895/1", t.from.as_string(false, false));
}

TEST(TicTest, FindFileAreaForTic) {
  FileHelper helper;

  const std::string contents = R"(
Area AREANAME
Size 12
Crc AF083B2D
File sample.zip
)";
  const auto tic_path = helper.CreateTempFile("sample.tic", contents);
  const auto arc_path = wwiv::core::FilePath(helper.TempDir(), "sample.zip");
  {
    File af(arc_path);
    ASSERT_TRUE(af.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite));
    ASSERT_EQ(12, af.Write("hello world\n"));
    af.Close();
  }

  const auto data = helper.Dir("data");
  std::random_device rd{};
  wwiv::core::uuid_generator generator(rd);
  auto uuid = generator.generate();

  Network net{};
  net.name = "foo";
  net.uuid = uuid;

  wwiv::sdk::files::dir_area_t dt;
  dt.net_uuid = uuid;
  dt.area_tag = "AREANAME";
  wwiv::sdk::files::directory_t dir;
  dir.area_tags.emplace_back(dt);
  dir.name = "d1";

  wwiv::sdk::files::Dirs dirs(data, 0);
  dirs.set_dirs({dir});

  wwiv::sdk::files::TicParser p(tic_path.parent_path());
  auto ot = p.parse("sample.tic");
  ASSERT_TRUE(ot);
  auto tic = ot.value();

  auto o = wwiv::sdk::files::FindFileAreaForTic(dirs, tic, net);
  ASSERT_TRUE(o.has_value());
  auto area = o.value();
  EXPECT_EQ(dir.name, "d1");
}

TEST(TicTest, FindFileAreaForTic_NotFound) {
  FileHelper helper;

  const std::string contents = R"(
Area AREANAME
Size 12
Crc AF083B2D
File sample.zip
)";
  const auto tic_path = helper.CreateTempFile("sample.tic", contents);
  const auto arc_path = wwiv::core::FilePath(helper.TempDir(), "sample.zip");
  {
    File af(arc_path);
    ASSERT_TRUE(af.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite));
    ASSERT_EQ(12, af.Write("hello world\n"));
    af.Close();
  }

  const auto data = helper.Dir("data");
  std::random_device rd{};
  wwiv::core::uuid_generator generator(rd);
  auto uuid = generator.generate();

  Network net{};
  net.name = "foo";
  net.uuid = uuid;

  wwiv::sdk::files::dir_area_t dt;
  dt.net_uuid = uuid;
  dt.area_tag = "2REANAME";
  wwiv::sdk::files::directory_t dir;
  dir.area_tags.emplace_back(dt);
  dir.name = "d1";

  wwiv::sdk::files::Dirs dirs(data, 0);
  dirs.set_dirs({dir});

  wwiv::sdk::files::TicParser p(tic_path.parent_path());
  auto ot = p.parse("sample.tic");
  ASSERT_TRUE(ot);
  auto tic = ot.value();

  auto o = wwiv::sdk::files::FindFileAreaForTic(dirs, tic, net);
  ASSERT_FALSE(o.has_value());
}
