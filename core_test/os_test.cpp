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

#include <cstdlib>
#include <iostream>
#include <string>

#include "core/os.h"

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

TEST(OsTest, EnvironmentVariable_Exists) {
  ASSERT_EQ(0, putenv("QWERTYUIOP=ASDF"));

  EXPECT_EQ("ASDF", environment_variable("QWERTYUIOP"));
}

TEST(OsTest, EnvironmentVariable_DoesNotExist) {
  EXPECT_EQ("", environment_variable("XXXQWERTYUIOP"));
}

TEST(OsTest, SetEnvironmentVariable) {
  ASSERT_EQ("ASDF", environment_variable("QWERTYUIOP"));
  ASSERT_TRUE(set_environment_variable("QWERTYUIOP", "ASDF"));
  ASSERT_EQ("ASDF", environment_variable("QWERTYUIOP"));
}
