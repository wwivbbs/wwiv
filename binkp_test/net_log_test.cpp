/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2020, WWIV Software Services             */
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
#include "binkp/net_log.h"
#include "core/datetime.h"
#include "core/log.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "fmt/printf.h"
#include "gtest/gtest.h"
#include <chrono>
#include <string>
#include <thread>

using std::clog;
using std::endl;
using std::string;
using std::thread;
using std::unique_ptr;
using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::strings;

class NetworkLogTest : public testing::Test {
public:
  NetworkLogTest() {
    CHECK(helper_.Mkdir("gfiles"));
    const auto dt = DateTime::now();
    now_ = dt.to_time_t();
    now_string_ = dt.to_string("%m/%d/%y %H:%M:%S");
  }

protected:
  FileHelper helper_;
  time_t now_;
  std::string now_string_;
};

TEST_F(NetworkLogTest, CreateLogLine_Fr_S1K_R2K_101s) {
  const NetworkLog net_log(helper_.DirName("gfiles"));
  const auto actual = net_log.CreateLogLine(now_, NetworkSide::FROM, 12345, 1024 * 1024,
                                            2048 * 1024, std::chrono::seconds(101), "rushnet");
  const auto expected =
      fmt::format("{} Fr 12345, S:1024k, R:2048k            1.7 min  rushnet", now_string_);
  EXPECT_EQ(expected, actual);
}

TEST_F(NetworkLogTest, CreateLogLine_To_S1K_R2K_101s) {
  const NetworkLog net_log(helper_.DirName("gfiles"));
  const auto actual = net_log.CreateLogLine(now_, NetworkSide::TO, 12345, 1024 * 1024, 2048 * 1024,
                                            std::chrono::seconds(101), "rushnet");
  const auto expected =
      fmt::format("{} To 12345, S:1024k, R:2048k            1.7 min  rushnet", now_string_);
  EXPECT_EQ(expected, actual);
}

TEST_F(NetworkLogTest, CreateLogLine_Fr_S1K_R0K_101s) {
  const NetworkLog net_log(helper_.DirName("gfiles"));
  const auto actual = net_log.CreateLogLine(now_, NetworkSide::FROM, 12345, 1024 * 1024, 0,
                                            std::chrono::seconds(101), "rushnet");
  const auto expected =
      fmt::format("{} Fr 12345, S:1024k, R:   0k            1.7 min  rushnet", now_string_);
  EXPECT_EQ(expected, actual);
}

TEST_F(NetworkLogTest, CreateLogLine_To_S1K_R0K_101s) {
  const NetworkLog net_log(helper_.DirName("gfiles"));
  const auto actual = net_log.CreateLogLine(now_, NetworkSide::TO, 12345, 1024 * 1024, 0,
                                            std::chrono::seconds(101), "rushnet");
  const auto expected =
      fmt::format("{} To 12345, S:1024k, R:   0k            1.7 min  rushnet", now_string_);
  EXPECT_EQ(expected, actual);
}

TEST_F(NetworkLogTest, CreateLogLine_Fr_S0K_R3K_101s) {
  const NetworkLog net_log(helper_.DirName("gfiles"));
  const auto actual = net_log.CreateLogLine(now_, NetworkSide::FROM, 12345, 0, 3072 * 1024,
                                            std::chrono::seconds(101), "rushnet");
  const auto expected =
      fmt::format("{} Fr 12345, S:   0k, R:3072k            1.7 min  rushnet", now_string_);
  EXPECT_EQ(expected, actual);
}

TEST_F(NetworkLogTest, CreateLogLine_To_S0K_R3K_101s) {
  const NetworkLog net_log(helper_.DirName("gfiles"));
  const auto actual = net_log.CreateLogLine(now_, NetworkSide::TO, 12345, 0, 3072 * 1024,
                                            std::chrono::seconds(101), "rushnet");
  const auto expected =
      fmt::format("{} To 12345, S:   0k, R:3072k            1.7 min  rushnet", now_string_);
  EXPECT_EQ(expected, actual);
}
