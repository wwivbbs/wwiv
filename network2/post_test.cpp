/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*                Copyright (C)2026, WWIV Software Services               */
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
/**************************************************************************/
#include "network2/post.h"

#include "gtest/gtest.h"
#include "core/datetime.h"
#include "core/fake_clock.h"
#include "core/file.h"
#include "net_core/netdat.h"
#include "network2/context.h"
#include "sdk/filenames.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/net/packets.h"
#include "sdk/net/subscribers.h"
#include "sdk/sdk_helper.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/vardec.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::net::network2;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::sdk::net;

namespace {
// Our node on the first network (CreateTestNetwork) and the second one.
constexpr uint16_t kOtherNetNode = 10;
// The system that sends us posts on the first network.
constexpr uint16_t kSender = 5;
} // namespace

class Network2PostTest : public testing::Test {
public:
  Network2PostTest() : clock_(DateTime::now()) {}

  void SetUp() override {
    // The SSM about posts waiting for validation goes to user #1.
    UserManager um(helper.config());
    User u{};
    User::CreateNewUserRecord(&u, 50, 20, 0, 0.1234f, {7, 11, 14, 13, 31, 10, 12, 9, 5, 3},
                              {7, 15, 15, 15, 112, 15, 15, 7, 7, 7});
    u.set_name("SYSOP");
    ASSERT_TRUE(um.writeuser(&u, 1));

    nets_.push_back(helper.CreateTestNetwork(network_type_t::wwivnet));
    nets_.push_back(
        Network(network_type_t::wwivnet, "OtherNET", helper.CreatePath("other"), kOtherNetNode));
    user_manager_ = std::make_unique<UserManager>(helper.config());
    netdat_ = std::make_unique<NetDat>(helper.gfilesdir(), helper.logdir(), nets_.front(), '2',
                                       clock_);
    context_ = std::make_unique<Context>(helper.config(), nets_.front(), *user_manager_, nets_,
                                         *netdat_);
    MessageApiOptions options;
    options.overflow_strategy = OverflowStrategy::delete_none;
    context_->set_api(2, std::make_unique<WWIVMessageApi>(options, helper.config(), nets_,
                                                          new NullLastReadImpl()));
  }

  // Adds the sub "TEST" on the first network, where host is its host (0 when
  // we host it).  gated_host, when set, also carries it on the second network.
  void AddSub(uint8_t anony, uint16_t host, std::optional<uint16_t> gated_host = std::nullopt) {
    sub_.filename = "test";
    sub_.name = "Test Sub";
    sub_.anony = anony;
    sub_.maxmsgs = 50;
    subboard_network_data_t sn{};
    sn.stype = "TEST";
    sn.net_num = 0;
    sn.host = host;
    sub_.nets.push_back(sn);
    if (gated_host) {
      subboard_network_data_t gn{};
      gn.stype = "TEST";
      gn.net_num = 1;
      gn.host = *gated_host;
      sub_.nets.push_back(gn);
    }
    ASSERT_TRUE(context_->subs.add(sub_));
  }

  static void WriteSubscribers(const Network& net, const std::set<uint16_t>& subscribers) {
    ASSERT_TRUE(WriteSubcriberFile(FilePath(net.dir, "nTEST.net"), subscribers));
  }

  // A post from kSender on the first network.
  static NetPacket Post(const std::string& title, const std::string& subtype = "TEST") {
    static daten_t daten = 915192000;
    ParsedNetPacketText ppt(main_type_new_post);
    ppt.set_subtype(subtype);
    ppt.set_title(title);
    ppt.set_sender("JOE #1 @5");
    ppt.set_date(daten);
    ppt.set_text("Hello\r\n");
    net_header_rec nh{};
    nh.tosys = 1;
    nh.touser = 0;
    nh.fromsys = kSender;
    nh.fromuser = 1;
    nh.main_type = main_type_new_post;
    nh.daten = daten++;
    return NetPacket(nh, {}, ppt);
  }

  // The packets in the named file(s) in a network's directory.
  static std::vector<NetPacket> ReadPackets(const Network& net, const std::string& prefix) {
    std::vector<NetPacket> result;
    for (const auto& e : std::filesystem::directory_iterator(net.dir)) {
      if (!e.path().filename().string().starts_with(prefix)) {
        continue;
      }
      File f(e.path());
      if (!f.Open(File::modeReadOnly | File::modeBinary)) {
        continue;
      }
      for (;;) {
        auto [packet, response] = read_packet(f, false);
        if (response != ReadNetPacketResponse::OK) {
          break;
        }
        result.push_back(packet);
      }
    }
    return result;
  }

  // Packets sent out on a network.
  [[nodiscard]] std::vector<NetPacket> sent(int net_num) const {
    return ReadPackets(nets_.at(net_num), "p1-");
  }

  [[nodiscard]] std::vector<NetPacket> deadnet() const {
    return ReadPackets(nets_.front(), DEAD_NET);
  }

  [[nodiscard]] int ssms_sent() const {
    const auto path = FilePath(helper.datadir(), SMW_DAT);
    if (!File::Exists(path)) {
      return 0;
    }
    return static_cast<int>(File(path).length() / sizeof(shortmsgrec));
  }

  [[nodiscard]] int posts() const {
    if (!context_->api(2).Exist(sub_)) {
      return 0;
    }
    auto area = context_->api(2).Open(sub_, -1);
    return area ? area->number_of_messages() : 0;
  }

  [[nodiscard]] bool pending(int num) const {
    auto area = context_->api(2).Open(sub_, -1);
    const auto h = area ? area->ReadMessageHeader(num) : std::nullopt;
    return h && h->pending_network();
  }

  SdkHelper helper;
  FakeClock clock_;
  std::vector<Network> nets_;
  std::unique_ptr<UserManager> user_manager_;
  std::unique_ptr<NetDat> netdat_;
  std::unique_ptr<Context> context_;
  subboard_t sub_{};
};

// On a sub we host, a post goes on to every other subscriber: not back to
// its sender, and not to us.
TEST_F(Network2PostTest, Hosted_SentToSubscribers) {
  AddSub(0, 0);
  WriteSubscribers(nets_.front(), {nets_.front().sysnum, kSender, 6, 7});
  auto p = Post("Title");
  ASSERT_TRUE(handle_new_post(*context_, p));

  EXPECT_EQ(1, posts());
  EXPECT_FALSE(pending(1));
  const auto out = sent(0);
  ASSERT_EQ(1u, out.size());
  EXPECT_EQ(kSender, out.front().nh.fromsys);
  EXPECT_EQ((std::vector<uint16_t>{6, 7}), out.front().list);
}

// A duplicate is discarded, and not sent on a second time.
TEST_F(Network2PostTest, Hosted_Duplicate_NotSentAgain) {
  AddSub(0, 0);
  WriteSubscribers(nets_.front(), {kSender, 6, 7});
  auto p = Post("Title");
  ASSERT_TRUE(handle_new_post(*context_, p));
  auto dupe = p;
  ASSERT_TRUE(handle_new_post(*context_, dupe));

  EXPECT_EQ(1, posts());
  EXPECT_EQ(1u, sent(0).size());
}

// A subscriber on one network has nobody to send a post on to.
TEST_F(Network2PostTest, Subscriber_NotSent) {
  AddSub(0, 2);
  auto p = Post("Title");
  ASSERT_TRUE(handle_new_post(*context_, p));

  EXPECT_EQ(1, posts());
  EXPECT_TRUE(sent(0).empty());
}

// On a sub we host that needs network validation, the post waits for the
// sysop, who is told once, not once for every post.
TEST_F(Network2PostTest, ValNet_Hosted_Held) {
  AddSub(anony_val_net, 0);
  WriteSubscribers(nets_.front(), {kSender, 6, 7});
  auto p1 = Post("One");
  ASSERT_TRUE(handle_new_post(*context_, p1));
  auto p2 = Post("Two");
  ASSERT_TRUE(handle_new_post(*context_, p2));

  EXPECT_EQ(2, posts());
  EXPECT_TRUE(pending(1));
  EXPECT_TRUE(pending(2));
  EXPECT_TRUE(sent(0).empty());
  EXPECT_EQ(1, ssms_sent());
}

// A duplicate of a held post isn't stored or SSMed again.
TEST_F(Network2PostTest, ValNet_Hosted_Duplicate) {
  AddSub(anony_val_net, 0);
  auto p = Post("Title");
  ASSERT_TRUE(handle_new_post(*context_, p));
  auto dupe = p;
  ASSERT_TRUE(handle_new_post(*context_, dupe));

  EXPECT_EQ(1, posts());
  EXPECT_TRUE(sent(0).empty());
  EXPECT_EQ(1, ssms_sent());
}

// As net38: a subscriber on one network sends nothing on, so there's nothing
// to hold.
TEST_F(Network2PostTest, ValNet_Subscriber_NotHeld) {
  AddSub(anony_val_net, 2);
  auto p = Post("Title");
  ASSERT_TRUE(handle_new_post(*context_, p));

  EXPECT_EQ(1, posts());
  EXPECT_FALSE(pending(1));
  EXPECT_EQ(0, ssms_sent());
}

// A sub gated to another network sends the post on there, from us.
TEST_F(Network2PostTest, Gated_SentOnOtherNetwork) {
  AddSub(0, 2, 0);
  WriteSubscribers(nets_.at(1), {20, 21});
  auto p = Post("Title");
  ASSERT_TRUE(handle_new_post(*context_, p));

  EXPECT_TRUE(sent(0).empty());
  const auto out = sent(1);
  ASSERT_EQ(1u, out.size());
  EXPECT_EQ(kOtherNetNode, out.front().nh.fromsys);
  EXPECT_EQ((std::vector<uint16_t>{20, 21}), out.front().list);
}

// As net38: on a gated sub that needs validation, the copy for the other
// network waits for the sysop too.
TEST_F(Network2PostTest, ValNet_Gated_Held) {
  AddSub(anony_val_net, 2, 0);
  WriteSubscribers(nets_.at(1), {20, 21});
  auto p = Post("Title");
  ASSERT_TRUE(handle_new_post(*context_, p));

  EXPECT_TRUE(pending(1));
  EXPECT_TRUE(sent(0).empty());
  EXPECT_TRUE(sent(1).empty());
  EXPECT_EQ(1, ssms_sent());
}

// A post for a sub we don't carry goes to dead.net once, and nowhere else.
TEST_F(Network2PostTest, UnknownSub_DeadNet) {
  AddSub(0, 0);
  WriteSubscribers(nets_.front(), {kSender, 6, 7});
  auto p = Post("Title", "NOSUCH");
  ASSERT_TRUE(handle_new_post(*context_, p));

  EXPECT_EQ(0, posts());
  EXPECT_EQ(1u, deadnet().size());
  EXPECT_TRUE(sent(0).empty());
}
