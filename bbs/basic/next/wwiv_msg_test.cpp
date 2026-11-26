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

#include "bbs/application.h"
#include "bbs/basic/message_base.h"
#include "bbs/basic/next/bbs_message_base.h"
#include "bbs/basic/next/next_engine.h"
#include "bbs/basic/next/type_ahead_test_util.h"
#include "bbs/bbs.h"
#include "bbs/bbs_helper.h"
#include "common/input.h"
#include "common/output.h"
#include "core/datafile.h"
#include "core/datetime.h"
#include "core/file.h"
#include "fmt/format.h"
#include "install_scripts.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/vardec.h"

#include <filesystem>
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
using testing::StartsWith;

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
  [[nodiscard]] Poster poster(int) const override { return poster_; }
  [[nodiscard]] bool can_post(int) const override { return can_post_; }
  [[nodiscard]] std::optional<EditedPost> edit_post(int subnum, const std::string& title,
                                                    const std::string& irt, const std::string& to,
                                                    uint8_t anonymous,
                                                    const std::optional<ReplyTo>& reply) override {
    edits_.push_back(fmt::format("{}|{}|{}|{}|{}", subnum, title, irt, to, anonymous));
    reply_ = reply;
    return edited_;
  }
  void posted(int subnum, const std::string& title, std::chrono::seconds writing_time) override {
    posted_.push_back(fmt::format("{}:{}", subnum, title));
    writing_times_.push_back(static_cast<int>(writing_time.count()));
  }
  void deleted(int subnum, const std::string& title, int owneruser, uint32_t) override {
    deleted_.push_back(fmt::format("{}:{}:{}", subnum, title, owneruser));
  }

  std::vector<UserSub> user_subs_;
  std::optional<UserSub> current_;
  // Called by user_subs() (wwiv.msg.areas()), so a test can act mid-script.
  std::function<void()> on_user_subs;
  std::function<void()> on_rights_check;
  bool cosysop_{false};
  bool read_anonymous_{false};
  // The user as posting sees them: user #1, who may post 10 a day.
  Poster poster_{1, "TESTER #1", "Real Name", false, false, false, false, 0, 10, false};
  bool can_post_{true};
  // What edit_post() returns (nullopt: the user aborted), and what it was
  // asked: "subnum|title|irt|to|anonymous".
  std::optional<EditedPost> edited_;
  std::vector<std::string> edits_;
  // The post the last edit_post replied to, if any.
  std::optional<ReplyTo> reply_;
  // posted()'s writing_time for each post, in seconds.
  std::vector<int> writing_times_;
  // What posted() and deleted() were told: "subnum:title" and
  // "subnum:title:owneruser".
  std::vector<std::string> posted_;
  std::vector<std::string> deleted_;

private:
  Subs& subs_;
  MessageApi& api_;
};

} // namespace

// Hands a test's FakeMessageBase to an engine, which owns what it's given,
// so the fake outlives each script and a test can run several.
class ForwardingMessageBase final : public MessageBase {
public:
  explicit ForwardingMessageBase(MessageBase& m) : m_(m) {}
  [[nodiscard]] std::vector<UserSub> user_subs() const override { return m_.user_subs(); }
  [[nodiscard]] std::optional<UserSub> current_user_sub() const override {
    return m_.current_user_sub();
  }
  [[nodiscard]] const Subs& subs() const override { return m_.subs(); }
  [[nodiscard]] std::unique_ptr<MessageArea> open(int subnum) override { return m_.open(subnum); }
  [[nodiscard]] bool is_cosysop(int subnum) const override { return m_.is_cosysop(subnum); }
  [[nodiscard]] bool can_read_anonymous(int subnum) const override {
    return m_.can_read_anonymous(subnum);
  }
  [[nodiscard]] Poster poster(int subnum) const override { return m_.poster(subnum); }
  [[nodiscard]] bool can_post(int subnum) const override { return m_.can_post(subnum); }
  [[nodiscard]] std::optional<EditedPost> edit_post(int subnum, const std::string& title,
                                                    const std::string& irt, const std::string& to,
                                                    uint8_t anonymous,
                                                    const std::optional<ReplyTo>& reply) override {
    return m_.edit_post(subnum, title, irt, to, anonymous, reply);
  }
  void posted(int subnum, const std::string& title, std::chrono::seconds writing_time) override {
    m_.posted(subnum, title, writing_time);
  }
  void deleted(int subnum, const std::string& title, int owneruser, uint32_t daten) override {
    m_.deleted(subnum, title, owneruser, daten);
  }

private:
  MessageBase& m_;
};

class WwivMsgTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    helper.config().scripting_enabled(true);
    ASSERT_TRUE(helper.files().Mkdir("msgs"));
    a()->init_status_and_instances_for_test();

    MessageApiOptions options;
    options.overflow_strategy = OverflowStrategy::delete_none;
    api = std::make_unique<WWIVMessageApi>(options, helper.config(), networks_,
                                           new NullLastReadImpl());
    subs = std::make_unique<Subs>(helper.config().datadir(), std::vector<net::Network>{});
    AddSub("General", "general", "General chat");
    AddSub("Sysop", "sysop", "For the sysop");
    // Not one of the user's subs.
    AddSub("Secret", "secret", "Nobody can read this");

    msgs = std::make_unique<FakeMessageBase>(*subs, *api);
    msgs->user_subs_ = {{0, "1"}, {1, "S"}};
  }

  // The networks the message base knows, set before SetUp.
  std::vector<net::Network> networks_;

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
    engine.set_message_base(std::make_unique<ForwardingMessageBase>(*msgs));
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

// The user as the BBS's post() sees them.
TEST_F(BbsMessageBaseTest, Poster) {
  const auto sub = AddSub("general", 2, true);
  slrec sl{};
  sl.posts = 7;
  sl.ability = ability_post_anony;
  helper.config().sl(40, sl);
  helper.sess().effective_sl(40);
  helper.user()->restriction(User::restrictPost | User::restrictNet);
  helper.user()->posts_today(3);
  helper.user()->real_name("Real Person");
  helper.user()->set_name("POSTER");
  helper.sess().user_num(1);

  BbsMessageBase m(*a());
  const auto p = m.poster(sub);
  EXPECT_EQ(1, p.user_num);
  // Without a names list, the user's own name and number.
  EXPECT_EQ("Poster #1", p.name);
  EXPECT_EQ("Real Person", p.real_name);
  EXPECT_TRUE(p.restrict_post);
  EXPECT_TRUE(p.restrict_net);
  EXPECT_FALSE(p.restrict_anony);
  EXPECT_FALSE(p.restrict_validate);
  EXPECT_EQ(3, p.posts_today);
  EXPECT_EQ(7, p.max_posts_today);
  EXPECT_TRUE(p.sl_post_anony);
}

TEST_F(BbsMessageBaseTest, CanPost) {
  const auto open = AddSub("open", 2, true);
  const auto closed = AddSub("closed", 2, true);
  a()->subs().sub(closed).post_acs = "user.sl >= 255";
  helper.sess().effective_sl(50);
  BbsMessageBase m(*a());
  EXPECT_TRUE(m.can_post(open));
  EXPECT_FALSE(m.can_post(closed));
  EXPECT_FALSE(m.can_post(99));
}

// A post counts for the user and the system; deleting the user's own post
// takes its credit back.
TEST_F(BbsMessageBaseTest, PostedAndDeleted) {
  const auto sub = AddSub("general", 2, true);
  {
    // The status manager updates status.dat; the test BBS has none.
    DataFile<statusrec_t> status(FilePath(helper.config().datadir(), STATUS_DAT),
                                 File::modeBinary | File::modeReadWrite | File::modeCreateFile);
    ASSERT_TRUE(status);
    statusrec_t s{};
    ASSERT_TRUE(status.Write(0, &s));
  }
  a()->init_status_and_instances_for_test();
  auto& u = *helper.user();
  u.messages_posted(5);
  u.posts_today(1);
  u.firston("01/01/20");
  const auto msgs_today = a()->status_manager()->get_status()->msgs_today();
  const auto local_posts = a()->status_manager()->get_status()->localposts();

  BbsMessageBase m(*a());
  m.posted(sub, "Hello", std::chrono::seconds(0));
  EXPECT_EQ(6, u.messages_posted());
  EXPECT_EQ(2, u.posts_today());
  EXPECT_EQ(0, u.posts_net());
  EXPECT_EQ(local_posts + 1, a()->status_manager()->get_status()->localposts());
  // AddMessage counts the post in today's posts, not posted().
  EXPECT_EQ(msgs_today, a()->status_manager()->get_status()->msgs_today());

  const auto now = wwiv::core::daten_t_now();
  // Someone else's post: no change.
  m.deleted(sub, "Theirs", helper.sess().user_num() + 1, now);
  EXPECT_EQ(6, u.messages_posted());
  m.deleted(sub, "Hello", helper.sess().user_num(), now);
  EXPECT_EQ(5, u.messages_posted());
  // A post from before this account was made (a reused user number).
  m.deleted(sub, "Old", helper.sess().user_num(), 1);
  EXPECT_EQ(5, u.messages_posted());
}

// Sets up the user to write a post in the BBS's line editor, with keys
// typed ahead.
static void SetUpLineEditor(BbsHelper& helper) {
  a()->init_users_for_test();
  ASSERT_TRUE(a()->names()->Add("WRITER", 1));
  helper.user()->set_name("WRITER");
  helper.sess().user_num(1);
  helper.user()->clear_flag(User::flag_ansi);
  helper.user()->default_editor(0);
  ASSERT_TRUE(helper.files().Mkdir("temp"));
  helper.sess().dirs().temp_directory(helper.files().Dir("temp").string());
}

// edit_post runs the BBS's own editor (here the line editor, with keys typed
// ahead), on the sub it's given, and hands back what follows its author and
// date lines.
TEST_F(BbsMessageBaseTest, EditPost_LineEditor) {
  const auto sub = AddSub("general", 2, true);
  SetUpLineEditor(helper);
  helper.sess().SetCurrentReadMessageArea(-1);
  helper.sess().irt("Before");

  type_ahead::TypeAhead("Hello\rLine one\rLine two\r/S\r");
  BbsMessageBase m(*a());
  const auto e = m.edit_post(sub, "", "", "", 0, std::nullopt);
  type_ahead::StopTypingAhead();

  ASSERT_TRUE(e.has_value());
  EXPECT_EQ("Hello", e->title);
  EXPECT_EQ(0, e->anony);
  EXPECT_THAT(e->text, HasSubstr("\x04"
                                 "0PID: WWIV "));
  EXPECT_THAT(e->text, HasSubstr("Line one\r\nLine two\r\n"));
  // Not the author and date lines; the SDK adds those.
  EXPECT_THAT(e->text, Not(HasSubstr("Writer")));
  EXPECT_THAT(e->from, StartsWith("Writer #1"));
  // The BBS's current sub and in-reply-to title are put back.
  EXPECT_EQ(-1, helper.sess().GetCurrentReadMessageArea());
  EXPECT_EQ("Before", helper.sess().irt());
}

// A reply can quote the post it replies to (/Q in the line editor), as in
// the BBS's reader: /Q lists the post's lines to choose from. (Listing them
// checks for an abort key, which takes the typed-ahead keys after /Q, so the
// test stops there: the keys run out, which hangs up.)
TEST_F(BbsMessageBaseTest, EditPost_ReplyQuotes) {
  const auto sub = AddSub("general", 2, true);
  SetUpLineEditor(helper);
  type_ahead::HangUpWhenKeysRunOut(helper.context().session_context(), a()->sess());
  type_ahead::TypeAhead("/Q\r");
  BbsMessageBase m(*a());
  const MessageBase::ReplyTo reply{"Original Author", "Quoted words\r\n", 1700000000};
  const auto e = m.edit_post(sub, "Re: Hi", "", "Original Author", 0, reply);
  type_ahead::StopTypingAhead();
  EXPECT_THAT(helper.io()->captured(), HasSubstr("1 OA> Quoted words"));
  // Hung up, so nothing was posted.
  EXPECT_FALSE(e.has_value());
}

// An empty title aborts, as in the BBS.
TEST_F(BbsMessageBaseTest, EditPost_Aborted) {
  const auto sub = AddSub("general", 2, true);
  SetUpLineEditor(helper);
  type_ahead::TypeAhead("\r");
  BbsMessageBase m(*a());
  EXPECT_FALSE(m.edit_post(sub, "", "", "", 0, std::nullopt).has_value());
  type_ahead::StopTypingAhead();
}

// Where the user may choose, the editor asks whether to post anonymously.
TEST_F(BbsMessageBaseTest, EditPost_Anonymous) {
  const auto sub = AddSub("general", 2, true);
  SetUpLineEditor(helper);
  type_ahead::TypeAhead("Line one\r/S\rY");
  BbsMessageBase m(*a());
  const auto e = m.edit_post(sub, "Given title", "", "", anony_enable_anony, std::nullopt);
  type_ahead::StopTypingAhead();
  ASSERT_TRUE(e.has_value());
  EXPECT_EQ("Given title", e->title);
  EXPECT_EQ(anony_sender, e->anony);
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

// Posting and deleting (wwiv.msg writing).
class WwivMsgWriteTest : public WwivMsgTest {
protected:
  void SetUp() override {
    WwivMsgTest::SetUp();
    helper.config().script_package_msg_write_enabled(true);
    fake = msgs.get();
  }

  // The sub's post n, read straight from the message base.
  std::optional<Message> Read(int subnum, int n) {
    return api->Open(subs->sub(subnum), subnum)->ReadMessage(n);
  }

  // msgs, which the scripts use.
  FakeMessageBase* fake{nullptr};
};

TEST_F(WwivMsgWriteTest, Post) {
  const auto out = Run(R"(
a = wwiv.msg.area("1")
n = a.post("Hello", list("Line one", "", "Line three"))
m = a.message(n)
print n; " "; a.count; " "; m.title; " / "; m.from; " / "; m.anonymous
for line in m.lines()
  print "["; line; "]"
next
)");
  EXPECT_EQ("1 1 Hello / TESTER #1 / FALSE\r\n[Line one]\r\n[]\r\n[Line three]\r\n", out);
  EXPECT_THAT(fake->posted_, ElementsAre("0:Hello"));
  const auto m = Read(0, 1);
  ASSERT_TRUE(m);
  EXPECT_EQ(1, m->header().from_usernum());
  EXPECT_EQ(0, m->header().from_system());
  EXPECT_FALSE(m->header().unvalidated());
  // Signed with the program that wrote it, as the BBS's editor does.
  EXPECT_THAT(m->text().string(), HasSubstr("\x04"
                                            "0PID: WWIV "));
}

// Text can be a string; any line ending works, and ^D and ^Z are dropped so
// a script can't add control lines or end the text early.
TEST_F(WwivMsgWriteTest, Post_TextString) {
  const auto out = Run(R"(
a = wwiv.msg.area("1")
n = a.post("Hi", "one" + CHR(10) + "two" + CHR(13) + "three" + CHR(13) + CHR(10) + CHR(4) + "0MSGID: x" + CHR(26) + "!")
for line in a.message(n).lines()
  print "["; line; "]"
next
)");
  EXPECT_EQ("[one]\r\n[two]\r\n[three]\r\n[0MSGID: x!]\r\n", out);
}

TEST_F(WwivMsgWriteTest, Post_To) {
  const auto out = Run(R"(
a = wwiv.msg.area("1")
n = a.post("Re: Hello", "Thanks", "Rushfan")
print a.message(n).to
)");
  EXPECT_EQ("Rushfan\r\n", out);
}

TEST_F(WwivMsgWriteTest, Post_Unvalidated) {
  fake->poster_.restrict_validate = true;
  Run("n = wwiv.msg.area(\"1\").post(\"Hi\", \"x\")\n");
  const auto m = Read(0, 1);
  ASSERT_TRUE(m);
  EXPECT_TRUE(m->header().unvalidated());
}

// The sysop turns writing on in wwivconfig; it's off by default.
TEST_F(WwivMsgWriteTest, Disabled) {
  helper.config().script_package_msg_write_enabled(false);
  Post(0, "a #1", "Mine", "x\r\n");
  EXPECT_EQ("FALSE\r\n", Run("print wwiv.msg.area(\"1\").can_post\n"));
  EXPECT_THAT(Run("n = wwiv.msg.area(\"1\").post(\"Hi\", \"x\")\n"),
              HasSubstr("message_area.post: posting from scripts isn't turned on on this BBS"));
  EXPECT_THAT(Run("a = wwiv.msg.area(\"1\")\nx = a.delete_message(get(a.headers(), 0))\n"),
              HasSubstr("deleting from scripts isn't turned on on this BBS"));
}

TEST_F(WwivMsgWriteTest, CantPost) {
  auto expect_error = [this](const std::string& error) {
    const auto out = Run(R"(
a = wwiv.msg.area("1")
print a.can_post
n = a.post("Hi", "x")
)");
    EXPECT_THAT(out, HasSubstr("FALSE\r\n"));
    EXPECT_THAT(out, HasSubstr("message_area.post: " + error));
  };
  fake->poster_.restrict_post = true;
  expect_error("you aren't allowed to post");
  fake->poster_.restrict_post = false;

  fake->poster_.posts_today = 10;
  expect_error("you've posted as many messages as you may today");
  fake->poster_.posts_today = 0;

  fake->can_post_ = false;
  expect_error("you can't post in 'General'");
  fake->can_post_ = true;

  fake->poster_.restrict_net = true;
  subs->sub(0).nets.push_back(subboard_network_data_t{});
  expect_error("you can't post in networked areas such as 'General'");
}

TEST_F(WwivMsgWriteTest, Post_BadArguments) {
  EXPECT_THAT(Run("n = wwiv.msg.area(\"1\").post(\"  \", \"x\")\n"),
              HasSubstr("message_area.post: the title is empty"));
  EXPECT_THAT(Run("n = wwiv.msg.area(\"1\").post(STRING(72, \"x\"), \"x\")\n"),
              HasSubstr("message_area.post: the title is 72 characters; the most is 71"));
  EXPECT_THAT(Run("n = wwiv.msg.area(\"1\").post(\"Hi\", \"\")\n"),
              HasSubstr("message_area.post: the text is empty"));
  // A CR, LF or control character could end the RE:/BY: line and start a
  // control line of the script's own.
  EXPECT_THAT(Run("n = wwiv.msg.area(\"1\").post(\"Hi\", \"x\", \"Bob\" + CHR(13) + CHR(10) + "
                  "CHR(4) + \"0MSGID: 1\")\n"),
              HasSubstr("message_area.post: the name it's to has a control character (code 13)"));
  EXPECT_THAT(Run("n = wwiv.msg.area(\"1\").post(\"Hi\" + CHR(4), \"x\")\n"),
              HasSubstr("message_area.post: the title has a control character (code 4)"));
  EXPECT_THAT(Run("n = wwiv.msg.area(\"1\").post(\"Hi\", \"x\", STRING(72, \"b\"))\n"),
              HasSubstr("message_area.post: the name it's to is 72 characters; the most is 71"));
  EXPECT_THAT(Run("n = wwiv.msg.area(\"1\").post(\"Hi\")\n"),
              HasSubstr("message_area.post: expected 2 to 4 arguments, got 1"));
}

// Anonymous posts follow the area's setting and the user's rights, as in
// the BBS's post().
TEST_F(WwivMsgWriteTest, Anonymous) {
  const auto script = R"(
a = wwiv.msg.area("1")
print a.anonymous
n = a.post("Hi", "x", "", TRUE)
print a.message(n).anonymous; " "; a.message(n).from
)";
  // Not allowed.
  EXPECT_THAT(Run(script),
              HasSubstr("no\r\nmsgtest:4: error: message_area.post: posts in 'General' can't "
                        "be anonymous"));

  // The area allows it; the reader can't see who it was.
  subs->sub(0).anony = anony_enable_anony;
  EXPECT_EQ("allowed\r\nTRUE >UNKNOWN<\r\n", Run(script));

  // The user may not post anonymously.
  fake->poster_.restrict_anony = true;
  EXPECT_THAT(Run(script), HasSubstr("can't be anonymous"));
  fake->poster_.restrict_anony = false;

  // The user's SL may post anonymously in areas that don't say otherwise.
  subs->sub(0).anony = 0;
  fake->poster_.sl_post_anony = true;
  EXPECT_THAT(Run(script), HasSubstr("allowed\r\nTRUE"));
  fake->poster_.sl_post_anony = false;

  // Forced: anonymous even when the script doesn't ask.
  subs->sub(0).anony = anony_force_anony;
  EXPECT_EQ("forced\r\nTRUE >UNKNOWN<\r\n",
            Run("a = wwiv.msg.area(\"1\")\nprint a.anonymous\nn = a.post(\"Hi\", \"x\")\n"
                "print a.message(n).anonymous; \" \"; a.message(n).from\n"));

  // Real names: signed with the user's real name.
  subs->sub(0).anony = anony_real_name;
  EXPECT_EQ("no\r\nReal Name\r\n",
            Run("a = wwiv.msg.area(\"1\")\nprint a.anonymous\nn = a.post(\"Hi\", \"x\")\n"
                "print a.message(n).from\n"));

  // Dear Abby: TRUE posts as Abby.
  subs->sub(0).anony = anony_enable_dear_abby;
  EXPECT_THAT(Run(script), HasSubstr("allowed\r\nTRUE Abby"));

  // Networked areas don't allow it.
  subs->sub(0).anony = anony_enable_anony;
  subs->sub(0).nets.push_back(subboard_network_data_t{});
  EXPECT_EQ("no\r\n", Run("print wwiv.msg.area(\"1\").anonymous\n"));
}

TEST_F(WwivMsgWriteTest, DeleteOwnPost) {
  Post(0, "TESTER #1", "Mine", "x\r\n");
  Post(0, "TESTER #1", "Also mine", "y\r\n");
  const auto out = Run(R"(
a = wwiv.msg.area("1")
h = get(a.headers(), 0)
print a.delete_message(h); " "; a.count; " "; get(a.headers(), 0).title
print a.delete_message(h)
)");
  EXPECT_EQ("TRUE 1 Also mine\r\nFALSE\r\n", out);
  EXPECT_THAT(fake->deleted_, ElementsAre("0:Mine:1"));
}

TEST_F(WwivMsgWriteTest, DeleteSomeoneElsesPost) {
  Post(0, "TESTER #1", "Theirs", "x\r\n");
  fake->poster_.user_num = 2;
  const auto script = "a = wwiv.msg.area(\"1\")\nprint a.delete_message(get(a.headers(), 0))\n";
  EXPECT_THAT(
      Run(script),
      HasSubstr("message_area.delete_message: you can only delete your own posts in 'General'"));

  // A co-sysop of the area can.
  fake->cosysop_ = true;
  EXPECT_EQ("TRUE\r\n", Run(script));
}

// The posting example in wwivbasic/docs/BINDINGS.md.
TEST_F(WwivMsgWriteTest, DocExample) {
  EXPECT_EQ("Posted message 1\r\n", Run(R"bas(
a = wwiv.msg.area("1")
If a.can_post Then
  n = a.post("Hello", list("First line", "Second line"))
  print "Posted message "; n
EndIf
)bas"));
}

// WRITE_POST and REPLY: the user writes the post in the BBS's message editor
// (the fake's edit_post here).
TEST_F(WwivMsgWriteTest, WritePost) {
  fake->edited_ = MessageBase::EditedPost{"Typed title", "TESTER #1",
                                          "\x04"
                                          "0PID: WWIV x\r\nHello there\r\n",
                                          0};
  const auto out = Run(R"(
a = wwiv.msg.area("1")
n = a.write_post()
m = a.message(n)
print n; " "; m.title; " / "; m.from
for line in m.lines()
  print "["; line; "]"
next
)");
  EXPECT_EQ("1 Typed title / TESTER #1\r\n[Hello there]\r\n", out);
  // No title, so the editor asks; no reply title; to everyone; not anonymous.
  EXPECT_THAT(fake->edits_, ElementsAre("0||||0"));
  EXPECT_THAT(fake->posted_, ElementsAre("0:Typed title"));
}

TEST_F(WwivMsgWriteTest, WritePost_TitleAndTo) {
  fake->edited_ = MessageBase::EditedPost{"Given", "TESTER #1", "x\r\n", 0};
  EXPECT_EQ("1\r\n", Run("print wwiv.msg.area(\"1\").write_post(\"Given\", \"Bob\")\n"));
  EXPECT_THAT(fake->edits_, ElementsAre("0|Given||Bob|0"));
}

TEST_F(WwivMsgWriteTest, WritePost_Aborted) {
  EXPECT_EQ("0 0\r\n", Run(R"(
a = wwiv.msg.area("1")
print a.write_post(); " "; a.count
)"));
  EXPECT_TRUE(fake->posted_.empty());
}

// The editor asks about anonymity where the user may choose, and the post
// is saved as the user chose.
TEST_F(WwivMsgWriteTest, WritePost_Anonymous) {
  subs->sub(0).anony = anony_enable_anony;
  fake->edited_ = MessageBase::EditedPost{"Secret", "TESTER #1", "x\r\n", anony_sender};
  EXPECT_EQ("TRUE >UNKNOWN<\r\n", Run(R"(
a = wwiv.msg.area("1")
m = a.message(a.write_post())
print m.anonymous; " "; m.from
)"));
  EXPECT_THAT(fake->edits_, ElementsAre(fmt::format("0||||{}", anony_enable_anony)));
}

// A reply is to the post's author, with its title offered.
TEST_F(WwivMsgWriteTest, Reply) {
  Post(0, "rushfan #1", "Hello", "Hi there\r\n");
  fake->edited_ = MessageBase::EditedPost{"Re: Hello", "TESTER #1", "Hi back\r\n", 0};
  EXPECT_EQ("2 Re: Hello\r\n", Run(R"(
a = wwiv.msg.area("1")
n = a.reply(get(a.headers(), 0))
print n; " "; a.message(n).title
)"));
  EXPECT_THAT(fake->edits_, ElementsAre("0||Hello|rushfan #1|0"));
  // The editor gets the post to quote and thread the reply under.
  ASSERT_TRUE(fake->reply_.has_value());
  EXPECT_EQ("rushfan #1", fake->reply_->name);
  EXPECT_THAT(fake->reply_->text, HasSubstr("Hi there"));
  EXPECT_EQ(kDaten, fake->reply_->daten);
}

// A post that's gone can still be replied to, without quoting it.
TEST_F(WwivMsgWriteTest, Reply_ToADeletedPost) {
  Post(0, "rushfan #1", "Hello", "Hi there\r\n");
  fake->edited_ = MessageBase::EditedPost{"Re: Hello", "TESTER #1", "Hi back\r\n", 0};
  DeletePostBeforeFind(1);
  EXPECT_EQ("1\r\n", Run(R"(
a = wwiv.msg.area("1")
print a.reply(get(a.headers(), 0))
)"));
  EXPECT_THAT(fake->edits_, ElementsAre("0||Hello|rushfan #1|0"));
  EXPECT_FALSE(fake->reply_.has_value());
}

// The author's name and title come from a post, not the script, so they're
// made to fit rather than stopping the script.
TEST_F(WwivMsgWriteTest, Reply_CleansTheAuthorsName) {
  const auto long_name = std::string(80, 'n');
  Post(0, long_name + "\x03", "Title\x01", "x\r\n");
  fake->edited_ = MessageBase::EditedPost{"Re: Title", "TESTER #1", "y\r\n", 0};
  EXPECT_EQ("TRUE\r\n", Run(R"(
a = wwiv.msg.area("1")
print a.reply(get(a.headers(), 0)) > 0
)"));
  ASSERT_EQ(1u, fake->edits_.size());
  EXPECT_EQ(fmt::format("0||Title|{}|0", std::string(71, 'n')), fake->edits_.front());
}

// The time spent writing is passed on to posted(), which gives it back if
// the BBS does that; a script's own text took none.
TEST_F(WwivMsgWriteTest, WritingTime) {
  fake->edited_ =
      MessageBase::EditedPost{"Typed", "TESTER #1", "x\r\n", 0, std::chrono::seconds(42)};
  Run("a = wwiv.msg.area(\"1\")\nn = a.write_post()\nn = a.post(\"Hi\", \"x\")\n");
  EXPECT_THAT(fake->writing_times_, ElementsAre(42, 0));
}

TEST_F(WwivMsgWriteTest, WritePost_Errors) {
  EXPECT_THAT(Run("n = wwiv.msg.area(\"1\").write_post(\"a\", \"b\", \"c\")\n"),
              HasSubstr("message_area.write_post: expected 0 to 2 arguments, got 3"));
  EXPECT_THAT(Run("n = wwiv.msg.area(\"1\").reply(1)\n"),
              HasSubstr("message_area.reply: expected a message_header or message"));
  // Errors name the method.
  EXPECT_THAT(Run("n = wwiv.msg.area(\"1\").write_post(\"Hi\" + CHR(4))\n"),
              HasSubstr("message_area.write_post: the title has a control character (code 4)"));
  fake->can_post_ = false;
  EXPECT_THAT(Run("n = wwiv.msg.area(\"1\").write_post()\n"),
              HasSubstr("message_area.write_post: you can't post in 'General'"));
  helper.config().script_package_msg_write_enabled(false);
  EXPECT_THAT(Run("n = wwiv.msg.area(\"1\").write_post()\n"),
              HasSubstr("message_area.write_post: posting from scripts isn't turned on"));
  EXPECT_TRUE(fake->edits_.empty());
}

// msgview.bas posting and replying in the caller's editor (the fake's here).
TEST_F(MsgViewTest, Reply) {
  helper.config().script_package_msg_write_enabled(true);
  Post(0, "rushfan #1", "Hello", "Hi there\r\n");
  msgs->edited_ = MessageBase::EditedPost{"Re: Hello", "a #1", "Hi back\r\n", 0};
  // Area 1, message 1, reply, quit the message, the area and the viewer.
  const auto out = RunViewer("1\r1\rRQQ\rQ\r");
  EXPECT_THAT(out, HasSubstr("[R]eply"));
  EXPECT_THAT(msgs->edits_, ElementsAre("0||Hello|rushfan #1|0"));
  EXPECT_THAT(msgs->posted_, ElementsAre("0:Re: Hello"));
  // Back at the list, which has the reply.
  EXPECT_THAT(out.substr(out.find("[R]eply")), HasSubstr("General (2 messages)"));
}

TEST_F(MsgViewTest, WritePost) {
  helper.config().script_package_msg_write_enabled(true);
  Post(0, "rushfan #1", "Hello", "Hi there\r\n");
  msgs->edited_ = MessageBase::EditedPost{"New post", "a #1", "Text\r\n", 0};
  // Area 1, write a post, quit the area and the viewer.
  const auto out = RunViewer("1\rW\rQ\rQ\r");
  EXPECT_THAT(out, HasSubstr("[W]rite a post"));
  EXPECT_THAT(msgs->posted_, ElementsAre("0:New post"));
  // The list shows the new post.
  EXPECT_THAT(out, HasSubstr("New post"));
}

// Without "Enable MSG Writing" the viewer only reads.
TEST_F(MsgViewTest, NoPostingWithoutWriting) {
  Post(0, "rushfan #1", "Hello", "Hi there\r\n");
  // R isn't a command, so it goes on, which leaves the only message.
  const auto out = RunViewer("1\r1\rRQ\rQ\r");
  EXPECT_THAT(out, Not(HasSubstr("[R]eply")));
  EXPECT_THAT(out, Not(HasSubstr("[W]rite")));
  EXPECT_TRUE(msgs->edits_.empty());
}

TEST_F(MsgViewFullScreenTest, Reply) {
  helper.config().script_package_msg_write_enabled(true);
  Post(0, "rushfan #1", "Hello", "Hi there\r\n");
  msgs->edited_ = MessageBase::EditedPost{"Re: Hello", "a #1", "Hi back\r\n", 0};
  // General, the first message, reply, then quit the message, the list and
  // the viewer.
  const auto out = RunViewer("\r\rrqqq");
  EXPECT_THAT(out, HasSubstr("Reply"));
  EXPECT_THAT(msgs->edits_, ElementsAre("0||Hello|rushfan #1|0"));
  EXPECT_THAT(msgs->posted_, ElementsAre("0:Re: Hello"));
  // Back in the list, which has the reply.
  EXPECT_THAT(out, HasSubstr("General (2 messages)"));
}

TEST_F(MsgViewFullScreenTest, WritePost) {
  helper.config().script_package_msg_write_enabled(true);
  Post(0, "rushfan #1", "Hello", "Hi there\r\n");
  msgs->edited_ = MessageBase::EditedPost{"New post", "a #1", "Text\r\n", 0};
  // General, post, then quit the list and the viewer.
  const auto out = RunViewer("\rpqq");
  EXPECT_THAT(out, HasSubstr("Post"));
  EXPECT_THAT(msgs->posted_, ElementsAre("0:New post"));
  EXPECT_THAT(out, HasSubstr("General (2 messages)"));
  EXPECT_THAT(out, HasSubstr("New post"));
}

// A caller who may post can write the first post in an empty area.
TEST_F(MsgViewTest, WritePost_EmptyArea) {
  helper.config().script_package_msg_write_enabled(true);
  msgs->edited_ = MessageBase::EditedPost{"First post", "a #1", "Text\r\n", 0};
  // Sysop (empty), write a post, quit the area and the viewer.
  const auto out = RunViewer("S\rW\rQ\rQ\r");
  EXPECT_THAT(out, Not(HasSubstr("There are no messages")));
  EXPECT_THAT(msgs->posted_, ElementsAre("1:First post"));
  EXPECT_THAT(out, HasSubstr("Sysop (1 messages)"));
}

TEST_F(MsgViewFullScreenTest, WritePost_EmptyArea) {
  helper.config().script_package_msg_write_enabled(true);
  msgs->edited_ = MessageBase::EditedPost{"First post", "a #1", "Text\r\n", 0};
  // Down to Sysop (empty), Enter, post, then quit the list and the viewer.
  const auto out = RunViewer(std::string(kDown) + "\rpqq");
  EXPECT_THAT(out, Not(HasSubstr("There are no messages")));
  EXPECT_THAT(msgs->posted_, ElementsAre("1:First post"));
  EXPECT_THAT(out, HasSubstr("Sysop (1 messages)"));
}

// Posting in a sub on a WWIVnet network, where the message base sends posts
// to the network directory.
class WwivMsgNetTest : public WwivMsgWriteTest {
protected:
  void SetUp() override {
    ASSERT_TRUE(helper.files().Mkdir("network"));
    net_dir_ = helper.files().TempDir() / "network";
    networks_.emplace_back(net::network_type_t::wwivnet, "TestNET", net_dir_, 1);
    WwivMsgWriteTest::SetUp();
    // We host the sub, so posts go to its (no) subscribers.
    subboard_network_data_t sn{};
    sn.stype = "GENERAL";
    subs->sub(0).nets.push_back(sn);
    subs->sub(0).anony = anony_val_net;
  }

  // Packets written to the network for sending.
  [[nodiscard]] int packets_sent() const {
    int count = 0;
    for (const auto& e : std::filesystem::directory_iterator(net_dir_)) {
      if (e.path().filename().string().starts_with("p1-")) {
        ++count;
      }
    }
    return count;
  }

  [[nodiscard]] bool pending(int n) {
    const auto m = Read(0, n);
    return m && m->header().pending_network();
  }

  std::filesystem::path net_dir_;
};

// In a sub that needs network validation, posts wait for the sysop, as in
// the BBS's post().
TEST_F(WwivMsgNetTest, ValNet_Held) {
  EXPECT_EQ("1\r\n", Run("print wwiv.msg.area(\"1\").post(\"Hi\", \"x\")\n"));
  EXPECT_TRUE(pending(1));
  EXPECT_EQ(0, packets_sent());
}

// A co-sysop's new posts go out right away, from POST or the editor.
TEST_F(WwivMsgNetTest, ValNet_CoSysopPostsAreSent) {
  fake->cosysop_ = true;
  EXPECT_EQ("1\r\n", Run("print wwiv.msg.area(\"1\").post(\"Hi\", \"x\")\n"));
  fake->edited_ = MessageBase::EditedPost{"Typed", "TESTER #1", "x\r\n", 0};
  EXPECT_EQ("2\r\n", Run("print wwiv.msg.area(\"1\").write_post()\n"));
  EXPECT_FALSE(pending(1));
  EXPECT_FALSE(pending(2));
  EXPECT_EQ(2, packets_sent());
}

// But a co-sysop's reply still waits.
TEST_F(WwivMsgNetTest, ValNet_CoSysopReplyHeld) {
  Post(0, "rushfan #1", "Hello", "Hi there\r\n");
  fake->cosysop_ = true;
  fake->edited_ = MessageBase::EditedPost{"Re: Hello", "TESTER #1", "Hi back\r\n", 0};
  EXPECT_EQ("2\r\n", Run("a = wwiv.msg.area(\"1\")\nprint a.reply(get(a.headers(), 0))\n"));
  EXPECT_TRUE(pending(2));
  EXPECT_EQ(0, packets_sent());
}

// Without network validation, every post goes out.
TEST_F(WwivMsgNetTest, NoValNet_Sent) {
  subs->sub(0).anony = 0;
  EXPECT_EQ("1\r\n", Run("print wwiv.msg.area(\"1\").post(\"Hi\", \"x\")\n"));
  EXPECT_FALSE(pending(1));
  EXPECT_EQ(1, packets_sent());
}
