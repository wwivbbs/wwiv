/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*                Copyright (C)2015 WWIV Software Services                */
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
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "networkb/net_log.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

using std::clog;
using std::endl;
using std::string;
using std::thread;
using std::unique_ptr;
using namespace wwiv::net;
using namespace wwiv::strings;


class NetworkLogTest : public testing::Test {
public:
  NetworkLogTest() {
    helper_.Mkdir("gfiles");
    now_ = time(nullptr);

    struct tm* local = localtime(&now_);
    now_string_ = StringPrintf("%02d/%02d/%02d %02d:%02d:%02d",
      local->tm_mon + 1, local->tm_mday, local->tm_year % 100,
      local->tm_hour, local->tm_min, local->tm_sec);
  }
protected:
  FileHelper helper_;
  time_t now_;
  std::string now_string_;
};

TEST_F(NetworkLogTest, CreateLogLine_Fr_S1K_R2K_101s) {
  NetworkLog net_log(helper_.DirName("gfiles"));
  string actual = net_log.CreateLogLine(now_, NetworkSide::FROM, 12345, 1024 * 1024, 2048 * 1024, std::chrono::seconds(101), "rushnet");
  string expected = StringPrintf("%s Fr 12345, S:1024k, R:2048k            1.7 min  rushnet", now_string_.c_str());
  EXPECT_EQ(expected, actual);
}

TEST_F(NetworkLogTest, CreateLogLine_To_S1K_R2K_101s) {
  NetworkLog net_log(helper_.DirName("gfiles"));
  string actual = net_log.CreateLogLine(now_, NetworkSide::TO, 12345, 1024 * 1024, 2048 * 1024, std::chrono::seconds(101), "rushnet");
  string expected = StringPrintf("%s To 12345, S:1024k, R:2048k            1.7 min  rushnet", now_string_.c_str());
  EXPECT_EQ(expected, actual);
}

TEST_F(NetworkLogTest, CreateLogLine_Fr_S1K_R0K_101s) {
  NetworkLog net_log(helper_.DirName("gfiles"));
  string actual = net_log.CreateLogLine(now_, NetworkSide::FROM, 12345, 1024 * 1024, 0, std::chrono::seconds(101), "rushnet");
  string expected = StringPrintf("%s Fr 12345, S:1024k                     1.7 min  rushnet", now_string_.c_str());
  EXPECT_EQ(expected, actual);
}

TEST_F(NetworkLogTest, CreateLogLine_To_S1K_R0K_101s) {
  NetworkLog net_log(helper_.DirName("gfiles"));
  string actual = net_log.CreateLogLine(now_, NetworkSide::TO, 12345, 1024 * 1024, 0, std::chrono::seconds(101), "rushnet");
  string expected = StringPrintf("%s To 12345, S:1024k                     1.7 min  rushnet", now_string_.c_str());
  EXPECT_EQ(expected, actual);
}

TEST_F(NetworkLogTest, CreateLogLine_Fr_S0K_R3K_101s) {
  NetworkLog net_log(helper_.DirName("gfiles"));
  string actual = net_log.CreateLogLine(now_, NetworkSide::FROM, 12345, 0, 3072*1024, std::chrono::seconds(101), "rushnet");
  string expected = StringPrintf("%s Fr 12345,        , R:3072k            1.7 min  rushnet", now_string_.c_str());
  EXPECT_EQ(expected, actual);
}

TEST_F(NetworkLogTest, CreateLogLine_To_S0K_R3K_101s) {
  NetworkLog net_log(helper_.DirName("gfiles"));
  string actual = net_log.CreateLogLine(now_, NetworkSide::TO, 12345, 0, 3072 * 1024, std::chrono::seconds(101), "rushnet");
  string expected = StringPrintf("%s To 12345,        , R:3072k            1.7 min  rushnet", now_string_.c_str());
  EXPECT_EQ(expected, actual);
}
