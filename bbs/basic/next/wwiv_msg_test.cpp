/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2026, WWIV Software Services                  */
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
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "bbs/basic/next/bbs_message_base.h"
#include "bbs/basic/next/message_base.h"
#include "bbs/basic/next/next_engine.h"
#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/bbs_helper.h"
#include "common/input.h"
#include "common/output.h"
#include "core/datafile.h"
#include "core/file.h"
#include "sdk/config.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace wwiv::bbs::basic;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using testing::ElementsAre;
using testing::HasSubstr;
using testing::Not;

namespace {

// Real message areas, with the user's rights set by the test.
class FakeMessageBase final : public MessageBase {
public:
  FakeMessageBase(Subs& subs, MessageApi& api) : subs_(subs), api_(api) {}

  [[nodiscard]] std::vector<UserSub> user_subs() const override { return user_subs_; }
  [[nodiscard]] std::optional<int> current_user_sub() const override { return current_; }
  [[nodiscard]] const Subs& subs() const override { return subs_; }
  [[nodiscard]] std::unique_ptr<MessageArea> open(int subnum) override {
    const auto& sub = subs_.sub(subnum);
    if (!api_.Exist(sub)) {
      return nullptr;
    }
    return api_.Open(sub, subnum);
  }
  [[nodiscard]] bool is_cosysop(int) const override { return cosysop_; }
  [[nodiscard]] bool can_read_anonymous(int) const override { return read_anonymous_; }

  std::vector<UserSub> user_subs_;
  std::optional<int> current_;
  bool cosysop_{false};
  bool read_anonymous_{false};

private:
  Subs& subs_;
  MessageApi& api_;
};

} // namespace

class WwivMsgTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    helper.config().scripting_enabled(true);
    ASSERT_TRUE(helper.files().Mkdir("msgs"));
    a()->init_status_and_instances_for_test();

    MessageApiOptions options;
    options.overflow_strategy = OverflowStrategy::delete_none;
    api = std::make_unique<WWIVMessageApi>(options, helper.config(),
                                           std::vector<net::Network>{}, new NullLastReadImpl());
    subs = std::make_unique<Subs>(helper.config().datadir(), std::vector<net::Network>{});
    AddSub("General", "general", "General chat");
    AddSub("Sysop", "sysop", "For the sysop");
    // Not one of the user's subs.
    AddSub("Secret", "secret", "Nobody can read this");

    msgs = std::make_unique<FakeMessageBase>(*subs, *api);
    msgs->user_subs_ = {{0, "1"}, {1, "S"}};
  }

  void AddSub(const std::string& name, const std::string& filename, const std::string& desc) {
    subboard_t sub{};
    sub.name = name;
    sub.filename = filename;
    sub.desc = desc;
    sub.storage_type = 2;
    const auto n = subs->size();
    ASSERT_TRUE(subs->add(sub));
    ASSERT_TRUE(api->Create(sub, n));
  }

  // Posts a message to sub subnum and returns its number.
  int Post(int subnum, const std::string& from, const std::string& title,
           const std::string& text, uint8_t status = 0) {
    auto area = api->Open(subs->sub(subnum), subnum);
    auto msg = area->CreateMessage();
    auto& h = msg.header();
    h.set_from(from);
    h.set_from_usernum(1);
    h.set_title(title);
    h.set_daten(kDaten);
    h.set_status(status);
    msg.set_text(text);
    MessageAreaOptions o{};
    o.add_re_and_by_line = false;
    EXPECT_TRUE(area->AddMessage(msg, o));
    return area->number_of_messages();
  }

  // AddMessage doesn't keep a post's anonymous flags, so set them in the sub
  // file afterwards.
  void SetAnony(int subnum, int number, uint8_t anony) {
    const auto path = FilePath(helper.config().datadir(), subs->sub(subnum).filename + ".sub");
    DataFile<postrec> f(path, File::modeBinary | File::modeReadWrite);
    ASSERT_TRUE(f);
    postrec p{};
    ASSERT_TRUE(f.Read(number, &p));
    p.anony = anony;
    ASSERT_TRUE(f.Write(number, &p));
  }

  std::string Run(const std::string& text, bool* ok = nullptr) {
    NextEngine engine(bin, bout, helper.config(), &helper.context());
    engine.set_message_base(std::move(msgs));
    const auto r = engine.RunScript("msgtest", text);
    if (ok) {
      *ok = r;
    }
    return helper.io()->captured();
  }

  static constexpr daten_t kDaten = 1700000000;
  BbsHelper helper{};
  std::unique_ptr<WWIVMessageApi> api;
  std::unique_ptr<Subs> subs;
  std::unique_ptr<FakeMessageBase> msgs;
};

TEST_F(WwivMsgTest, ModuleName) {
  EXPECT_EQ("wwiv.msg\r\n", Run("print wwiv.msg.module_name()\n"));
}

TEST_F(WwivMsgTest, Disabled) {
  helper.config().script_package_msg_enabled(false);
  bool ok{true};
  const auto out = Run("print wwiv.msg.module_name()\nprint \"after\"\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, Not(HasSubstr("after")));
}

TEST_F(WwivMsgTest, EnabledByDefault) {
  EXPECT_TRUE(helper.config().script_package_msg_enabled());
}

TEST_F(WwivMsgTest, Areas) {
  Post(0, "rushfan #1", "Hello", "Hi\r\n");
  Post(0, "rushfan #1", "Again", "Hi\r\n");
  EXPECT_EQ("1/General/General chat/2\r\n"
            "S/Sysop/For the sysop/0\r\n",
            Run(R"(
for a in wwiv.msg.areas()
  print a.key; "/"; a.name; "/"; a.description; "/"; a.count
next
)"));
}

TEST_F(WwivMsgTest, Area_ByKey) {
  EXPECT_EQ("Sysop\r\n", Run("print wwiv.msg.area(\"s\").name\n"));
}

TEST_F(WwivMsgTest, Area_NotTheUsers) {
  bool ok{true};
  const auto out = Run("a = wwiv.msg.area(\"3\")\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("AREA: there is no message area '3'"));
}

TEST_F(WwivMsgTest, CurrentArea) {
  msgs->current_ = 1;
  EXPECT_EQ("Sysop\r\n", Run("print wwiv.msg.current_area().name\n"));
}

TEST_F(WwivMsgTest, CurrentArea_None) {
  bool ok{true};
  const auto out = Run("a = wwiv.msg.current_area()\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("CURRENT_AREA: the user isn't in a message area"));
}

TEST_F(WwivMsgTest, Headers) {
  Post(0, "rushfan #1", "Hello", "Hi\r\n");
  Post(0, "sysop #2", "Re: Hello", "Hi back\r\n");
  EXPECT_EQ("1/Hello/rushfan #1/All/1700000000/FALSE\r\n"
            "2/Re: Hello/sysop #2/All/1700000000/FALSE\r\n",
            Run(R"(
for h in wwiv.msg.area("1").headers()
  print h.number; "/"; h.title; "/"; h.from; "/"; h.to; "/"; h.date; "/"; h.anonymous
next
)"));
}

TEST_F(WwivMsgTest, Headers_SkipsUnvalidatedAndDeleted) {
  Post(0, "a #1", "One", "1\r\n");
  Post(0, "a #1", "Unvalidated", "2\r\n", status_unvalidated);
  Post(0, "a #1", "Deleted", "3\r\n", status_delete);
  Post(0, "a #1", "Four", "4\r\n");
  const std::string script = R"(
for h in wwiv.msg.area("1").headers()
  print h.number; " "; h.title
next
)";
  EXPECT_EQ("1 One\r\n4 Four\r\n", Run(script));
}

TEST_F(WwivMsgTest, Headers_CoSysopSeesUnvalidatedAndDeleted) {
  Post(0, "a #1", "One", "1\r\n");
  Post(0, "a #1", "Unvalidated", "2\r\n", status_unvalidated);
  Post(0, "a #1", "Deleted", "3\r\n", status_delete);
  msgs->cosysop_ = true;
  EXPECT_EQ("1 One\r\n2 Unvalidated\r\n3 Deleted\r\n", Run(R"(
for h in wwiv.msg.area("1").headers()
  print h.number; " "; h.title
next
)"));
}

TEST_F(WwivMsgTest, Message) {
  Post(0, "rushfan #1", "Hello", "Line one\r\n\x04" "0PID: test\r\nLine two\r\n");
  EXPECT_EQ("Hello/rushfan #1\r\n"
            "Line one\r\nLine two\r\n"
            "2\r\n",
            Run(R"(
m = wwiv.msg.area("1").message(1)
print m.title; "/"; m.from
' TEXT ends with a newline, as each line does.
print m.text;
print len(m.lines())
)"));
}

TEST_F(WwivMsgTest, Message_OutOfRange) {
  Post(0, "a #1", "One", "1\r\n");
  bool ok{true};
  const auto out = Run("m = wwiv.msg.area(\"1\").message(2)\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("MESSAGE: there is no message 2 in 'General' (it has 1)"));
}

TEST_F(WwivMsgTest, Message_Unvalidated) {
  Post(0, "a #1", "Unvalidated", "secret\r\n", status_unvalidated);
  bool ok{true};
  const auto out = Run("m = wwiv.msg.area(\"1\").message(1)\nprint m.text\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("MESSAGE: message 1 in 'General' can't be read"));
  EXPECT_THAT(out, Not(HasSubstr("secret")));
}

TEST_F(WwivMsgTest, Anonymous_Hidden) {
  Post(0, "rushfan #1", "Who", "text\r\n");
  SetAnony(0, 1, anony_sender);
  const auto out = Run(R"(
h = wwiv.msg.area("1").headers()
print get(h, 0).from; "/"; get(h, 0).anonymous
print wwiv.msg.area("1").message(1).from
)");
  EXPECT_EQ(">UNKNOWN</TRUE\r\n>UNKNOWN<\r\n", out);
}

TEST_F(WwivMsgTest, Anonymous_Readable) {
  Post(0, "rushfan #1", "Who", "text\r\n");
  SetAnony(0, 1, anony_sender);
  msgs->read_anonymous_ = true;
  EXPECT_EQ("rushfan #1/TRUE/1700000000\r\n", Run(R"(
m = wwiv.msg.area("1").message(1)
print m.from; "/"; m.anonymous; "/"; m.date
)"));
}

TEST_F(WwivMsgTest, Anonymous_DearAbby) {
  Post(0, "rushfan #1", "Help", "text\r\n");
  SetAnony(0, 1, anony_sender_da);
  EXPECT_EQ("Abby/0\r\n", Run(R"(
m = wwiv.msg.area("1").message(1)
print m.from; "/"; m.date
)"));
}

// BbsMessageBase's rights follow lcs() and read_post.
class BbsMessageBaseTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    slrec sl{};
    helper.config().sl(50, sl);
    sl.ability = ability_read_post_anony;
    helper.config().sl(60, sl);
    sl.ability = ability_limited_cosysop;
    helper.config().sl(70, sl);
    sl.ability = ability_cosysop;
    helper.config().sl(80, sl);
    qsc = 3;
    helper.sess().qsc = &qsc;
  }

  void TearDown() override { helper.sess().qsc = nullptr; }

  BbsHelper helper{};
  uint32_t qsc{0};
};

TEST_F(BbsMessageBaseTest, UserSubs) {
  a()->usub = {usersubrec{"1", 4}, usersubrec{"G", 7}};
  a()->set_current_user_sub_num(1);
  BbsMessageBase m(*a());
  const auto subs = m.user_subs();
  ASSERT_EQ(2u, subs.size());
  EXPECT_EQ(4, subs[0].subnum);
  EXPECT_EQ("1", subs[0].key);
  EXPECT_EQ(7, subs[1].subnum);
  EXPECT_EQ("G", subs[1].key);
  EXPECT_EQ(std::optional<int>(1), m.current_user_sub());

  a()->usub.clear();
  EXPECT_EQ(std::nullopt, m.current_user_sub());
}

TEST_F(BbsMessageBaseTest, Rights) {
  BbsMessageBase m(*a());
  helper.sess().effective_sl(50);
  EXPECT_FALSE(m.is_cosysop(3));
  EXPECT_FALSE(m.can_read_anonymous(3));

  helper.sess().effective_sl(60);
  EXPECT_FALSE(m.is_cosysop(3));
  EXPECT_TRUE(m.can_read_anonymous(3));

  // A limited co-sysop of sub 3 only.
  helper.sess().effective_sl(70);
  EXPECT_TRUE(m.is_cosysop(3));
  EXPECT_TRUE(m.can_read_anonymous(3));
  EXPECT_FALSE(m.is_cosysop(4));
  EXPECT_FALSE(m.can_read_anonymous(4));
  qsc = 999;
  EXPECT_TRUE(m.is_cosysop(4));

  helper.sess().effective_sl(80);
  EXPECT_TRUE(m.is_cosysop(4));

  helper.sess().effective_sl(255);
  EXPECT_TRUE(m.is_cosysop(4));
}

// The example in wwivbasic/docs/BINDINGS.md.
TEST_F(WwivMsgTest, DocExample) {
  Post(0, "rushfan #1", "Hello", "Hi there\r\nBye\r\n");
  EXPECT_EQ("1) General (1 messages)\r\n"
            "S) Sysop (0 messages)\r\n"
            "1 Hello - rushfan #1\r\n"
            "Hello\r\n"
            "Hi there\r\n"
            "Bye\r\n",
            Run(R"bas(
import "@wwiv.msg"

for a in areas()
  print a.key; ") "; a.name; " ("; a.count; " messages)"
next

a = area("1")
for h in a.headers()
  print h.number; " "; h.title; " - "; h.from
next

m = a.message(1)
print m.title
for line in m.lines()
  print line
next
)bas"));
}

TEST_F(WwivMsgTest, Message_KeepsBlankLinesAndIndentation) {
  Post(0, "a #1", "Art", "Para one\r\n\r\n  /\\_/\\\r\n\r\nPara two\r\n");
  EXPECT_EQ("[Para one]\r\n[]\r\n[  /\\_/\\]\r\n[]\r\n[Para two]\r\n", Run(R"(
for line in wwiv.msg.area("1").message(1).lines()
  print "["; line; "]"
next
)"));
}
