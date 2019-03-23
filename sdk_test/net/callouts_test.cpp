/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*                Copyright (C)2017, WWIV Software Services               */
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
#include "core/datetime.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "sdk/net/callouts.h"
#include "gtest/gtest.h"

#include <cstdint>
#include <ctime>
#include <string>

using std::endl;
using std::string;
using std::unique_ptr;
using namespace std::chrono;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

static time_t to_time_t(int mon, int day, int hour, int min) {
  struct tm t {};
  t.tm_hour = hour;
  t.tm_min = min;
  t.tm_year = 110; // (2010)
  t.tm_mon = mon;
  t.tm_mday = day;
  return mktime(&t);
}

static time_t fake_time_t() { return to_time_t(0, 1, 6, 1); }

class CalloutsTest : public testing::Test {
public:
  CalloutsTest() : t_(fake_time_t()), dt_(DateTime::from_time_t(fake_time_t())) { c_.sysnum = 1; }

protected:
  FileHelper helper_;
  net_call_out_rec c_{};
  NetworkContact ncn_{1};
  time_t t_;
  DateTime dt_;
};

TEST_F(CalloutsTest, NoCall) {
  ASSERT_TRUE(allowed_to_call(c_, DateTime::now()));

  c_.options = options_no_call;
  ASSERT_FALSE(allowed_to_call(c_, DateTime::now()));
}

TEST_F(CalloutsTest, ByTime) {

  ASSERT_TRUE(allowed_to_call(c_, dt_));

  c_.min_hr = 5;
  c_.max_hr = 6;
  ASSERT_FALSE(allowed_to_call(c_, dt_));
  c_.min_hr = 0;
  c_.max_hr = 6;
  ASSERT_FALSE(allowed_to_call(c_, dt_));

  c_.min_hr = 6;
  c_.max_hr = 7;
  ASSERT_TRUE(allowed_to_call(c_, dt_));
  c_.min_hr = 0;
  c_.max_hr = 7;
  ASSERT_TRUE(allowed_to_call(c_, dt_));

  c_.min_hr = 6;
  c_.max_hr = 24;
  ASSERT_TRUE(allowed_to_call(c_, dt_));
  c_.min_hr = 0;
  c_.max_hr = 24;
  ASSERT_TRUE(allowed_to_call(c_, dt_));

  auto late = DateTime::from_time_t(to_time_t(dt_.month(), dt_.day(), 23, 2));
  c_.min_hr = 20;
  c_.max_hr = 2;
  ASSERT_TRUE(allowed_to_call(c_, dt_));
}

TEST_F(CalloutsTest, ShouldCall_CallAnyay_TimeToCall) {
  ASSERT_TRUE(allowed_to_call(c_, dt_));

  // Call every 239 minutes
  c_.call_every_x_minutes = 239;
  auto last_connect = dt_ - minutes(240);
  ncn_.AddConnect(last_connect.to_time_t(), 100, 0);

  EXPECT_TRUE(should_call(ncn_, c_, dt_));
}

TEST_F(CalloutsTest, ShouldCall_CallAnyay_NotTimeYet) {
  ASSERT_TRUE(allowed_to_call(c_, dt_));

  // Call every 239 minutes
  c_.call_every_x_minutes = 239;
  auto last_connect = dt_ - minutes(238);
  ncn_.AddConnect(last_connect.to_time_t(), 100, 0);

  EXPECT_FALSE(should_call(ncn_, c_, dt_));
}

TEST_F(CalloutsTest, ShouldCall_CallAnyay_MinK) {
  ASSERT_TRUE(allowed_to_call(c_, dt_));

  // Call every 239 minutes
  c_.call_every_x_minutes = 239;
  c_.min_k = 10;
  auto last_connect = dt_ - minutes(238);
  ncn_.AddConnect(last_connect.to_time_t(), 100, 0);
  ncn_.set_bytes_waiting((10 * 1024) + 1);

  EXPECT_TRUE(should_call(ncn_, c_, dt_));
}

TEST_F(CalloutsTest, ShouldCall_CallAnyay_MinKNotMet) {
  ASSERT_TRUE(allowed_to_call(c_, dt_));

  // Call every 239 minutes
  c_.call_every_x_minutes = 239;
  c_.min_k = 10;
  auto last_connect = dt_ - minutes(238);
  ncn_.AddConnect(last_connect.to_time_t(), 100, 0);
  ncn_.set_bytes_waiting(8 * 1024);

  EXPECT_FALSE(should_call(ncn_, c_, dt_));
}