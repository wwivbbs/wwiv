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
#include "bbs/bbs_helper.h"
#include "bbs/message_file.h"
#include "bbs/msgbase1.h"
#include "common/message_editor_data.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/fido/fido_address.h"
#include "sdk/net/networks.h"
#include "sdk/net/packets.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"
#include <filesystem>

#include "gtest/gtest.h"

using namespace std::string_literals;
using namespace wwiv::common;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::sdk::fido;
using namespace wwiv::core;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;

TEST(Msgbase1Test, Smoke) {}

TEST(Msgbase1Test, AddFtnMsgid_Smoke) {
  BbsHelper helper;
  helper.SetUp();

  MessageEditorData data("Random Username");
  const std::string msgid = "1:2/3 deadbeef";
  FidoAddress addr("1:2/3");
  data.text = "Hello World\r\n\004"
              "0MSGID 1:2/3 deadbeef\r\nHello World2\r\n";
  add_ftn_msgid(*a()->config(), addr, msgid, &data);
  
  // One one gone
  ASSERT_EQ(std::string::npos, data.text.find("MSGID 1:2/3 deadbeef"));
  // Reply line added right.
  ASSERT_NE(std::string::npos, data.text.find("REPLY: 1:2/3 deadbeef\r\n"));
}

// send_net_post: the packets a post from this BBS becomes on the sub's
// networks.
class SendNetPostTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    ASSERT_TRUE(helper.files().Mkdir("msgs"));
    a()->init_status_and_instances_for_test();
    AddNetwork("NetA", 1);
    AddNetwork("NetB", 10);
    sub_.filename = "s1";
    sub_.name = "Net Sub";
  }

  void AddNetwork(const std::string& name, uint16_t sysnum) {
    const auto dir = helper.files().CreateTempFilePath(name + "/x").parent_path();
    std::filesystem::create_directories(dir);
    const auto num = a()->nets().size();
    // The BBS appends file names to a network's directory, which ends in a
    // separator, as it does in WWIV's network config.
    const std::filesystem::path dir_with_slash = StrCat(dir.string(), File::pathSeparatorChar);
    ASSERT_TRUE(a()->nets().insert(static_cast<int>(num),
                                   Network(network_type_t::wwivnet, name, dir_with_slash, sysnum)));
  }

  void AddSubNet(int net_num, const std::string& stype, uint16_t host) {
    subboard_network_data_t sn{};
    sn.stype = stype;
    sn.net_num = static_cast<int16_t>(net_num);
    sn.host = host;
    sub_.nets.push_back(sn);
  }

  // A post by user #1 here, saved as the BBS's post() saves it.
  postrec NewPost() {
    postrec p{};
    to_char_array(p.title, "Hello");
    p.ownersys = 0;
    p.owneruser = 1;
    p.daten = 915192000;
    p.msg.storage_type = 2;
    savefile(std::string(kText), &p.msg, sub_.filename);
    return p;
  }

  // The packets written to network net_num's outbound file.
  static std::vector<NetPacket> packets(int net_num) {
    std::vector<NetPacket> result;
    File f(FilePath(a()->nets()[net_num].dir, StrCat("p1", a()->network_extension())));
    if (!f.Open(File::modeReadOnly | File::modeBinary)) {
      return result;
    }
    for (;;) {
      auto [packet, response] = read_packet(f, false);
      if (response != ReadNetPacketResponse::OK) {
        break;
      }
      result.push_back(packet);
    }
    return result;
  }

  static constexpr char kText[] = "SYSOP #1\r\nFri Jan 01 12:00:00 1999\r\nBody\r\n";
  BbsHelper helper;
  subboard_t sub_{};
};

TEST_F(SendNetPostTest, OneNetwork_ToHost) {
  AddSubNet(0, "TA", 2);
  auto p = NewPost();
  send_net_post(&p, sub_);
  const auto a_packets = packets(0);
  ASSERT_EQ(1u, a_packets.size());
  const auto& pk = a_packets.front();
  EXPECT_EQ(main_type_new_post, pk.nh.main_type);
  EXPECT_EQ(2, pk.nh.tosys);
  EXPECT_EQ(1, pk.nh.fromsys);
  EXPECT_EQ(1, pk.nh.fromuser);
  EXPECT_EQ(0, pk.nh.list_len);
  EXPECT_EQ("TA\0Hello\0"s + kText, pk.text());
}

// The post goes out on every network; on the second the BBS gates it, and
// it comes from this BBS's node there.
TEST_F(SendNetPostTest, TwoNetworks_SentOnBoth) {
  AddSubNet(0, "TA", 2);
  AddSubNet(1, "TB", 20);
  auto p = NewPost();
  send_net_post(&p, sub_);
  const auto a_packets = packets(0);
  ASSERT_EQ(1u, a_packets.size());
  EXPECT_EQ(2, a_packets.front().nh.tosys);
  EXPECT_EQ("TA\0Hello\0"s + kText, a_packets.front().text());

  const auto b_packets = packets(1);
  ASSERT_EQ(1u, b_packets.size());
  const auto& pk = b_packets.front();
  EXPECT_EQ(main_type_new_post, pk.nh.main_type);
  EXPECT_EQ(20, pk.nh.tosys);
  EXPECT_EQ(10, pk.nh.fromsys);
  EXPECT_EQ(1, pk.nh.fromuser);
  // The name line keeps its line break.
  EXPECT_EQ("TB\0Hello\0"s + kText, pk.text());
}
