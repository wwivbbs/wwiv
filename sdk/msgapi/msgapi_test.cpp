/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2016-2022, WWIV Software Services             */
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

#include "core/file.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/net/packets.h"
#include "sdk/net/subscribers.h"
#include "sdk/sdk_helper.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/vardec.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace std;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::strings;

class MsgApiTest : public testing::Test {
public:
  void SetUp() override {
    MessageApiOptions options;
    options.overflow_strategy = OverflowStrategy::delete_none;
    api.reset(new WWIVMessageApi(options, helper.config(), {}, new NullLastReadImpl()));
  }

  static Message CreateMessage(MessageArea& area, uint16_t from, const string& fromname,
                               const string& title, const string& text) {
    auto msg = area.CreateMessage();

    auto& h = msg.header();
    static uint32_t daten = 915192000;

    h.set_from_system(0);
    h.set_from_usernum(static_cast<uint16_t>(from));
    h.set_title(title);
    h.set_from(fromname);
    h.set_to("To");
    h.set_daten(daten++);
    h.set_in_reply_to("IRT");
    msg.set_text(text);

    return msg;
  }

  SdkHelper helper;
  unique_ptr<MessageApi> api;
};

TEST_F(MsgApiTest, CreateArea) {
  subboard_t sub{};
  sub.filename = "a1";
  ASSERT_TRUE(api->Create(sub, -1));
  auto a1(api->Open(sub, -1));
  EXPECT_TRUE(a1->Close());

  EXPECT_TRUE(File::Exists(FilePath(helper.datadir(), "a1.sub")));
  EXPECT_TRUE(File::Exists(FilePath(helper.msgsdir(), "a1.dat")));
}

TEST_F(MsgApiTest, SmokeTest) {
  subboard_t sub{};
  sub.filename = "a1";
  {
    ASSERT_TRUE(api->Create(sub, -1));
    auto area(api->Open(sub, -1));
    auto msg(CreateMessage(*area, 1234, "From", "Title", "Line1\r\nLine2\r\n"));
    EXPECT_TRUE(area->AddMessage(msg, {}));
  }

  auto a2(api->Open(sub, -1));
  EXPECT_EQ(1, a2->number_of_messages());
  const auto m1 = a2->ReadMessage(1);
  EXPECT_EQ("From", m1->header().from());
}

TEST_F(MsgApiTest, ToName) {
  subboard_t sub{};
  sub.filename = "a1";
  {
    ASSERT_TRUE(api->Create(sub, -1));
    auto area(api->Open(sub, -1));
    auto msg(CreateMessage(*area, 1234, "From", "Title", "Line1\r\nLine2\r\n"));
    msg.header().set_to("Dude");
    EXPECT_TRUE(area->AddMessage(msg, {}));
  }

  auto a2(api->Open(sub, -1));
  const auto m1 = a2->ReadMessage(1);
  EXPECT_EQ("Dude", m1->header().to()) << "T:" << m1->text().string();
}

// AddMessage keeps the post's anonymous flags.
TEST_F(MsgApiTest, AddMessage_KeepsAnonymous) {
  subboard_t sub{};
  sub.filename = "a1";
  {
    ASSERT_TRUE(api->Create(sub, -1));
    auto area(api->Open(sub, -1));
    auto msg(CreateMessage(*area, 1234, "From", "Title", "Line1\r\n"));
    msg.header().set_anony(anony_sender);
    EXPECT_TRUE(area->AddMessage(msg, {}));
  }

  auto a2(api->Open(sub, -1));
  const auto h = a2->ReadMessageHeader(1);
  ASSERT_TRUE(h);
  EXPECT_EQ(anony_sender, h->anony());
}

TEST_F(MsgApiTest, Resynch) {
  subboard_t sub{};
  sub.filename = "a1";
  ASSERT_TRUE(api->Create(sub, -1));
  auto area(api->Open(sub, -1));
  {
    auto m(CreateMessage(*area, 1, "From1", "Title1", "Line1\r\nLine2\r\n"));
    EXPECT_TRUE(area->AddMessage(m, {}));
    m.header().set_from_usernum(2);
    m.header().set_from("From2");
    m.header().set_title("Title2");
    EXPECT_TRUE(area->AddMessage(m, {}));
    m.header().set_from_usernum(3);
    m.header().set_from("From3");
    m.header().set_title("Title3");
    EXPECT_TRUE(area->AddMessage(m, {}));
  }

  // Re open the area, ensure that we read back the same first
  // two messages.
  auto a2(api->Open(sub, -1));
  auto m1 = a2->ReadMessage(1);
  EXPECT_EQ("From1", m1->header().from());
  auto m2 = a2->ReadMessage(2);
  EXPECT_EQ("From2", m2->header().from());

  // Delete message #1, now we should just have 1 message.
  EXPECT_TRUE(a2->DeleteMessage(1));
  EXPECT_EQ(2, a2->number_of_messages());

  // ResyncMessage should move this to message 1 since
  // we deleted the old 1.
  auto msgnum = 2;
  a2->ResyncMessage(msgnum, *m2);
  EXPECT_EQ(1, msgnum);
}

TEST_F(MsgApiTest, Resynch_MessageNumber) {
  {
    subboard_t sub{};
    sub.filename = "a1";
    ASSERT_TRUE(api->Create(sub, -1));
    auto area(api->Open(sub, -1));
    auto m(CreateMessage(*area, 1234, "From", "Title", "Line1\r\nLine2\r\n"));
    EXPECT_TRUE(area->AddMessage(m, {}));
    m.header().set_from("From2");
    EXPECT_TRUE(area->AddMessage(m, {}));
  }

  // Re open the area, ensure that we read back the same first
  // two messages.
  subboard_t sub{};
  sub.filename = "a1";
  auto a2(api->Open(sub, -1));

  // Delete message #1, now we should just have 1 message.
  EXPECT_TRUE(a2->DeleteMessage(1));
  EXPECT_EQ(1, a2->number_of_messages());

  // ResyncMessage should move this to message 1 since
  // we deleted the old 1.
  auto msgnum = 2;
  a2->ResyncMessage(msgnum);
  EXPECT_EQ(1, msgnum);
}

// The text is kept as the BBS's reader shows it: blank lines and spaces are
// part of it (paragraphs, quotes, ASCII art). bbs/msgapi_legacy_compare_test
// checks this against the reader itself.
TEST_F(MsgApiTest, ReadMessage_KeepsBlankLinesAndIndentation) {
  subboard_t sub{};
  sub.filename = "a1";
  ASSERT_TRUE(api->Create(sub, -1));
  {
    auto area(api->Open(sub, -1));
    auto msg(CreateMessage(*area, 1, "From", "Title",
                           "Para one\r\n\r\n   indented  \r\n\r\n\r\nPara two\r\n\r\n\r\n"));
    EXPECT_TRUE(area->AddMessage(msg, {}));
  }
  auto a2(api->Open(sub, -1));
  const auto m = a2->ReadMessage(1);
  ASSERT_TRUE(m);
  EXPECT_EQ("Para one\r\n\r\n   indented  \r\n\r\n\r\nPara two\r\n\r\n\r\n", m->text().string());
  // The RE: and BY: lines that AddMessage added are still parsed out.
  EXPECT_EQ("To", m->header().to());
  EXPECT_EQ("Title", m->header().in_reply_to());
}

// Nothing after the control-Z that ends the text is read.
TEST_F(MsgApiTest, ReadMessage_StopsAtControlZ) {
  subboard_t sub{};
  sub.filename = "a1";
  ASSERT_TRUE(api->Create(sub, -1));
  {
    auto area(api->Open(sub, -1));
    auto msg(CreateMessage(*area, 1, "From", "Title", "Line1\r\nLine2\x1a\r\nJunk\r\n"));
    EXPECT_TRUE(area->AddMessage(msg, {}));
  }
  auto a2(api->Open(sub, -1));
  const auto m = a2->ReadMessage(1);
  ASSERT_TRUE(m);
  EXPECT_EQ("Line1\r\nLine2\r\n", m->text().string());
}

// AddMessage on subs carried on a network.
class MsgApiNetTest : public testing::Test {
public:
  void SetUp() override {
    // The SSM about posts waiting for validation goes to user #1.
    UserManager um(helper.config());
    User u{};
    User::CreateNewUserRecord(&u, 50, 20, 0, 0.1234f, {7, 11, 14, 13, 31, 10, 12, 9, 5, 3},
                              {7, 15, 15, 15, 112, 15, 15, 7, 7, 7});
    u.set_name("SYSOP");
    ASSERT_TRUE(um.writeuser(&u, 1));
  }

  // A sub on one network where we're a subscriber, so a post sent out on it
  // becomes one packet to the host.
  std::unique_ptr<MessageArea> CreateArea(net::network_type_t net_type, uint8_t anony,
                                          uint16_t host = 2) {
    net_ = helper.CreateTestNetwork(net_type);
    MessageApiOptions options;
    options.overflow_strategy = OverflowStrategy::delete_none;
    api_ = std::make_unique<WWIVMessageApi>(options, helper.config(),
                                            std::vector<net::Network>{net_},
                                            new NullLastReadImpl());
    sub_.filename = "a1";
    sub_.name = "Net Sub";
    sub_.anony = anony;
    subboard_network_data_t sn{};
    sn.stype = "TEST";
    sn.net_num = 0;
    sn.host = host;
    sub_.nets.push_back(sn);
    EXPECT_TRUE(api_->Create(sub_, -1));
    return api_->Open(sub_, -1);
  }

  static Message NewPost(MessageArea& area, const std::string& title, const std::string& to) {
    auto msg = area.CreateMessage();
    static uint32_t daten = 915192000;
    auto& h = msg.header();
    h.set_from_system(0);
    h.set_from_usernum(1);
    h.set_from("SYSOP #1");
    h.set_title(title);
    h.set_to(to);
    h.set_daten(daten++);
    msg.set_text("Hello\r\n");
    return msg;
  }

  // The packets written to the network's directory.
  [[nodiscard]] std::vector<net::NetPacket> packets() const {
    std::vector<net::NetPacket> result;
    for (const auto& e : std::filesystem::directory_iterator(net_.dir)) {
      if (!e.path().filename().string().starts_with("p1-")) {
        continue;
      }
      File f(e.path());
      if (!f.Open(File::modeReadOnly | File::modeBinary)) {
        continue;
      }
      for (;;) {
        auto [packet, response] = net::read_packet(f, false);
        if (response != net::ReadNetPacketResponse::OK) {
          break;
        }
        result.push_back(packet);
      }
    }
    return result;
  }

  // Packets written to the network's directory.
  [[nodiscard]] int packets_sent() const {
    int count = 0;
    for (const auto& e : std::filesystem::directory_iterator(net_.dir)) {
      if (e.path().filename().string().starts_with("p1-")) {
        ++count;
      }
    }
    return count;
  }

  [[nodiscard]] int ssms_sent() const {
    const auto path = FilePath(helper.datadir(), SMW_DAT);
    if (!File::Exists(path)) {
      return 0;
    }
    return static_cast<int>(File(path).length() / sizeof(shortmsgrec));
  }

  [[nodiscard]] bool pending(MessageArea& area, int num) const {
    const auto h = area.ReadMessageHeader(num);
    return h && h->pending_network();
  }

  SdkHelper helper;
  net::Network net_;
  subboard_t sub_{};
  std::unique_ptr<MessageApi> api_;
};

TEST_F(MsgApiNetTest, Send) {
  auto area = CreateArea(net::network_type_t::wwivnet, 0);
  MessageAreaOptions o{};
  o.send_post_to_network = true;
  auto m = NewPost(*area, "Title", "");
  ASSERT_TRUE(area->AddMessage(m, o));
  EXPECT_FALSE(pending(*area, 1));
  EXPECT_EQ(1, packets_sent());
  EXPECT_EQ(0, ssms_sent());
}

TEST_F(MsgApiNetTest, NotSentUnlessAsked) {
  auto area = CreateArea(net::network_type_t::wwivnet, 0);
  auto m = NewPost(*area, "Title", "");
  ASSERT_TRUE(area->AddMessage(m, {}));
  EXPECT_FALSE(pending(*area, 1));
  EXPECT_EQ(0, packets_sent());
}

// Posts from here on a sub that needs network validation wait for the sysop,
// who is told once, not once for every post.
TEST_F(MsgApiNetTest, ValNet_HeldPending_OneSsm) {
  auto area = CreateArea(net::network_type_t::wwivnet, anony_val_net);
  MessageAreaOptions o{};
  o.send_post_to_network = true;
  auto m1 = NewPost(*area, "One", "");
  ASSERT_TRUE(area->AddMessage(m1, o));
  auto m2 = NewPost(*area, "Two", "");
  ASSERT_TRUE(area->AddMessage(m2, o));
  EXPECT_TRUE(pending(*area, 1));
  EXPECT_TRUE(pending(*area, 2));
  EXPECT_EQ(0, packets_sent());
  EXPECT_EQ(1, ssms_sent());
}

// As the BBS does for a co-sysop's post.
TEST_F(MsgApiNetTest, ValNet_SkipValidation) {
  auto area = CreateArea(net::network_type_t::wwivnet, anony_val_net);
  MessageAreaOptions o{};
  o.send_post_to_network = true;
  o.skip_network_validation = true;
  auto m = NewPost(*area, "Title", "");
  ASSERT_TRUE(area->AddMessage(m, o));
  EXPECT_FALSE(pending(*area, 1));
  EXPECT_EQ(1, packets_sent());
  EXPECT_EQ(0, ssms_sent());
}

// A post from a network (network2) keeps the pending flag its caller gave it,
// and nothing is sent.  The sysop is told about the held one.
TEST_F(MsgApiNetTest, ValNet_FromNetwork) {
  auto area = CreateArea(net::network_type_t::wwivnet, anony_val_net);
  auto m1 = NewPost(*area, "Not held", "");
  m1.header().set_from_system(2);
  ASSERT_TRUE(area->AddMessage(m1, {}));
  auto m2 = NewPost(*area, "Held", "");
  m2.header().set_from_system(2);
  m2.header().set_pending_network(true);
  ASSERT_TRUE(area->AddMessage(m2, {}));
  EXPECT_FALSE(pending(*area, 1));
  EXPECT_TRUE(pending(*area, 2));
  EXPECT_EQ(0, packets_sent());
  EXPECT_EQ(1, ssms_sent());
}

// Only the first post from a network waiting for validation is SSMed.
TEST_F(MsgApiNetTest, ValNet_FromNetwork_OneSsm) {
  auto area = CreateArea(net::network_type_t::wwivnet, anony_val_net, 0);
  for (const auto* title : {"One", "Two"}) {
    auto m = NewPost(*area, title, "");
    m.header().set_from_system(2);
    m.header().set_pending_network(true);
    ASSERT_TRUE(area->AddMessage(m, {}));
  }
  EXPECT_EQ(0, packets_sent());
  EXPECT_EQ(1, ssms_sent());
}

// A post that couldn't be saved here isn't sent out.
TEST_F(MsgApiNetTest, SaveFails_NotSent) {
  auto area = CreateArea(net::network_type_t::wwivnet, 0);
  // With no header record, the post can't be added to the sub.
  const auto sub_fn = FilePath(helper.datadir(), "a1.sub");
  ASSERT_TRUE(File::Exists(sub_fn));
  std::filesystem::resize_file(sub_fn, 0);
  MessageAreaOptions o{};
  o.send_post_to_network = true;
  auto m = NewPost(*area, "Title", "");
  EXPECT_FALSE(area->AddMessage(m, o));
  EXPECT_EQ(0, packets_sent());
}

// Nor is the sysop told about a held post that couldn't be saved.
TEST_F(MsgApiNetTest, ValNet_SaveFails_NoSsm) {
  auto area = CreateArea(net::network_type_t::wwivnet, anony_val_net);
  const auto sub_fn = FilePath(helper.datadir(), "a1.sub");
  ASSERT_TRUE(File::Exists(sub_fn));
  std::filesystem::resize_file(sub_fn, 0);
  MessageAreaOptions o{};
  o.send_post_to_network = true;
  auto m = NewPost(*area, "Title", "");
  EXPECT_FALSE(area->AddMessage(m, o));
  EXPECT_EQ(0, ssms_sent());
}

// valscan only looks on subs that need validation, so a post marked pending
// on any other sub is sent anyway, and not left pending.
TEST_F(MsgApiNetTest, PendingFromCaller_NotValNet_Sent) {
  auto area = CreateArea(net::network_type_t::wwivnet, 0);
  MessageAreaOptions o{};
  o.send_post_to_network = true;
  auto m = NewPost(*area, "Title", "");
  m.header().set_pending_network(true);
  ASSERT_TRUE(area->AddMessage(m, o));
  EXPECT_FALSE(pending(*area, 1));
  EXPECT_EQ(1, packets_sent());
  EXPECT_EQ(0, ssms_sent());
}

// On a sub we host, the post goes to every subscriber but us, even when we're
// in our own subscriber list.
TEST_F(MsgApiNetTest, Hosted_NotSentToSelf) {
  auto area = CreateArea(net::network_type_t::wwivnet, 0, 0);
  ASSERT_TRUE(WriteSubcriberFile(FilePath(net_.dir, "nTEST.net"), {net_.sysnum, 5, 6}));
  MessageAreaOptions o{};
  o.send_post_to_network = true;
  auto m = NewPost(*area, "Title", "");
  ASSERT_TRUE(area->AddMessage(m, o));
  const auto sent = packets();
  ASSERT_EQ(1u, sent.size());
  EXPECT_EQ(net_.sysnum, sent.front().nh.fromsys);
  EXPECT_EQ((std::vector<uint16_t>{5, 6}), sent.front().list);
}

// On an FTN sub the To: name goes in a ^D0FidoAddr line, except for All in
// any case.
TEST_F(MsgApiNetTest, Ftn_FidoAddr) {
  auto area = CreateArea(net::network_type_t::ftn, 0);
  auto m1 = NewPost(*area, "One", "Rushfan");
  ASSERT_TRUE(area->AddMessage(m1, {}));
  auto m2 = NewPost(*area, "Two", "all");
  ASSERT_TRUE(area->AddMessage(m2, {}));

  const auto r1 = area->ReadMessage(1);
  ASSERT_TRUE(r1);
  EXPECT_NE(std::string::npos, r1->text().string().find("\x04" "0FidoAddr: Rushfan\r\n"));
  const auto r2 = area->ReadMessage(2);
  ASSERT_TRUE(r2);
  EXPECT_EQ(std::string::npos, r2->text().string().find("FidoAddr"));
}
