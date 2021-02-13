/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                 Copyright (C)2018-2021, WWIV Software Services         */
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
#include "core/os.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core_test/file_helper.h"
#include "sdk/wwivd_config.h"
#include "wwivd/wwivd_non_http.h"
#include <chrono>
#include <string>
#include <vector>

using std::string;
using std::vector;
using namespace std::chrono_literals;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::wwivd;

TEST(GoodIps, IsAlwaysAllowed) {
  const vector<string> lines{"10.0.0.1", "192.168.0.1 # This is line #2"};
  GoodIp ip(lines);
  EXPECT_TRUE(ip.IsAlwaysAllowed("10.0.0.1"));
  EXPECT_TRUE(ip.IsAlwaysAllowed("192.168.0.1"));

  EXPECT_FALSE(ip.IsAlwaysAllowed("10.0.0.2"));
}

TEST(BadIps, Smoke) {
  FileHelper helper;
  auto fn = helper.CreateTempFile("badip.txt", "10.0.0.1\r\n8.8.8.8\r\n");
  BadIp ip(fn.string());
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

TEST(AutoBlock, ShouldBlock) {
  wwivd_blocking_t b{};
  b.auto_blocklist = true;
  b.auto_bl_seconds = 1;
  b.auto_bl_sessions = 1;
  FileHelper helper;
  const auto fn = helper.CreateTempFile("badip.txt", "10.0.0.1\r\n8.8.8.8\r\n");
  auto bip = std::make_shared<BadIp>(fn.string());
  AutoBlocker blocker(bip, b, helper.TempDir());
  EXPECT_FALSE(bip->IsBlocked("1.1.1.1"));
  blocker.Connection("1.1.1.1");
  wwiv::os::sleep_for(1s);
  blocker.Connection("1.1.1.1");
  EXPECT_TRUE(bip->IsBlocked("1.1.1.1"));
}

TEST(AutoBlock, ShouldNotBlock) {
  wwivd_blocking_t b{};
  b.auto_blocklist = true;
  b.auto_bl_seconds = 1;
  b.auto_bl_sessions = 1;
  FileHelper helper;
  const auto fn = helper.CreateTempFile("badip.txt", "10.0.0.1\r\n8.8.8.8\r\n");
  auto bip = std::make_shared<BadIp>(fn.string());
  AutoBlocker blocker(bip, b, helper.TempDir());
  EXPECT_FALSE(bip->IsBlocked("1.1.1.1"));
  blocker.Connection("1.1.1.1");
  wwiv::os::sleep_for(2s);
  blocker.Connection("1.1.1.1");
  EXPECT_FALSE(bip->IsBlocked("1.1.1.1"));
}