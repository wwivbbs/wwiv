/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                  Copyright (C)2018, WWIV Software Services             */
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
#include "core/datetime.h"
#include "gtest/gtest.h"

#include "core/log.h"

#include <cstdlib>
#include <ctime>
#include <string>

using std::map;
using std::string;
using std::vector;

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace wwiv::core;

static std::string daten_to_mmddyyyy(daten_t n) {
  auto dt = DateTime::from_daten(n);
  return dt.to_string("%m/%d/%Y");
}

TEST(DateTime, Now) {
  auto start = DateTime::now();
  auto start_t = time(nullptr);
  EXPECT_LE(std::abs(start.to_time_t() - start_t), 1);
}

TEST(DateTime, ToDatenT) {
  auto start = DateTime::now();
  auto t = start.to_time_t();
  auto d = start.to_daten_t();
  EXPECT_EQ(static_cast<time_t>(d), t);
}

TEST(DateTime, ToSystemClock) {
  auto start = DateTime::now();
  auto now = system_clock::now();
  EXPECT_LE(std::abs(start.to_time_t() - system_clock::to_time_t(now)), 1);
}

TEST(DateTime, OperatorPlus) {
  auto start = DateTime::now();
  auto start_t = start.to_time_t();
  auto start_3s = start + 3s;
  EXPECT_EQ(start_3s.to_time_t(), start_t + 3);
}

TEST(DateTime, OperatorMinus) {
  auto start = DateTime::now();
  auto start_t = start.to_time_t();
  auto start_5s = start - 5s;
  EXPECT_EQ(start_5s.to_time_t(), start_t - 5);
}

TEST(DateTime, OperatorPlusEquals) {
  auto start = DateTime::now();
  auto start_t = start.to_time_t();
  start += 3s;
  EXPECT_EQ(start.to_time_t(), start_t + 3);
}

TEST(DateTime, OperatorMinusEquals) {
  auto start = DateTime::now();
  auto start_t = start.to_time_t();
  start -= 5s;
  EXPECT_EQ(start.to_time_t(), start_t - 5);
}

TEST(DateTime_Parsing, Parse_yyyymmdd_good_nondst) {
  auto dt = parse_yyyymmdd("2003-01-02");
  EXPECT_EQ(daten_to_mmddyyyy(dt.to_daten_t()), "01/02/2003");
  EXPECT_EQ(0, dt.hour());
  EXPECT_EQ(0, dt.minute());
  EXPECT_EQ(0, dt.second());
}

TEST(DateTime_Parsing, Parse_yyyymmdd_good_dst) {
  auto dt = parse_yyyymmdd("2003-06-07");
  EXPECT_EQ(daten_to_mmddyyyy(dt.to_daten_t()), "06/07/2003");
  EXPECT_EQ(0, dt.hour());
  EXPECT_EQ(0, dt.minute());
  EXPECT_EQ(0, dt.second());
}

TEST(DateTime_Parsing, Parse_yyyymmdd_with_optional_hms_good) {
  auto dt = parse_yyyymmdd_with_optional_hms("2003-01-02 01:02:03");
  EXPECT_EQ(daten_to_mmddyyyy(dt.to_daten_t()), "01/02/2003");
  EXPECT_EQ(1, dt.hour());
  EXPECT_EQ(2, dt.minute());
  EXPECT_EQ(3, dt.second());
}

TEST(DateTime_Parsing, Parse_yyyymmdd_with_optional_hms_good_dst) {
  auto dt = parse_yyyymmdd_with_optional_hms("2003-06-07 01:02:03");
  EXPECT_EQ(daten_to_mmddyyyy(dt.to_daten_t()), "06/07/2003");
  EXPECT_EQ(1, dt.hour());
  EXPECT_EQ(2, dt.minute());
  EXPECT_EQ(3, dt.second());
}

TEST(DateTime_Parsing, Parse_yyyymmdd_with_optional_hms_without_hms) {
  auto dt = parse_yyyymmdd_with_optional_hms("2003-04-05");
  EXPECT_EQ(daten_to_mmddyyyy(dt.to_daten_t()), "04/05/2003");
  EXPECT_EQ(0, dt.hour());
  EXPECT_EQ(0, dt.minute());
  EXPECT_EQ(0, dt.second());
}

TEST(DateTime_Parsing, Parse_yyyymmdd_fail) {
  auto dt = parse_yyyymmdd("2003-04-05x");
  EXPECT_NE(daten_to_mmddyyyy(dt.to_daten_t()), "04/05/2003");
}

TEST(DateTime, Plus_Duration) {
  const auto t1 = DateTime::now();
  const auto t2s{t1 + 2s};
  auto t2{t1};
  t2 += 2s;

  ASSERT_EQ(2, t2s.to_time_t() - t1.to_time_t());
  ASSERT_EQ(2, t2.to_time_t() - t1.to_time_t());
}

TEST(DateTime, Minus_Duration) {
  const auto t1 = DateTime::now();
  const auto t2s{t1 - 2s};
  auto t2{t1};
  t2 -= 2s;

  ASSERT_EQ(2, t1.to_time_t() - t2s.to_time_t());
  ASSERT_EQ(2, t1.to_time_t() - t2.to_time_t());
}

TEST(DateTime, Comparisons) {
  const auto t1 = DateTime::now();
  auto t2{t1};
  t2 += 1s;
  const auto t1a{t1};

  EXPECT_TRUE(t1 == t1a);
  ASSERT_EQ(t1, t1a);

  EXPECT_FALSE(t1 == t2);
  EXPECT_NE(t1, t2);

  EXPECT_TRUE(t2 > t1);
  EXPECT_TRUE(t1 < t2);
  EXPECT_TRUE(t1 <= t2);
  EXPECT_TRUE(t1 <= t1a);
  EXPECT_TRUE(t2 >= t1);
  EXPECT_TRUE(t1 >= t1a);
}
