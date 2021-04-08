/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2021, WWIV Software Services               */
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

#include "bbs_test/bbs_helper.h"
#include "common/output.h"
#include "sdk/menus/menu_set.h"
#include <string>

class BPutchTest : public ::testing::Test {
protected:
  void SetUp() override { helper.SetUp(); }

  virtual int Puts(std::string s) {
    auto count = 0;
    for (const auto& c : s) {
      count += bout.bputch(c);
    }
    return count;
  }

  BbsHelper helper{};
};

TEST_F(BPutchTest, SingleLetter) {
  EXPECT_EQ(1, bout.bputch('A'));
  EXPECT_STREQ("A", helper.io()->captured().c_str());
}

TEST_F(BPutchTest, MultipleLetters) {
  const std::string kHelloWorld = "Hello World\r\n";
  EXPECT_EQ(wwiv::stl::ssize(kHelloWorld), Puts(kHelloWorld));
  EXPECT_EQ(kHelloWorld, helper.io()->captured());
}
