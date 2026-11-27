/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2026, WWIV Software Services           */
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
#include "common/datetime.h"

#include "gtest/gtest.h"
#include <chrono>
#include <ctime>

using namespace std::chrono;
using namespace wwiv::common;

// A local time today, built through mktime like the code under test.
static system_clock::time_point today_at(int hour, int min, int sec) {
  auto t = wwiv::core::DateTime::now().to_tm();
  t.tm_hour = hour;
  t.tm_min = min;
  t.tm_sec = sec;
  t.tm_isdst = -1;
  return system_clock::from_time_t(std::mktime(&t));
}

TEST(CommonDateTimeTest, DurationSinceMidnight) {
  EXPECT_EQ(hours(10) + minutes(30) + seconds(15),
            duration_since_midnight(today_at(10, 30, 15)));
  EXPECT_EQ(seconds(0), duration_since_midnight(today_at(0, 0, 0)));
}

TEST(CommonDateTimeTest, MinutesAfterMidnight) {
  EXPECT_EQ(today_at(13, 45, 0), minutes_after_midnight(13 * 60 + 45));
  EXPECT_EQ(today_at(0, 0, 0), minutes_after_midnight(0));
}
