/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2018-2020, WWIV Software Services             */
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
#include "core/log.h"

#include <map>
#include <string>
#include <vector>

using std::map;
using std::string;
using std::vector;

using namespace wwiv::core;

class TestAppender : public Appender {
public:
  TestAppender() : Appender() {}
  bool append(const std::string& message) override {
    log_lines.push_back(message);
    return true;
  }

  mutable std::vector<std::string> log_lines;
};

class LogTest : public ::testing::Test {
protected:
  void SetUp() override { 
    // Clear all of the loggers
    info = std::make_shared<TestAppender>();
    warning = std::make_shared<TestAppender>();
    Logger::config().log_to.clear();
    Logger::config().add_appender(LoggerLevel::info, info);
    Logger::config().add_appender(LoggerLevel::warning, warning); 

    timestamp.assign("2018-01-01 21:12:00,530 ");
    const auto fn = [this]() { return timestamp; };
    Logger::config().timestamp_fn_ = fn;
  }

  void TearDown() override { Logger::config().reset(); }

  std::shared_ptr<TestAppender> info;
  std::shared_ptr<TestAppender> warning;
  std::string timestamp;
};

TEST_F(LogTest, Smoke) {
  LOG(INFO) << "Hello World!";
  EXPECT_EQ(1, info->log_lines.size());
  EXPECT_EQ("2018-01-01 21:12:00,530 INFO  Hello World!", info->log_lines.front());
  EXPECT_TRUE(warning->log_lines.empty());
}
