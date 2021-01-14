/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2021, WWIV Software Services           */
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
#include "core/strings.h"
#include <iostream>
#include <string>

using std::string;
using namespace std::chrono;
using namespace wwiv::os;
using namespace wwiv::strings;


TEST(OsTest, WaitFor_PredicateTrue) {
  auto predicate = []() { return true; };
  const auto start = system_clock::now();
  const auto d = seconds(1);
  const auto is_predicate_true = wait_for(predicate, d);
  ASSERT_TRUE(system_clock::now() < start + d);
  EXPECT_TRUE(is_predicate_true);
}

TEST(OsTest, WaitFor_PredicateFalse) {
  auto predicate = []() { return false; };
  const auto start = system_clock::now();
  const auto d = milliseconds(100);
  const auto is_predicate_true = wait_for(predicate, d);
  ASSERT_TRUE(system_clock::now() >= start + d);
  EXPECT_FALSE(is_predicate_true);
}

TEST(OsTest, SleepFor) {
  const auto start = system_clock::now();
  const auto d = milliseconds(100);
  sleep_for(d);
  const auto now = system_clock::now();
  ASSERT_TRUE(now - start - d < d) << (now - start - d).count();
}

TEST(OsTest, EnvironmentVariable_Exists) {
  char s[81];
  strcpy(s, "QWERTYUIOP=ASDF2");
  ASSERT_EQ(0, putenv(s));

  EXPECT_EQ("ASDF2", environment_variable("QWERTYUIOP"));
}

TEST(OsTest, EnvironmentVariable_DoesNotExist) {
  string name = test_info_->name();
  StringUpperCase(&name);
  EXPECT_EQ("", environment_variable(name));
}

TEST(OsTest, SetEnvironmentVariable) {
  string name = test_info_->name();
  StringUpperCase(&name);
  // Clear the environment
  std::cout << name;
  ASSERT_EQ("", environment_variable(name));
  ASSERT_TRUE(set_environment_variable(name, "ASDF"));
  ASSERT_EQ("ASDF", environment_variable(name));
}
