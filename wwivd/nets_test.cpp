/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*             Copyright (C)2026, WWIV Software Services             */
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
#include "wwivd/nets.h"

#include "gtest/gtest.h"
#include <chrono>

using namespace std::chrono_literals;
using namespace wwiv::wwivd;

namespace {
const auto kStart = std::chrono::system_clock::time_point{} + 1000h;
}

TEST(BeginDayRetryTest, DoesNotRunWhenStatusIsToday) {
  const BeginDayRetry b;
  EXPECT_FALSE(b.should_run("11/27/26", "11/27/26", kStart));
}

TEST(BeginDayRetryTest, RunsOnANewDay) {
  const BeginDayRetry b;
  EXPECT_TRUE(b.should_run("11/27/26", "11/26/26", kStart));
}

// The command ran but status.dat still has yesterday: don't run it again on
// the next pass of the loop, only once the retry interval has gone by.
TEST(BeginDayRetryTest, WaitsBeforeRetryingTheSameDay) {
  BeginDayRetry b(1h);
  b.attempted("11/27/26", kStart);
  EXPECT_FALSE(b.should_run("11/27/26", "11/26/26", kStart + 5s));
  EXPECT_FALSE(b.should_run("11/27/26", "11/26/26", kStart + 59min));
  EXPECT_TRUE(b.should_run("11/27/26", "11/26/26", kStart + 1h));
}

// A failed attempt yesterday doesn't hold up today's run.
TEST(BeginDayRetryTest, NewDayRunsRightAwayAfterAFailedAttempt) {
  BeginDayRetry b(1h);
  b.attempted("11/27/26", kStart);
  EXPECT_TRUE(b.should_run("11/28/26", "11/26/26", kStart + 10min));
}

TEST(BeginDayRetryTest, DoesNotRunOnceStatusIsUpdated) {
  BeginDayRetry b(1h);
  b.attempted("11/27/26", kStart);
  EXPECT_FALSE(b.should_run("11/27/26", "11/27/26", kStart + 2h));
}
