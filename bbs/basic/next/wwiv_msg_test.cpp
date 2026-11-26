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
#include "bbs/basic/message_base.h"
#include "bbs/basic/next/next_engine.h"
#include "bbs/basic/next/type_ahead_test_util.h"
#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/bbs_helper.h"
#include "common/input.h"
#include "common/output.h"
#include "core/datafile.h"
#include "core/file.h"
#include "fmt/format.h"
#include "sdk/config.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/vardec.h"
#include "install_scripts.h"

#include <functional>
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

  [[nodiscard]] std::vector<UserSub> user_subs() const override {
    if (on_user_subs) {
      on_user_subs();
    }
    return user_subs_;
  }
  [[nodiscard]] std::optional<UserSub> current_user_sub() const override { return current_; }
  [[nodiscard]] const Subs& subs() const override { return subs_; }
  [[nodiscard]] std::unique_ptr<MessageArea> open(int subnum) override {
    const auto& sub = subs_.sub(subnum);
    if (!api_.Exist(sub)) {
      return nullptr;
    }
    return api_.Open(sub, subnum);
  }
  [[nodiscard]] bool is_cosysop(int) const override {
    // HEADERS and FIND check this before reading, so a test can change the
    // sub between them.
    if (on_rights_check) {
      on_rights_check();
    }
    return cosysop_;
  }
  [[nodiscard]] bool can_read_anonymous(int) const override { return read_anonymous_; }

  std::vector<UserSub> user_subs_;
  std::optional<UserSub> current_;
  // Called by user_subs() (wwiv.msg.areas()), so a test can act mid-script.
  std::function<void()> on_user_subs;
  std::function<void()> on_rights_check;
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
    const auto n = area->number_of_messages();
    // The BBS gives each post the next serial (qscan) from status.dat; the
    // test BBS has none, so every post would get 1.
    UpdatePost(subnum, n, [this](postrec& p) { p.qscan = next_qscan_++; });
    return n;
  }

  // Deletes post n of sub 0 after HEADERS() has read the sub and before the
  // next FIND() looks in it, as if someone deleted it while the user read
  // the list. HEADERS and FIND each check the user's rights (is_cosysop)
  // once before reading the sub, so the delete happens on the second check.
  void DeletePostBeforeFind(int n) {
    int checks = 0;
    msgs->on_rights_check = [this, n, checks]() mutable {
      if (++checks == 2) {
        EXPECT_TRUE(api->Open(subs->sub(0), 0)->DeleteMessage(n));
      }
    };
  }

  // Changes post number in sub subnum in the sub file.
  void UpdatePost(int subnum, int number, const std::function<void(postrec&)>& f) {
    const auto path = FilePath(helper.config().datadir(), subs->sub(subnum).filename + ".sub");
    DataFile<postrec> file(path, File::modeBinary | File::modeReadWrite);
    ASSERT_TRUE(file);
    postrec p{};
    ASSERT_TRUE(file.Read(number, &p));
    f(p);
    ASSERT_TRUE(file.Write(number, &p));
  }

  // AddMessage doesn't keep a post's anonymous flags, so set them in the sub
  // file afterwards.
  void SetAnony(int subnum, int number, uint8_t anony) {
    UpdatePost(subnum, number, [anony](postrec& p) { p.anony = anony; });
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
  uint32_t next_qscan_{1000};
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
  msgs->current_ = MessageBase::UserSub{1, "S"};
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

TEST_F(WwivMsgTest, Message_Zero) {
  Post(0, "a #1", "One", "1\r\n");
  bool ok{true};
  const auto out = Run("m = wwiv.msg.area(\"1\").message(0)\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("MESSAGE: there is no message 0 in 'General' (it has 1)"));
}

TEST_F(WwivMsgTest, Message_Negative) {
  Post(0, "a #1", "One", "1\r\n");
  bool ok{true};
  const auto out = Run("m = wwiv.msg.area(\"1\").message(-1)\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("MESSAGE: there is no message -1 in 'General' (it has 1)"));
}

// A sub whose message files haven't been made yet has no messages.
TEST_F(WwivMsgTest, Message_AreaNotCreated) {
  subboard_t sub{};
  sub.name = "New";
  sub.filename = "new";
  sub.storage_type = 2;
  ASSERT_TRUE(subs->add(sub));
  msgs->user_subs_.push_back({3, "N"});
  bool ok{true};
  const auto out = Run(R"(
a = wwiv.msg.area("N")
print a.count; "/"; len(a.headers())
m = a.message(1)
)", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("0/0\r\n"));
  EXPECT_THAT(out, HasSubstr("MESSAGE: there is no message 1 in 'New' (it has 0)"));
}

// Without a MessageBase there's no wwiv.msg module.
TEST_F(WwivMsgTest, NoMessageBase) {
  NextEngine engine(bin, bout, helper.config(), &helper.context());
  EXPECT_FALSE(engine.RunScript("msgtest", "print wwiv.msg.module_name()\nprint \"after\"\n"));
  EXPECT_THAT(helper.io()->captured(), Not(HasSubstr("after")));
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

TEST_F(WwivMsgTest, Anonymous_DearAbby_Readable) {
  Post(0, "rushfan #1", "Help", "text\r\n");
  SetAnony(0, 1, anony_sender_da);
  msgs->read_anonymous_ = true;
  // The sender is shown, but the BBS hides the date of these from everyone.
  EXPECT_EQ("rushfan #1/TRUE/0\r\n", Run(R"(
m = wwiv.msg.area("1").message(1)
print m.from; "/"; m.anonymous; "/"; m.date
)"));
}

TEST_F(WwivMsgTest, Anonymous_ProblemedPerson) {
  Post(0, "rushfan #1", "Help", "text\r\n");
  SetAnony(0, 1, anony_sender_pp);
  EXPECT_EQ("Problemed Person/TRUE/0\r\n", Run(R"(
m = wwiv.msg.area("1").message(1)
print m.from; "/"; m.anonymous; "/"; m.date
)"));
}

TEST_F(WwivMsgTest, Anonymous_ProblemedPerson_Readable) {
  Post(0, "rushfan #1", "Help", "text\r\n");
  SetAnony(0, 1, anony_sender_pp);
  msgs->read_anonymous_ = true;
  EXPECT_EQ("rushfan #1/TRUE/0\r\n", Run(R"(
m = wwiv.msg.area("1").message(1)
print m.from; "/"; m.anonymous; "/"; m.date
)"));
}

TEST_F(WwivMsgTest, Message_Deleted) {
  Post(0, "a #1", "Deleted", "gone\r\n", status_delete);
  bool ok{true};
  const auto out = Run("m = wwiv.msg.area(\"1\").message(1)\nprint m.text\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("MESSAGE: message 1 in 'General' can't be read"));
  EXPECT_THAT(out, Not(HasSubstr("gone")));
}

TEST_F(WwivMsgTest, Message_CoSysopReadsUnvalidatedAndDeleted) {
  Post(0, "a #1", "Unvalidated", "two\r\n", status_unvalidated);
  Post(0, "a #1", "Deleted", "three\r\n", status_delete);
  msgs->cosysop_ = true;
  EXPECT_EQ("two\r\nthree\r\n", Run(R"(
print wwiv.msg.area("1").message(1).text;
print wwiv.msg.area("1").message(2).text;
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
    ASSERT_TRUE(helper.files().Mkdir("msgs"));
    a()->init_message_areas_for_test();
  }

  void TearDown() override { helper.sess().qsc = nullptr; }

  // Adds a sub to the BBS's subs, making its message files if create.
  int AddSub(const std::string& filename, int storage_type, bool create) {
    subboard_t sub{};
    sub.name = filename;
    sub.filename = filename;
    sub.storage_type = static_cast<uint8_t>(storage_type);
    const auto n = a()->subs().size();
    EXPECT_TRUE(a()->subs().add(sub));
    if (create) {
      EXPECT_TRUE(a()->msgapi(2)->Create(sub, n));
    }
    return n;
  }

  BbsHelper helper{};
  uint32_t qsc{0};
};

TEST_F(BbsMessageBaseTest, Open) {
  const auto type2 = AddSub("type2", 2, true);
  const auto type0 = AddSub("type0", 0, true);
  const auto missing = AddSub("missing", 2, false);
  const auto type1 = AddSub("type1", 1, false);

  BbsMessageBase m(*a());
  const auto area = m.open(type2);
  ASSERT_NE(nullptr, area);
  EXPECT_EQ(0, area->number_of_messages());
  EXPECT_NE(nullptr, m.open(type0));
  // No message files yet.
  EXPECT_EQ(nullptr, m.open(missing));
  // No API for this storage type.
  EXPECT_EQ(nullptr, m.open(type1));
  // Not a sub.
  EXPECT_EQ(nullptr, m.open(-1));
  EXPECT_EQ(nullptr, m.open(a()->subs().size()));
}

TEST_F(BbsMessageBaseTest, UserSubs_SkipsSubsThatDontExist) {
  AddSub("one", 2, false);
  a()->usub = {usersubrec{"1", 0}, usersubrec{"2", 5}, usersubrec{"3", -1}};
  BbsMessageBase m(*a());
  const auto subs = m.user_subs();
  ASSERT_EQ(1u, subs.size());
  EXPECT_EQ(0, subs[0].subnum);
  EXPECT_EQ("1", subs[0].key);
}

TEST_F(BbsMessageBaseTest, UserSubs) {
  for (auto i = 0; i < 8; i++) {
    AddSub(fmt::format("sub{}", i), 2, false);
  }
  a()->usub = {usersubrec{"1", 4}, usersubrec{"G", 7}};
  a()->set_current_user_sub_num(1);
  BbsMessageBase m(*a());
  const auto subs = m.user_subs();
  ASSERT_EQ(2u, subs.size());
  EXPECT_EQ(4, subs[0].subnum);
  EXPECT_EQ("1", subs[0].key);
  EXPECT_EQ(7, subs[1].subnum);
  EXPECT_EQ("G", subs[1].key);
  ASSERT_TRUE(m.current_user_sub());
  EXPECT_EQ(7, m.current_user_sub()->subnum);
  EXPECT_EQ("G", m.current_user_sub()->key);

  a()->usub.clear();
  EXPECT_EQ(std::nullopt, m.current_user_sub());
}

// The current sub is a position in usub, which user_subs() filters: an entry
// that isn't a sub before it mustn't shift it.
TEST_F(BbsMessageBaseTest, CurrentUserSub_AfterASubThatDoesntExist) {
  for (auto i = 0; i < 8; i++) {
    AddSub(fmt::format("sub{}", i), 2, false);
  }
  a()->usub = {usersubrec{"1", 99}, usersubrec{"G", 7}};
  BbsMessageBase m(*a());
  a()->set_current_user_sub_num(1);
  ASSERT_TRUE(m.current_user_sub());
  EXPECT_EQ(7, m.current_user_sub()->subnum);
  EXPECT_EQ("G", m.current_user_sub()->key);
  // The current entry isn't a sub.
  a()->set_current_user_sub_num(0);
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

  // No qscan pointers yet: a limited co-sysop of nothing.
  helper.sess().qsc = nullptr;
  EXPECT_FALSE(m.is_cosysop(3));
  EXPECT_FALSE(m.can_read_anonymous(3));
  helper.sess().qsc = &qsc;

  // Not a sub (as when the user has no subs), even with qsc's first entry
  // all ones.
  qsc = 0xffffffff;
  EXPECT_FALSE(m.is_cosysop(-1));
  qsc = 3;

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

TEST_F(WwivMsgTest, Ids_AreDistinct) {
  Post(0, "a #1", "One", "1\r\n");
  Post(0, "a #1", "Two", "2\r\n");
  EXPECT_EQ("FALSE\r\nTRUE\r\n", Run(R"(
h = wwiv.msg.area("1").headers()
print get(h, 0).id = get(h, 1).id
print wwiv.msg.area("1").message(2).id = get(h, 1).id
)"));
}

// After an earlier post is deleted, FIND gives a post's new number.
TEST_F(WwivMsgTest, Find_AfterAnEarlierPostIsDeleted) {
  Post(0, "a #1", "One", "1\r\n");
  Post(0, "a #1", "Two", "2\r\n");
  Post(0, "a #1", "Three", "3\r\n");
  DeletePostBeforeFind(1);
  EXPECT_EQ("2/Three/0\r\n", Run(R"(
a = wwiv.msg.area("1")
h = a.headers()
n = a.find(get(h, 2))
print n; "/"; a.message(n).title; "/"; a.find(get(h, 0))
)"));
}

TEST_F(WwivMsgTest, Find_PostDeleted) {
  Post(0, "a #1", "One", "1\r\n");
  Post(0, "a #1", "Two", "2\r\n");
  DeletePostBeforeFind(2);
  EXPECT_EQ("0\r\n", Run(R"(
a = wwiv.msg.area("1")
h = a.headers()
print a.find(get(h, 1))
)"));
}

TEST_F(WwivMsgTest, Find_TakesAMessageToo) {
  Post(0, "a #1", "One", "1\r\n");
  EXPECT_EQ("1\r\n", Run(R"(
a = wwiv.msg.area("1")
print a.find(a.message(1))
)"));
}

TEST_F(WwivMsgTest, Find_NotAHeader) {
  bool ok{true};
  const auto out = Run("print wwiv.msg.area(\"1\").find(1)\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("FIND: expected a message_header or message"));
}

// An area opens its sub once, but still sees posts made after that.
TEST_F(WwivMsgTest, Area_SeesNewPosts) {
  Post(0, "a #1", "One", "1\r\n");
  msgs->on_user_subs = [this] {
    if (Post(0, "a #1", "Two", "2\r\n") > 2) {
      FAIL() << "posted more than once";
    }
  };
  // AREA calls user_subs() too, so Two is posted when the area is chosen.
  EXPECT_EQ("2/2/Two\r\n", Run(R"(
a = wwiv.msg.area("1")
print a.count; "/"; len(a.headers()); "/"; a.message(2).title
)"));
}

TEST_F(WwivMsgTest, Message_KeepsBlankLinesAndIndentation) {
  Post(0, "a #1", "Art", "Para one\r\n\r\n  /\\_/\\\r\n\r\nPara two\r\n");
  EXPECT_EQ("[Para one]\r\n[]\r\n[  /\\_/\\]\r\n[]\r\n[Para two]\r\n", Run(R"(
for line in wwiv.msg.area("1").message(1).lines()
  print "["; line; "]"
next
)"));
}

// install/scripts/msgview.bas, the sample message viewer, run with keys
// typed ahead.
class MsgViewTest : public WwivMsgTest {
protected:
  void SetUp() override {
    WwivMsgTest::SetUp();
    // Without ANSI the line editor echoes plain text.
    helper.user()->set_flag(User::flag_ansi, false);
    type_ahead::HangUpWhenKeysRunOut(helper.context().session_context(),
                                                    a()->sess());
  }

  void TearDown() override { type_ahead::StopTypingAhead(); }

  // Runs the viewer with keys typed ahead. The keys must end it (Q at the
  // area list): running out of keys hangs up, which fails the test.
  std::string RunViewer(const std::string& keys) {
    type_ahead::TypeAhead(keys);
    const std::string script(reinterpret_cast<const char*>(k_msgview_bas), sizeof(k_msgview_bas));
    bool ok{false};
    auto out = Run(script, &ok);
    EXPECT_TRUE(ok) << out;
    return out;
  }
};

TEST_F(MsgViewTest, ListsAreas) {
  Post(0, "a #1", "One", "1\r\n");
  const auto out = RunViewer("Q\r");
  EXPECT_THAT(out, HasSubstr("Message Areas"));
  EXPECT_THAT(out, HasSubstr("General"));
  EXPECT_THAT(out, HasSubstr("1 msgs"));
  EXPECT_THAT(out, HasSubstr("Sysop"));
  EXPECT_THAT(out, HasSubstr("0 msgs"));
  // Not one of the user's areas.
  EXPECT_THAT(out, Not(HasSubstr("Secret")));
}

TEST_F(MsgViewTest, ReadsMessages) {
  Post(0, "rushfan #1", "Hello", "Hi there\r\n");
  Post(0, "sysop #2", "Re: Hello", "Hi back\r\n");
  // Area 1, message 1, next, then quit the message, the area and the viewer.
  const auto out = RunViewer("1\r1\rNQQ\rQ\r");
  EXPECT_THAT(out, HasSubstr("Title : Hello"));
  EXPECT_THAT(out, HasSubstr("From  : rushfan #1"));
  EXPECT_THAT(out, HasSubstr("Hi there"));
  EXPECT_THAT(out, HasSubstr("Title : Re: Hello"));
  EXPECT_THAT(out, HasSubstr("Hi back"));
  EXPECT_LT(out.find("Hi there"), out.find("Hi back"));
}

TEST_F(MsgViewTest, AreaKeyIsCaseInsensitive) {
  Post(1, "a #1", "For the sysop", "x\r\n");
  const auto out = RunViewer("s\rQ\rQ\r");
  EXPECT_THAT(out, HasSubstr("For the sysop"));
}

TEST_F(MsgViewTest, PagesHeaders) {
  for (auto i = 1; i <= 17; i++) {
    Post(0, "a #1", fmt::format("Post {}", i), "x\r\n");
  }
  const auto out = RunViewer("1\rN\rQ\rQ\r");
  // The first page stops at 15; the next page has the rest.
  const auto next_page = out.find("Message #", out.find("Post 15"));
  ASSERT_NE(std::string::npos, next_page);
  EXPECT_EQ(std::string::npos, out.substr(0, next_page).find("Post 16"));
  EXPECT_THAT(out.substr(next_page), HasSubstr("Post 16"));
  EXPECT_THAT(out.substr(next_page), HasSubstr("Post 17"));
}

TEST_F(MsgViewTest, SkipsWhatTheUserCantRead) {
  Post(0, "a #1", "Visible", "x\r\n");
  Post(0, "a #1", "Hidden", "secret\r\n", status_unvalidated);
  // Message 2 isn't listed, so asking for it says so instead of failing.
  const auto out = RunViewer("1\r2\r\rQ\rQ\r");
  EXPECT_THAT(out, HasSubstr("Visible"));
  EXPECT_THAT(out, Not(HasSubstr("Hidden")));
  EXPECT_THAT(out, Not(HasSubstr("secret")));
  EXPECT_THAT(out, HasSubstr("There is no message 2 to read."));
}

// The viewer prints what other users wrote with MCI off.
TEST_F(MsgViewTest, PostsCantRunMci) {
  helper.user()->set_name("READERNAME");
  Post(0, "a #1", "Title |@N", "Text |@N here\r\n");
  const auto out = RunViewer("1\r1\rQQ\rQ\r");
  EXPECT_THAT(out, HasSubstr("Text  here"));
  EXPECT_THAT(out, Not(HasSubstr("READERNAME")));
}

TEST_F(MsgViewTest, CountsOnlyReadableMessages) {
  Post(0, "a #1", "Hidden", "x\r\n", status_unvalidated);
  Post(0, "a #1", "Visible", "y\r\n");
  const auto out = RunViewer("1\r2\rQQ\rQ\r");
  EXPECT_THAT(out, HasSubstr("Msg   : 1 of 1 in General (#2)"));
}

// A post deleted after the list was read moves the later ones down; the
// viewer still shows the post that was picked.
TEST_F(MsgViewTest, EarlierPostDeletedWhileReading) {
  Post(0, "a #1", "One", "1\r\n");
  Post(0, "a #1", "Two", "2\r\n");
  Post(0, "a #1", "Three", "3\r\n");
  DeletePostBeforeFind(1);
  const auto out = RunViewer("1\r3\rQQ\rQ\r");
  EXPECT_THAT(out, HasSubstr("Title : Three"));
  EXPECT_THAT(out, HasSubstr("(#2)"));
}

TEST_F(MsgViewTest, PostDeletedWhileReading) {
  Post(0, "a #1", "One", "1\r\n");
  Post(0, "a #1", "Two", "2\r\n");
  DeletePostBeforeFind(2);
  // Area 1, message 2 (gone), a key for the pause, then quit.
  const auto out = RunViewer("1\r2\r\rQ\rQ\r");
  EXPECT_THAT(out, HasSubstr("That message has been deleted."));
  EXPECT_THAT(out, Not(HasSubstr("Title : Two")));
  // The list is read again.
  EXPECT_THAT(out, HasSubstr("General (1 messages)"));
}

TEST_F(MsgViewTest, EmptyArea) {
  const auto out = RunViewer("S\r\rQ\r");
  EXPECT_THAT(out, HasSubstr("There are no messages in Sysop."));
}

TEST_F(MsgViewTest, UnknownArea) {
  const auto out = RunViewer("ZZ\r\rQ\r");
  EXPECT_THAT(out, HasSubstr("There is no area ZZ."));
}

// msgview.bas full-screen, as callers with ANSI see it. Keys come from the
// caller's terminal, where ANSI key sequences (ESC [ B is Down) come from.
class MsgViewFullScreenTest : public WwivMsgTest {
protected:
  void SetUp() override {
    WwivMsgTest::SetUp();
    helper.user()->set_flag(User::flag_ansi, true);
    helper.user()->screen_width(80);
    helper.sess().num_screen_lines(25);
    type_ahead::HangUpWhenKeysRunOut(helper.context().session_context(), a()->sess());
  }

  void TearDown() override {
    type_ahead::StopTypingAhead();
    remote()->keys.clear();
  }

  TestRemoteIO* remote() { return static_cast<TestRemoteIO*>(helper.io()->remote_io()); }

  // Runs the viewer with keys from the caller. The keys must end it (Q at
  // the area list): running out of keys hangs up, which fails the test.
  std::string RunViewer(const std::string& keys) {
    a()->sess().incom(true);
    a()->sess().ok_modem_stuff(true);
    remote()->keys = keys;
    const std::string script(reinterpret_cast<const char*>(k_msgview_bas), sizeof(k_msgview_bas));
    bool ok{false};
    auto out = Run(script, &ok);
    EXPECT_TRUE(ok) << out;
    EXPECT_FALSE(a()->sess().hangup()) << "The keys ran out";
    return out;
  }

  static constexpr char kDown[] = "\x1b[B";
  static constexpr char kPageDown[] = "\x1b[6~";
};

TEST_F(MsgViewFullScreenTest, ListsAreas) {
  Post(0, "a #1", "One", "1\r\n");
  const auto out = RunViewer("q");
  EXPECT_THAT(out, HasSubstr("Message Areas"));
  EXPECT_THAT(out, HasSubstr("General"));
  EXPECT_THAT(out, HasSubstr("1 msgs"));
  EXPECT_THAT(out, HasSubstr("Sysop"));
  EXPECT_THAT(out, Not(HasSubstr("Secret")));
}

TEST_F(MsgViewFullScreenTest, ReadsMessages) {
  Post(0, "rushfan #1", "Hello", "Hi there\r\n");
  Post(0, "sysop #2", "Re: Hello", "Hi back\r\n");
  // General, the first message, next, then quit the message, the list and
  // the viewer.
  const auto out = RunViewer("\r\rnqqq");
  EXPECT_THAT(out, HasSubstr("Title : Hello"));
  EXPECT_THAT(out, HasSubstr("From  : rushfan #1"));
  EXPECT_THAT(out, HasSubstr("Hi there"));
  EXPECT_THAT(out, HasSubstr("Msg   : 2 of 2 in General"));
  EXPECT_THAT(out, HasSubstr("Title : Re: Hello"));
  EXPECT_LT(out.find("Hi there"), out.find("Hi back"));
}

// The list opens on the second message, after moving down to it.
TEST_F(MsgViewFullScreenTest, ReadsTheSelectedMessage) {
  Post(0, "a #1", "First", "one\r\n");
  Post(0, "a #1", "Second", "two\r\n");
  const auto out = RunViewer(std::string("\r") + kDown + "\rqqq");
  EXPECT_THAT(out, HasSubstr("Title : Second"));
  EXPECT_THAT(out, Not(HasSubstr("Title : First")));
}

// On a 25-row screen with 5 header lines, 17 rows of text show at a time.
TEST_F(MsgViewFullScreenTest, ScrollsLongMessages) {
  std::string text;
  for (auto i = 1; i <= 30; i++) {
    text += fmt::format("Line {}\r\n", i);
  }
  Post(0, "a #1", "Long", text);
  const auto out = RunViewer(std::string("\r\r") + kPageDown + "qqq");
  // Each page's text is drawn before the bar that says which lines show.
  const auto first_page_end = out.find("1-17 of 30");
  ASSERT_NE(std::string::npos, first_page_end) << out;
  EXPECT_THAT(out.substr(0, first_page_end), HasSubstr("Line 17"));
  EXPECT_THAT(out.substr(0, first_page_end), Not(HasSubstr("Line 18")));
  EXPECT_THAT(out.substr(first_page_end), HasSubstr("Line 30"));
  EXPECT_THAT(out.substr(first_page_end), HasSubstr("18-30 of 30"));
}

// Space pages, and at the end of a message goes on to the next.
TEST_F(MsgViewFullScreenTest, SpaceAtTheEndReadsTheNext) {
  Post(0, "a #1", "First", "one\r\n");
  Post(0, "a #1", "Second", "two\r\n");
  const auto out = RunViewer("\r\r qqq");
  EXPECT_THAT(out, HasSubstr("Title : Second"));
}

// Lines wider than the screen are split rather than wrapping on their own.
TEST_F(MsgViewFullScreenTest, SplitsWideLines) {
  Post(0, "a #1", "Wide", std::string(79, 'x') + "END\r\n");
  const auto out = RunViewer("\r\rqqq");
  EXPECT_THAT(out, HasSubstr("1-2 of 2"));
  EXPECT_THAT(out, HasSubstr("END"));
}

TEST_F(MsgViewFullScreenTest, PostsCantRunMci) {
  helper.user()->set_name("READERNAME");
  Post(0, "a #1", "Title |@N", "Text |@N here\r\n");
  const auto out = RunViewer("\r\rqqq");
  EXPECT_THAT(out, HasSubstr("Text  here"));
  EXPECT_THAT(out, Not(HasSubstr("READERNAME")));
}

TEST_F(MsgViewFullScreenTest, PostDeletedWhileReading) {
  Post(0, "a #1", "One", "1\r\n");
  Post(0, "a #1", "Two", "2\r\n");
  DeletePostBeforeFind(2);
  // General, down to message 2 (gone), a key, then quit.
  const auto out = RunViewer(std::string("\r") + kDown + "\r qq");
  EXPECT_THAT(out, HasSubstr("That message has been deleted."));
  EXPECT_THAT(out, Not(HasSubstr("Title : Two")));
  // The list is read again.
  EXPECT_THAT(out, HasSubstr("General (1 messages)"));
}

// An empty area says so on the command line, and stays on the area list.
TEST_F(MsgViewFullScreenTest, EmptyArea) {
  // Down to Sysop, Enter, then quit.
  const auto out = RunViewer(std::string(kDown) + "\rq");
  EXPECT_THAT(out, HasSubstr("There are no messages in Sysop."));
}
