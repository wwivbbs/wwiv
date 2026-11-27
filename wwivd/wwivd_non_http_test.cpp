/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                 Copyright (C)2018-2022, WWIV Software Services         */
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
#include "core/clock.h"
#include "core/fake_clock.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/test/file_helper.h"
#include "sdk/wwivd_config.h"
#include "wwivd/wwivd_non_http.h"

#include "gtest/gtest.h"
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::wwivd;

TEST(GoodIps, IsAlwaysAllowed) {
  const std::vector<std::string> lines{"10.0.0.1", "192.168.0.1 # This is line #2"};
  GoodIp ip(lines);
  EXPECT_TRUE(ip.IsAlwaysAllowed("10.0.0.1"));
  EXPECT_TRUE(ip.IsAlwaysAllowed("192.168.0.1"));

  EXPECT_FALSE(ip.IsAlwaysAllowed("10.0.0.2"));
}

TEST(BadIps, Smoke) {
  wwiv::core::test::FileHelper helper;
  auto fn = helper.CreateTempFile("badip.txt", "10.0.0.1\r\n8.8.8.8\r\n");
  FakeClock clock(DateTime::now());
  BadIp ip(fn, clock);
  EXPECT_TRUE(ip.IsBlocked("10.0.0.1"));
  EXPECT_TRUE(ip.IsBlocked("8.8.8.8"));
  EXPECT_FALSE(ip.IsBlocked("4.4.4.4"));
  ip.Block("1.1.1.1");
  EXPECT_TRUE(ip.IsBlocked("1.1.1.1"));

  TextFile tf(fn, "rt");
  auto contents = tf.ReadFileIntoString();
  EXPECT_TRUE(contents.find("1.1.1.1") != contents.npos);
  EXPECT_TRUE(ip.IsBlocked("10.0.0.1"));
  EXPECT_TRUE(ip.IsBlocked("8.8.8.8"));
  EXPECT_FALSE(ip.IsBlocked("4.4.4.4"));
}

// Connection threads check badip.txt while AutoBlocker adds to it.
TEST(BadIps, ConcurrentBlockAndCheck) {
  wwiv::core::test::FileHelper helper;
  const auto fn = helper.CreateTempFile("badip.txt", "10.0.0.1\r\n");
  FakeClock clock(DateTime::now());
  BadIp ip(fn, clock);
  constexpr int kWriters = 4;
  constexpr int kPerWriter = 50;
  std::atomic<bool> done{false};
  {
    std::vector<std::jthread> threads;
    for (auto r = 0; r < 4; r++) {
      threads.emplace_back([&] {
        while (!done) {
          EXPECT_TRUE(ip.IsBlocked("10.0.0.1"));
          (void)ip.IsBlocked("192.168.1.1");
        }
      });
    }
    std::vector<std::jthread> writers;
    for (auto w = 0; w < kWriters; w++) {
      writers.emplace_back([&, w] {
        for (auto i = 0; i < kPerWriter; i++) {
          ip.Block(StrCat("192.168.", w, ".", i));
        }
      });
    }
    writers.clear();
    done = true;
  }
  for (auto w = 0; w < kWriters; w++) {
    for (auto i = 0; i < kPerWriter; i++) {
      EXPECT_TRUE(ip.IsBlocked(StrCat("192.168.", w, ".", i)));
    }
  }
  // Each append is a whole line.
  TextFile tf(fn, "rt");
  const auto lines = tf.ReadFileIntoVector();
  ASSERT_EQ(1u + kWriters * kPerWriter, lines.size());
  for (const auto& line : lines) {
    EXPECT_TRUE(line == "10.0.0.1" || line.find(" # AutoBlocked by wwivd on: ") != line.npos)
        << line;
  }
}

TEST(AutoBlock, ShouldBlock) {
  wwivd_blocking_t b{};
  b.auto_blocklist = true;
  b.auto_bl_seconds = 2;
  b.auto_bl_sessions = 1;
  wwiv::core::test::FileHelper helper;
  const auto fn = helper.CreateTempFile("badip.txt", "10.0.0.1\r\n8.8.8.8\r\n");
  FakeClock clock(DateTime::now());
  auto bip = std::make_shared<BadIp>(fn, clock);
  AutoBlocker blocker(bip, b, helper.TempDir(), clock);
  EXPECT_FALSE(blocker.blocked("1.1.1.1"));
  blocker.Connection("1.1.1.1");
  clock.tick(1s);
  blocker.Connection("1.1.1.1");
  EXPECT_TRUE(blocker.blocked("1.1.1.1"));
  EXPECT_FALSE(bip->IsBlocked("1.1.1.1"));
}

TEST(AutoBlock, Escalate1) {
  wwivd_blocking_t b{};
  b.auto_blocklist = true;
  b.auto_bl_seconds = 2;
  b.auto_bl_sessions = 1;
  wwiv::core::test::FileHelper helper;
  const auto fn = helper.CreateTempFile("badip.txt", "10.0.0.1\r\n8.8.8.8\r\n");
  FakeClock clock(DateTime::now());
  auto bip = std::make_shared<BadIp>(fn, clock);
  AutoBlocker blocker(bip, b, helper.TempDir(), clock);
  EXPECT_FALSE(blocker.blocked("1.1.1.1"));
  blocker.Connection("1.1.1.1");
  clock.tick(1s);
  blocker.Connection("1.1.1.1");
  EXPECT_TRUE(blocker.blocked("1.1.1.1"));
  EXPECT_EQ(1, blocker.auto_blocked().at("1.1.1.1").count);
  // not blocked permanently yet.
  EXPECT_FALSE(bip->IsBlocked("1.1.1.1"));
}

TEST(AutoBlock, Escalate_All) {
  wwivd_blocking_t b{};
  b.auto_blocklist = true;
  b.auto_bl_seconds = 2;
  b.auto_bl_sessions = 1;
  b.block_duration.emplace_back("1m");
  b.block_duration.emplace_back("2m");
  b.block_duration.emplace_back("3m");
  b.block_duration.emplace_back("4m");
  wwiv::core::test::FileHelper helper;
  const auto fn = helper.CreateTempFile("badip.txt", "10.0.0.1\r\n8.8.8.8\r\n");
  FakeClock clock(DateTime::now());
  auto bip = std::make_shared<BadIp>(fn, clock);
  AutoBlocker blocker(bip, b, helper.TempDir(), clock);
  EXPECT_FALSE(blocker.blocked("1.1.1.1"));
  blocker.Connection("1.1.1.1");
  clock.tick(1s);
  blocker.Connection("1.1.1.1");
  EXPECT_TRUE(blocker.blocked("1.1.1.1"));
  EXPECT_EQ(1, blocker.auto_blocked().at("1.1.1.1").count);
  // not blocked permanently yet.
  EXPECT_FALSE(bip->IsBlocked("1.1.1.1"));

  clock.tick(61s);
  blocker.Connection("1.1.1.1");
  blocker.Connection("1.1.1.1");
  EXPECT_TRUE(blocker.blocked("1.1.1.1"));
  EXPECT_EQ(2, blocker.auto_blocked().at("1.1.1.1").count);
  // not blocked permanently yet.
  EXPECT_FALSE(bip->IsBlocked("1.1.1.1"));

  clock.tick(121s);
  blocker.Connection("1.1.1.1");
  blocker.Connection("1.1.1.1");
  EXPECT_TRUE(blocker.blocked("1.1.1.1"));
  EXPECT_EQ(3, blocker.auto_blocked().at("1.1.1.1").count);
  // not blocked permanently yet.
  EXPECT_FALSE(bip->IsBlocked("1.1.1.1"));

  clock.tick(181s);
  blocker.Connection("1.1.1.1");
  blocker.Connection("1.1.1.1");
  EXPECT_TRUE(blocker.blocked("1.1.1.1"));
  EXPECT_EQ(4, blocker.auto_blocked().at("1.1.1.1").count);
  // not blocked permanently yet.
  EXPECT_FALSE(bip->IsBlocked("1.1.1.1"));


  clock.tick(241s);
  blocker.Connection("1.1.1.1");
  blocker.Connection("1.1.1.1");
  EXPECT_FALSE(blocker.blocked("1.1.1.1"));
  // blocked permanently yet.
  EXPECT_TRUE(bip->IsBlocked("1.1.1.1"));
}

TEST(AutoBlock, ShouldNotBlock) {
  wwivd_blocking_t b{};
  b.auto_blocklist = true;
  b.auto_bl_seconds = 1;
  b.auto_bl_sessions = 1;
  wwiv::core::test::FileHelper helper;
  const auto fn = helper.CreateTempFile("badip.txt", "10.0.0.1\r\n8.8.8.8\r\n");
  FakeClock clock(DateTime::now());
  auto bip = std::make_shared<BadIp>(fn, clock);
  AutoBlocker blocker(bip, b, helper.TempDir(), clock);
  EXPECT_FALSE(bip->IsBlocked("1.1.1.1"));
  blocker.Connection("1.1.1.1");
  clock.tick(2s);
  blocker.Connection("1.1.1.1");
  EXPECT_FALSE(bip->IsBlocked("1.1.1.1"));
}

// Connection threads update the AutoBlocker while /blocking reads it.
TEST(AutoBlock, ConcurrentConnectionsAndReads) {
  wwivd_blocking_t b{};
  b.auto_blocklist = true;
  b.auto_bl_seconds = 60;
  b.auto_bl_sessions = 3;
  wwiv::core::test::FileHelper helper;
  const auto fn = helper.CreateTempFile("badip.txt", "");
  FakeClock clock(DateTime::now());
  auto bip = std::make_shared<BadIp>(fn, clock);
  AutoBlocker blocker(bip, b, helper.TempDir(), clock);
  std::atomic<bool> done{false};
  {
    std::jthread reader([&] {
      while (!done) {
        for (const auto& [ip, e] : blocker.auto_blocked()) {
          EXPECT_GT(e.count, 0) << ip;
        }
        (void)blocker.recent_sessions();
        (void)blocker.blocked("10.0.0.1");
      }
    });
    std::vector<std::jthread> callers;
    for (auto t = 0; t < 4; t++) {
      callers.emplace_back([&, t] {
        for (auto i = 0; i < 20; i++) {
          // The fake clock doesn't move, and connections in the same second
          // count once, so block each address directly too.
          const auto ip = StrCat("10.", t, ".0.", i);
          blocker.Connection(ip);
          blocker.escalate_block(ip);
          blocker.Connection(ip);
        }
      });
    }
    callers.clear();
    done = true;
  }
  EXPECT_EQ(80u, blocker.auto_blocked().size());
}

TEST(ColorTest, NoAnsi) {
  auto current = 7;
  EXPECT_EQ("", Color(14, false, current));
  EXPECT_EQ(7, current);
}

TEST(ColorTest, TracksTheCurrentColor) {
  auto current = 7;
  EXPECT_FALSE(Color(14, true, current).empty());
  EXPECT_EQ(14, current);
  // Same color: nothing to send.
  EXPECT_EQ("", Color(14, true, current));
  // From bold yellow to cyan, bold has to be turned off, which takes a reset.
  EXPECT_EQ("\x1b[0;36;40m", Color(3, true, current));
}

// Each caller has their own current color: one caller's menu doesn't
// change what's sent to another.
TEST(ColorTest, CallersDontShareState) {
  auto first = 7;
  auto second = 7;
  const auto expected = Color(14, true, first);
  EXPECT_EQ(expected, Color(14, true, second));
}
