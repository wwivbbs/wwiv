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
