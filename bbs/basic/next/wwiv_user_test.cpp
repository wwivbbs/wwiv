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

#include "bbs/basic/next/bbs_user_base.h"
#include "bbs/basic/next/next_engine.h"
#include "bbs/basic/user_base.h"
#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/bbs_helper.h"
#include "common/input.h"
#include "common/output.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/config.h"
#include "sdk/names.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/vardec.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

using namespace wwiv::bbs::basic;
using namespace wwiv::sdk;
using namespace wwiv::strings;
using testing::HasSubstr;
using testing::Not;

namespace {

// Users kept in memory, with the caller's rights set by the test.
class FakeUserBase final : public UserBase {
public:
  [[nodiscard]] const User& current() const override { return current_; }
  [[nodiscard]] int current_number() const override { return current_number_; }
  [[nodiscard]] bool is_cosysop() const override { return cosysop_; }
  [[nodiscard]] std::optional<User> read(int user_number) const override {
    const auto it = users_.find(user_number);
    if (it == users_.end() || it->second.deleted()) {
      return std::nullopt;
    }
    return it->second;
  }
  [[nodiscard]] int find(const std::string& name) const override {
    for (const auto& [number, u] : users_) {
      if (iequals(u.name(), name)) {
        return number;
      }
    }
    return 0;
  }

  User current_{};
  int current_number_{1};
  bool cosysop_{false};
  std::map<int, User> users_;
};

User MakeUser(const std::string& name, daten_t last_on) {
  User u{};
  u.set_name(name);
  u.last_daten(last_on);
  return u;
}

} // namespace

class WwivUserTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    helper.config().scripting_enabled(true);
    users = std::make_unique<FakeUserBase>();
    users->current_ = MakeUser("RUSHFAN", kDaten);
    users->users_[1] = users->current_;
    users->users_[2] = MakeUser("SYSOP", kDaten - 60);
  }

  std::string Run(const std::string& text, bool* ok = nullptr) {
    NextEngine engine(bin, bout, helper.config(), &helper.context());
    engine.set_user_base(std::move(users));
    const auto r = engine.RunScript("usertest", text);
    if (ok) {
      *ok = r;
    }
    return helper.io()->captured();
  }

  static constexpr daten_t kDaten = 1700000000;
  BbsHelper helper{};
  std::unique_ptr<FakeUserBase> users;
};

TEST_F(WwivUserTest, ModuleName) {
  EXPECT_EQ("wwiv.user\r\n", Run("print wwiv.user.module_name()\n"));
}

TEST_F(WwivUserTest, EnabledByDefault) {
  EXPECT_TRUE(helper.config().script_package_user_enabled());
}

TEST_F(WwivUserTest, Disabled) {
  helper.config().script_package_user_enabled(false);
  bool ok{true};
  const auto out = Run("print wwiv.user.module_name()\nprint \"after\"\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, Not(HasSubstr("after")));
}

// Without a UserBase there's no wwiv.user module.
TEST_F(WwivUserTest, NoUserBase) {
  NextEngine engine(bin, bout, helper.config(), &helper.context());
  EXPECT_FALSE(engine.RunScript("usertest", "print wwiv.user.module_name()\nprint \"after\"\n"));
  EXPECT_THAT(helper.io()->captured(), Not(HasSubstr("after")));
}

TEST_F(WwivUserTest, Current) {
  auto& u = users->current_;
  u.real_name("Rob");
  u.sl(200);
  u.dsl(100);
  u.ar_int(1 | 4);  // A and C
  u.dar_int(2);     // B
  u.logons(42);
  u.messages_posted(7);
  u.uploaded(3);
  u.downloaded(5);
  EXPECT_EQ("1/RUSHFAN/Rob/200/100/AC/B/FALSE/FALSE\r\n42/7/3/5/1700000000\r\n", Run(R"(
u = wwiv.user.current()
print u.number; "/"; u.name; "/"; u.real_name; "/"; u.sl; "/"; u.dsl; "/"; u.ar; "/"; u.dar; "/"; u.sysop; "/"; u.cosysop
print u.logons; "/"; u.posts; "/"; u.uploads; "/"; u.downloads; "/"; u.last_on
)"));
}

TEST_F(WwivUserTest, Current_NoRealName) {
  EXPECT_EQ("[]\r\n", Run("print \"[\"; wwiv.user.current().real_name; \"]\"\n"));
}

TEST_F(WwivUserTest, Current_Sysop) {
  users->current_.sl(255);
  EXPECT_EQ("TRUE/FALSE\r\n", Run(R"(
u = wwiv.user.current()
print u.sysop; "/"; u.cosysop
)"));
}

TEST_F(WwivUserTest, Current_CoSysop) {
  users->cosysop_ = true;
  EXPECT_EQ("FALSE/TRUE\r\n", Run(R"(
u = wwiv.user.current()
print u.sysop; "/"; u.cosysop
)"));
}

TEST_F(WwivUserTest, Find) {
  EXPECT_EQ("2/2/0\r\n", Run(R"(
print wwiv.user.find("SYSOP"); "/"; wwiv.user.find("sysop"); "/"; wwiv.user.find("nobody")
)"));
}

TEST_F(WwivUserTest, User) {
  EXPECT_EQ("2/SYSOP/1699999940\r\n", Run(R"(
u = wwiv.user.user(2)
print u.number; "/"; u.name; "/"; u.last_on
)"));
}

TEST_F(WwivUserTest, User_None) {
  bool ok{true};
  const auto out = Run("u = wwiv.user.user(9)\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("USER: there is no user 9"));
}

TEST_F(WwivUserTest, User_Deleted) {
  users->users_[2].set_inact(User::userDeleted);
  bool ok{true};
  const auto out = Run("u = wwiv.user.user(2)\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("USER: there is no user 2"));
}

// Another user's details are only the public ones.
TEST_F(WwivUserTest, User_NoPrivateDetails) {
  for (const auto* member : {"sl", "real_name", "ar", "logons"}) {
    SCOPED_TRACE(member);
    helper.io()->Clear();
    users = std::make_unique<FakeUserBase>();
    users->users_[2] = MakeUser("SYSOP", kDaten);
    users->users_[2].sl(77);
    bool ok{true};
    const auto out = Run(fmt::format("print wwiv.user.user(2).{}\n", member), &ok);
    EXPECT_FALSE(ok);
    EXPECT_THAT(out, Not(HasSubstr("77")));
  }
}

// BbsUserBase reads the BBS's user.lst and names.lst, with the rights the
// BBS's user.* expressions use.
class BbsUserBaseTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    // The test config has neither; writeuser quietly skips users past
    // max_users.
    helper.config().max_users(10);
    helper.config().raw_config().userreclen = sizeof(userrec);
    a()->init_users_for_test();
    a()->users()->set_user_writes_allowed(true);
    AddUser(2, "SYSOP");
    AddUser(3, "GONE");
    AddUser(4, "AWAY");
    auto away = a()->users()->readuser(4);
    ASSERT_TRUE(away);
    away->set_inact(User::userInactive);
    ASSERT_TRUE(a()->users()->writeuser(away, 4));
    auto u = a()->users()->readuser(3);
    ASSERT_TRUE(u);
    u->set_inact(User::userDeleted);
    ASSERT_TRUE(a()->users()->writeuser(u, 3));
  }

  void AddUser(int number, const std::string& name) {
    User u{};
    u.set_name(name);
    u.last_daten(1700000000);
    ASSERT_TRUE(a()->users()->writeuser(u, number));
    ASSERT_TRUE(a()->names()->Add(name, number));
  }

  BbsHelper helper{};
};

TEST_F(BbsUserBaseTest, Read) {
  const BbsUserBase users(*a());
  const auto u = users.read(2);
  ASSERT_TRUE(u);
  EXPECT_EQ("SYSOP", u->name());
  EXPECT_EQ(std::nullopt, users.read(3)) << "deleted";
  EXPECT_EQ(std::nullopt, users.read(4)) << "inactive";
  EXPECT_EQ(std::nullopt, users.read(0));
  EXPECT_EQ(std::nullopt, users.read(99));
}

TEST_F(BbsUserBaseTest, Find) {
  const BbsUserBase users(*a());
  EXPECT_EQ(2, users.find("sysop"));
  EXPECT_EQ(0, users.find("nobody"));
}

TEST_F(BbsUserBaseTest, Current) {
  helper.user()->set_name("RUSHFAN");
  helper.sess().user_num(1);
  const BbsUserBase users(*a());
  EXPECT_EQ("RUSHFAN", users.current().name());
  EXPECT_EQ(1, users.current_number());
}

TEST_F(BbsUserBaseTest, CoSysop) {
  slrec sl{};
  helper.config().sl(50, sl);
  sl.ability = ability_cosysop;
  helper.config().sl(80, sl);
  const BbsUserBase users(*a());

  helper.user()->sl(50);
  helper.sess().effective_sl(50);
  EXPECT_FALSE(users.is_cosysop());
  // A co-sysop through the effective SL (as when the sysop gives temporary
  // rights).
  helper.sess().effective_sl(80);
  EXPECT_TRUE(users.is_cosysop());
  helper.sess().effective_sl(50);
  helper.user()->sl(255);
  EXPECT_TRUE(users.is_cosysop());

  // Before the session's SL is set, the stored SL counts, as in user.cosysop.
  helper.sess().effective_sl(0);
  helper.user()->sl(80);
  EXPECT_TRUE(users.is_cosysop());
  helper.user()->sl(50);
  EXPECT_FALSE(users.is_cosysop());
}
