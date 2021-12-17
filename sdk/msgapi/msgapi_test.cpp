/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2016-2021, WWIV Software Services             */
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
#include "core/file_helper.h"
#include "sdk/config.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/sdk_helper.h"
#include <memory>
#include <string>

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
    config = make_unique<Config>(helper.root());
    api.reset(new WWIVMessageApi(options, *config, {}, new NullLastReadImpl()));
  }

  static unique_ptr<Message> CreateMessage(MessageArea& area, uint16_t from, const string& fromname,
                                           const string& title, const string& text) {
    auto msg(area.CreateMessage());

    auto& h = msg->header();
    static uint32_t daten = 915192000;

    h.set_from_system(0);
    h.set_from_usernum(static_cast<uint16_t>(from));
    h.set_title(title);
    h.set_from(fromname);
    h.set_to("To");
    h.set_daten(daten++);
    h.set_in_reply_to("IRT");
    msg->text().set_text(text);

    return msg;
  }

  SdkHelper helper;
  unique_ptr<MessageApi> api;
  unique_ptr<Config> config;
};

TEST_F(MsgApiTest, CreateArea) {
  subboard_t sub{};
  sub.filename = "a1";
  ASSERT_TRUE(api->Create(sub, -1));
  unique_ptr<MessageArea> a1(api->Open(sub, -1));
  EXPECT_TRUE(a1->Close());

  EXPECT_TRUE(File::Exists(FilePath(helper.data(), "a1.sub")));
  EXPECT_TRUE(File::Exists(FilePath(helper.msgs(), "a1.dat")));
}

TEST_F(MsgApiTest, SmokeTest) {
  subboard_t sub{};
  sub.filename = "a1";
  {
    ASSERT_TRUE(api->Create(sub, -1));
    unique_ptr<MessageArea> area(api->Open(sub, -1));
    const auto msg(CreateMessage(*area, 1234, "From", "Title", "Line1\r\nLine2\r\n"));
    EXPECT_TRUE(area->AddMessage(*msg, {}));
  }

  unique_ptr<MessageArea> a2(api->Open(sub, -1));
  EXPECT_EQ(1, a2->number_of_messages());
  const auto m1 = a2->ReadMessage(1);
  EXPECT_EQ("From", m1->header().from());
}

TEST_F(MsgApiTest, ToName) {
  subboard_t sub{};
  sub.filename = "a1";
  {
    ASSERT_TRUE(api->Create(sub, -1));
    unique_ptr<MessageArea> area(api->Open(sub, -1));
    const auto msg(CreateMessage(*area, 1234, "From", "Title", "Line1\r\nLine2\r\n"));
    msg->header().set_to("Dude");
    EXPECT_TRUE(area->AddMessage(*msg, {}));
  }

  unique_ptr<MessageArea> a2(api->Open(sub, -1));
  const auto m1 = a2->ReadMessage(1);
  EXPECT_EQ("Dude", m1->header().to()) << "T:" << m1->text().text();
}

TEST_F(MsgApiTest, Resynch) {
  subboard_t sub{};
  sub.filename = "a1";
  ASSERT_TRUE(api->Create(sub, -1));
  unique_ptr<MessageArea> area(api->Open(sub, -1));
  {
    auto m(CreateMessage(*area, 1, "From1", "Title1", "Line1\r\nLine2\r\n"));
    EXPECT_TRUE(area->AddMessage(*m, {}));
    m->header().set_from_usernum(2);
    m->header().set_from("From2");
    m->header().set_title("Title2");
    EXPECT_TRUE(area->AddMessage(*m, {}));
    m->header().set_from_usernum(3);
    m->header().set_from("From3");
    m->header().set_title("Title3");
    EXPECT_TRUE(area->AddMessage(*m, {}));
  }

  // Re open the area, ensure that we read back the same first
  // two messages.
  unique_ptr<MessageArea> a2(api->Open(sub, -1));
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
    unique_ptr<MessageArea> area(api->Open(sub, -1));
    auto m(CreateMessage(*area, 1234, "From", "Title", "Line1\r\nLine2\r\n"));
    EXPECT_TRUE(area->AddMessage(*m, {}));
    m->header().set_from("From2");
    EXPECT_TRUE(area->AddMessage(*m, {}));
  }

  // Re open the area, ensure that we read back the same first
  // two messages.
  subboard_t sub{};
  sub.filename = "a1";
  unique_ptr<MessageArea> a2(api->Open(sub, -1));

  // Delete message #1, now we should just have 1 message.
  EXPECT_TRUE(a2->DeleteMessage(1));
  EXPECT_EQ(1, a2->number_of_messages());

  // ResyncMessage should move this to message 1 since
  // we deleted the old 1.
  auto msgnum = 2;
  a2->ResyncMessage(msgnum);
  EXPECT_EQ(1, msgnum);
}
