/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2016-2017, WWIV Software Services             */
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

#include "core/file.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "sdk/config.h"
#include "sdk/networks.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk_test/sdk_helper.h"

using namespace std;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::strings;

class MsgApiTest: public testing::Test {
public:
  void SetUp() override {
    wwiv::sdk::msgapi::MessageApiOptions options;
    options.overflow_strategy = wwiv::sdk::msgapi::OverflowStrategy::delete_none;
    config = make_unique<Config>(helper.root());
    api.reset(new WWIVMessageApi(options, *config, {}, new NullLastReadImpl()));
  }

  unique_ptr<Message> CreateMessage(MessageArea& area, uint16_t from, const string& fromname, const string& title, const string& text) {
    unique_ptr<Message> msg(area.CreateMessage());

    auto* h = msg->header();
    static uint32_t daten = 915192000;

    h->set_from_system(0);
    h->set_from_usernum(static_cast<uint16_t>(from));
    h->set_title(title);
    h->set_from(fromname);
    h->set_to("To");
    h->set_daten(daten++);
    h->set_in_reply_to("IRT");
    msg->text()->set_text(text);
    
    return msg;
  }

  SdkHelper helper;
  unique_ptr<MessageApi> api;
  unique_ptr<Config> config;
};

TEST_F(MsgApiTest, CreateArea) {
  unique_ptr<MessageArea> a1(api->Create("a1", -1));
  EXPECT_TRUE(a1->Close());

  EXPECT_TRUE(File::Exists(wwiv::core::FilePath(helper.data(), "a1.sub")));
  EXPECT_TRUE(File::Exists(wwiv::core::FilePath(helper.msgs(), "a1.dat")));
}

TEST_F(MsgApiTest, SmokeTest) {
  {
    unique_ptr<MessageArea> area(api->Create("a1", -1));
    unique_ptr<Message> msg(CreateMessage(*area, 1234, "From", "Title", "Line1\r\nLine2\r\n"));
    EXPECT_TRUE(area->AddMessage(*msg));
  }

  unique_ptr<MessageArea> a2(api->Open("a1", -1));
  EXPECT_EQ(1, a2->number_of_messages());
  unique_ptr<Message> m1(a2->ReadMessage(1));
  EXPECT_EQ("From", m1->header()->from());
}

TEST_F(MsgApiTest, Resynch) {
  unique_ptr<MessageArea> area(api->Create("a1", -1));
  {
    unique_ptr<Message> m(CreateMessage(*area, 1234, "From", "Title", "Line1\r\nLine2\r\n"));
    EXPECT_TRUE(area->AddMessage(*m));
    m->header()->set_from("From2");
    EXPECT_TRUE(area->AddMessage(*m));
    m->header()->set_from("From3");
    EXPECT_TRUE(area->AddMessage(*m));
  }
  
  // Re open the area, ensure that we read back the same first
  // two messages.
  unique_ptr<MessageArea> a2(api->Open("a1", -1));
  unique_ptr<Message> m1(a2->ReadMessage(1));
  EXPECT_EQ("From", m1->header()->from());
  unique_ptr<Message> m2(a2->ReadMessage(2));
  EXPECT_EQ("From2", m2->header()->from());

  // Delete message #1, now we should just have 1 message.
  EXPECT_TRUE(a2->DeleteMessage(1));
  EXPECT_EQ(2, a2->number_of_messages());

  // ResyncMessage should move this to message 1 since
  // we deleted the old 1.
  int msgnum = 2;
  a2->ResyncMessage(msgnum, *m2);
  EXPECT_EQ(1, msgnum);
}

TEST_F(MsgApiTest, Resynch_MessageNumber) {
  {
    unique_ptr<MessageArea> area(api->Create("a1", -1));
    unique_ptr<Message> m(CreateMessage(*area, 1234, "From", "Title", "Line1\r\nLine2\r\n"));
    EXPECT_TRUE(area->AddMessage(*m));
    m->header()->set_from("From2");
    EXPECT_TRUE(area->AddMessage(*m));
  }

  // Re open the area, ensure that we read back the same first
  // two messages.
  unique_ptr<MessageArea> a2(api->Open("a1", -1));

  // Delete message #1, now we should just have 1 message.
  EXPECT_TRUE(a2->DeleteMessage(1));
  EXPECT_EQ(1, a2->number_of_messages());

  // ResyncMessage should move this to message 1 since
  // we deleted the old 1.
  int msgnum = 2;
  a2->ResyncMessage(msgnum);
  EXPECT_EQ(1, msgnum);
}
