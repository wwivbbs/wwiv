/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*                 Copyright (C)2014, WWIV Software Services              */
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
#include "core/os.h"

#include <iostream>
#include <string>

using std::string;
using namespace std::chrono;
using namespace wwiv::os;

TEST(OsTest, WaitFor_PredicateTrue) {
  auto predicate = []() { return true; };
  auto start = system_clock::now();
  auto d = seconds(1);
  bool is_predicate_true = wait_for(predicate, d);
  ASSERT_TRUE(system_clock::now() < start + d);
  EXPECT_TRUE(is_predicate_true);
}

TEST(OsTest, WaitFor_PredicateFalse) {
  auto predicate = []() { return false; };
  auto start = system_clock::now();
  auto d = milliseconds(100);
  bool is_predicate_true = wait_for(predicate, d);
  ASSERT_TRUE(system_clock::now() >= start + d);
  EXPECT_FALSE(is_predicate_true);
}

TEST(OsTest, SleepFor) {
  auto start = system_clock::now();
  auto d = milliseconds(100);
  sleep_for(d);
  auto now = system_clock::now();
  ASSERT_TRUE(now - start - d < d) << (now - start - d).count();
}
