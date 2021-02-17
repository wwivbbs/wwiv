/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2018-2021, WWIV Software Services          */
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

#include "core/datetime.h"
#include "core/fake_clock.h"
#include "sdk/user.h"
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
  const auto dt = DateTime::from_daten(n);
  return dt.to_string("%m/%d/%Y");
}

TEST(DateTime, Now) {
  const auto start = DateTime::now();
  const auto start_t = time(nullptr);
  EXPECT_LE(std::abs(start.to_time_t() - start_t), 1);
}

TEST(DateTime, ToDatenT) {
  const auto start = DateTime::now();
  const auto t = start.to_time_t();
  const auto d = start.to_daten_t();
  EXPECT_EQ(static_cast<time_t>(d), t);
}

TEST(DateTime, ToSystemClock) {
  const auto start = DateTime::now();
  const auto now = system_clock::now();
  EXPECT_LE(std::abs(start.to_time_t() - system_clock::to_time_t(now)), 1);
}

TEST(DateTime, OperatorPlus) {
  const auto start = DateTime::now();
  const auto start_t = start.to_time_t();
  const auto start_3s = start + 3s;
  EXPECT_EQ(start_3s.to_time_t(), start_t + 3);
}

TEST(DateTime, OperatorMinus) {
  const auto start = DateTime::now();
  const auto start_t = start.to_time_t();
  const auto start_5s = start - 5s;
  EXPECT_EQ(start_5s.to_time_t(), start_t - 5);
}

TEST(DateTime, OperatorPlusEquals) {
  auto start = DateTime::now();
  const auto start_t = start.to_time_t();
  start += 3s;
  EXPECT_EQ(start.to_time_t(), start_t + 3);
}

TEST(DateTime, OperatorMinusEquals) {
  auto start = DateTime::now();
  const auto start_t = start.to_time_t();
  start -= 5s;
  EXPECT_EQ(start.to_time_t(), start_t - 5);
}

TEST(DateTime_Parsing, Parse_yyyymmdd_good_nondst) {
  const auto dt = parse_yyyymmdd("2003-01-02");
  EXPECT_EQ(daten_to_mmddyyyy(dt.to_daten_t()), "01/02/2003");
  EXPECT_EQ(12, dt.hour());
  EXPECT_EQ(0, dt.minute());
  EXPECT_EQ(0, dt.second());
}

TEST(DateTime_Parsing, Parse_yyyymmdd_good_dst) {
  const auto dt = parse_yyyymmdd("2003-06-07");
  EXPECT_EQ(daten_to_mmddyyyy(dt.to_daten_t()), "06/07/2003");
  EXPECT_EQ(12, dt.hour());
  EXPECT_EQ(0, dt.minute());
  EXPECT_EQ(0, dt.second());
}

TEST(DateTime_Parsing, Parse_yyyymmdd_with_optional_hms_good) {
  const auto dt = parse_yyyymmdd_with_optional_hms("2003-01-02 01:02:03");
  EXPECT_EQ(daten_to_mmddyyyy(dt.to_daten_t()), "01/02/2003");
  EXPECT_EQ(1, dt.hour());
  EXPECT_EQ(2, dt.minute());
  EXPECT_EQ(3, dt.second());
}

TEST(DateTime_Parsing, Parse_yyyymmdd_with_optional_hms_good_dst) {
  const auto dt = parse_yyyymmdd_with_optional_hms("2003-06-07 01:02:03");
  EXPECT_EQ(daten_to_mmddyyyy(dt.to_daten_t()), "06/07/2003");
  EXPECT_EQ(1, dt.hour());
  EXPECT_EQ(2, dt.minute());
  EXPECT_EQ(3, dt.second());
}

TEST(DateTime_Parsing, Parse_yyyymmdd_with_optional_hms_without_hms) {
  const auto dt = parse_yyyymmdd_with_optional_hms("2003-04-05");
  EXPECT_EQ(daten_to_mmddyyyy(dt.to_daten_t()), "04/05/2003");
  EXPECT_EQ(12, dt.hour());
  EXPECT_EQ(0, dt.minute());
  EXPECT_EQ(0, dt.second());
}

TEST(DateTime_Parsing, Parse_yyyymmdd_fail) {
  const auto dt = parse_yyyymmdd("2003-04-05x");
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

  EXPECT_GT(t2, t1);
  EXPECT_LT(t1, t2);
  EXPECT_LE(t1, t2);
  EXPECT_LE(t1, t1a) << "t1: " << t1.to_string() << "; t1a: " << t1a.to_string();
  EXPECT_GE(t2, t1);
  EXPECT_GE(t1, t1a);
}

TEST(DateTime, YearsOld_AfterBirthday) {
  tm t{};
  t.tm_mon = 9;
  t.tm_mday = 25;
  t.tm_year = 120;
  const auto now = DateTime::from_tm(&t);
  FakeClock clock(now);
  EXPECT_EQ(50, years_old(10, 24, 1970, clock));
}

TEST(DateTime, YearsOld_OnBirthday) {
  tm t{};
  t.tm_mon = 9;
  t.tm_mday = 25;
  t.tm_year = 120;
  const auto now = DateTime::from_tm(&t);
  FakeClock clock(now);
  EXPECT_EQ(50, years_old(10, 25, 1970, clock));
}

TEST(DateTime, YearsOld_BeforeBirthday) {
  tm t{};
  t.tm_mon = 9;
  t.tm_mday = 25;
  t.tm_year = 120;
  const auto now = DateTime::from_tm(&t);
  FakeClock clock(now);
  EXPECT_EQ(49, years_old(10, 26, 1970, clock));
}

TEST(DateTime, ParseTimeSpan_Second) {
  auto o = parse_time_span("2s");
  ASSERT_TRUE(o);

  EXPECT_EQ(std::chrono::seconds(2), o.value());
}

TEST(DateTime, ParseTimeSpan_Minute) {
  auto o = parse_time_span("3m");
  ASSERT_TRUE(o);

  EXPECT_EQ(std::chrono::minutes(3), o.value());
}

TEST(DateTime, ParseTimeSpan_Hour) {
  auto o = parse_time_span("4h");
  ASSERT_TRUE(o);

  EXPECT_EQ(std::chrono::hours(4), o.value());
}

TEST(DateTime, ParseTimeSpan_Day) {
  auto o = parse_time_span("5d");
  ASSERT_TRUE(o);

  EXPECT_EQ(std::chrono::hours(120), o.value());
}

TEST(DateTime, ParseTimeSpan_Invalid) {
  ASSERT_FALSE(parse_time_span(""));
  ASSERT_FALSE(parse_time_span("d1"));
  ASSERT_FALSE(parse_time_span("1q"));
  ASSERT_FALSE(parse_time_span("q"));
  ASSERT_FALSE(parse_time_span("s"));
  ASSERT_FALSE(parse_time_span("d"));
  ASSERT_FALSE(parse_time_span("h"));
  ASSERT_FALSE(parse_time_span("m"));
  ASSERT_FALSE(parse_time_span("-1s"));
}

