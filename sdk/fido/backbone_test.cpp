/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2022, WWIV Software Services                  */
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
#include "gmock/gmock.h"

#include "core/inifile.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/test/file_helper.h"
#include "sdk/net/net.h"
#include "sdk/fido/backbone.h"
#include <type_traits>
#include <string>

using namespace testing;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::sdk::fido;


class BackboneTest : public testing::Test {
public:
  BackboneTest() {
    networks.emplace_back(network_type_t::wwivnet, "WWIVnet"); // 0

    net.dir = helper_.TempDir().string();
    net.type = network_type_t::ftn;
    net.fido.fido_address = "11:1/211";
    networks.push_back(net); // 1

    backbone_file = helper_.CreateTempFile("backbone.na", R"(
FOO           This is the foo echo
BAR           This is the bar echo

)");

    ini_file = helper_.CreateTempFile("import.ini", R"(
[backbone]
post_acs = user.sl >= 21
read_acs = user.sl >= 12
maxmsgs = 512
net_num = 1
uplink = 11:1/1
)");

    subs = std::make_unique<Subs>(helper_.Dir("data"), networks);

    inif = std::make_unique<IniFile>(ini_file, "backbone");
  }

protected:

  void DumpSubs() {
    for (const auto& sub : subs->subs()) {
      LOG(INFO) << "Sub fn: " << sub.filename << "; name: " << sub.name
                << "; net.stype: " << sub.nets.front().stype;
    }
  }
  wwiv::core::test::FileHelper helper_;
  Network net{};
  std::vector<Network> networks;
  std::unique_ptr<Subs> subs;

  std::filesystem::path ini_file{};
  std::filesystem::path backbone_file{};
  std::unique_ptr<IniFile> inif;
};

TEST_F(BackboneTest, ParseBackboneFile) {
  auto backbone_entries = ParseBackboneFile(backbone_file);
  ASSERT_THAT(backbone_entries.size(), Eq(2));

  EXPECT_THAT(backbone_entries.front().tag, Eq("FOO"));
  EXPECT_THAT(backbone_entries.front().desc, Eq("This is the foo echo"));
}

TEST_F(BackboneTest, TwoNew) {
  ASSERT_THAT(subs->size(), Eq(0));
  ASSERT_TRUE(inif->IsOpen());
  auto backbone_entries = ParseBackboneFile(backbone_file);

  // Ensure the import operation claims to have completed successfully.
  auto r = ImportSubsFromBackbone(*subs, net, 1, *inif, backbone_entries);
  ASSERT_TRUE(r.success);
  ASSERT_TRUE(r.subs_dirty);
  EXPECT_THAT(subs->size(), Eq(2));
  DumpSubs();

  // Ensure that Subs now contains the newly added subs.
  auto sit = subs->subs().cbegin();
  auto bit = backbone_entries.cbegin();
  auto send = std::end(subs->subs());
  auto bend = std::end(backbone_entries);
  while (sit != send) { 
    const auto& sub = *sit++;
    const auto& bbe = *bit++;
    EXPECT_THAT(sub.filename, StrCaseEq(bbe.tag));
    EXPECT_THAT(sub.read_acs, StrEq("user.sl >= 12"));
    EXPECT_THAT(sub.post_acs, StrEq("user.sl >= 21"));
    EXPECT_THAT(sub.maxmsgs, Eq(512));
    EXPECT_THAT(sub.name, StrCaseEq(bbe.desc));
    EXPECT_THAT(sub.nets.front().stype, StrCaseEq(bbe.tag));
  }
  EXPECT_EQ(bit, bend);
  EXPECT_EQ(sit, send);

}

/** By default the INI file for the test class contained no conferences */
TEST_F(BackboneTest, NoConf) {
  ASSERT_THAT(subs->size(), Eq(0));
  ASSERT_TRUE(inif->IsOpen());
  auto backbone_entries = ParseBackboneFile(backbone_file);

  // Ensure the import operation claims to have completed successfully.
  auto r = ImportSubsFromBackbone(*subs, net, 1, *inif, backbone_entries);
  ASSERT_TRUE(r.success);
  ASSERT_TRUE(r.subs_dirty);
  EXPECT_THAT(subs->size(), Eq(2));

  // Ensure that Subs now contains the newly added subs.
  auto sit = subs->subs().front();
  EXPECT_TRUE(sit.conf.empty());
}

TEST_F(BackboneTest, WithConf) {
  ASSERT_THAT(subs->size(), Eq(0));
  auto backbone_entries = ParseBackboneFile(backbone_file);
  // Create new inifile with conferences.
  ini_file = helper_.CreateTempFile("import.ini", R"(
[backbone]
post_acs = user.sl >= 21
read_acs = user.sl >= 12
maxmsgs = 512
net_num = 1
uplink = 11:1/1
conf = AwF
)");
  // Use new inif that has conferences in it.
  inif = std::make_unique<IniFile>(ini_file, "backbone");
  ASSERT_TRUE(inif->IsOpen());

  // Ensure the import operation claims to have completed successfully.
  auto r = ImportSubsFromBackbone(*subs, net, 1, *inif, backbone_entries);
  ASSERT_TRUE(r.success);
  ASSERT_TRUE(r.subs_dirty);
  EXPECT_THAT(subs->size(), Eq(2));

  // Ensure that Subs now contains the newly added subs.
  auto sit = subs->subs().front();
  EXPECT_FALSE(sit.conf.empty());
  // Note the conf list is sorted and has the correct case.
  EXPECT_THAT(sit.conf.to_string(), Eq("AFW"));
}

TEST_F(BackboneTest, OneAlreadyExists) {
  ASSERT_THAT(subs->size(), Eq(0));
  subboard_t newsub{};
  newsub.filename = "BAR";
  newsub.name = "BAR";
  newsub.nets.emplace_back(subboard_network_data_t{"BAR", 0, 1, 0, 0, {}});
  subs->insert(0, newsub);
  ASSERT_TRUE(inif->IsOpen());
  auto backbone_entries = ParseBackboneFile(backbone_file);

  // Ensure the import operation claims to have completed successfully.
  auto r = ImportSubsFromBackbone(*subs, net, 1, *inif, backbone_entries);
  ASSERT_TRUE(r.success);
  ASSERT_TRUE(r.subs_dirty);
  EXPECT_THAT(subs->size(), Eq(2));
  DumpSubs();

  // Ensure that Subs now contains the newly added subs.
  auto sit = subs->subs().cbegin();
  auto send = std::end(subs->subs());
  EXPECT_THAT((*sit++).filename, StrCaseEq("BAR"));
  EXPECT_THAT((*sit++).filename, StrCaseEq("FOO"));
  EXPECT_THAT(sit, Eq(send));
}



TEST_F(BackboneTest, TwoAlreadyExists) {
  ASSERT_THAT(subs->size(), Eq(0));
  {
    subboard_t newsub{};
    newsub.filename = "FOO";
    newsub.name = "FOO";
    newsub.nets.emplace_back(subboard_network_data_t{"FOO", 0, 1, 0, 0, {}});
    subs->insert(0, newsub);
  }
  {
    subboard_t newsub{};
    newsub.filename = "BAR";
    newsub.name = "BAR";
    newsub.nets.emplace_back(subboard_network_data_t{"BAR", 0, 1, 0, 0, {}});
    subs->insert(0, newsub);
  }
  ASSERT_TRUE(inif->IsOpen());
  auto backbone_entries = ParseBackboneFile(backbone_file);

  // Ensure the import operation claims to have completed successfully.
  auto r = ImportSubsFromBackbone(*subs, net, 1, *inif, backbone_entries);
  ASSERT_TRUE(r.success);
  // Nothing added, therefore dirty == false
  ASSERT_FALSE(r.subs_dirty);
  EXPECT_THAT(subs->size(), Eq(2));
  DumpSubs();

  // Ensure that Subs now contains the no newly added subs and has the short
  // name specified in the test code.
  auto sit = subs->subs().cbegin();
  auto send = std::end(subs->subs());
  EXPECT_THAT((*sit).filename, StrCaseEq("BAR"));
  EXPECT_THAT((*sit++).name, StrCaseEq("BAR"));
  EXPECT_THAT((*sit).filename, StrCaseEq("FOO"));
  EXPECT_THAT((*sit++).name, StrCaseEq("FOO"));
  EXPECT_THAT(sit, Eq(send));
}
