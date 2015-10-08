/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*           Copyright (C)2014-2015 WWIV Software Services                */
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

#include <iostream>
#include <memory>
#include <string>

#include "bbs/bputch.h"
#include "bbs/bbs.h"
#include "bbs_test/bbs_helper.h"
#include "core/strings.h"
#include "core_test/file_helper.h"

using std::cout;
using std::endl;
using std::string;

class BPutchFileTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        helper.SetUp();
    }

    virtual int Puts(string s) {
      int count = 0;
      for (const auto& c : s) {
        count += bputch(c);
      }
      return count;
    }

    BbsHelper helper;
};

TEST_F(BPutchFileTest, SingleLetter) {
  EXPECT_EQ(1, bputch('A'));
  EXPECT_STREQ("A", helper.io()->captured().c_str());
}

TEST_F(BPutchFileTest, MultipleLetters) {
  const string kHelloWorld = "Hello World\r\n";
  EXPECT_EQ(kHelloWorld.size(), Puts(kHelloWorld));
  EXPECT_EQ(kHelloWorld, helper.io()->captured());
}

TEST_F(BPutchFileTest, SinglePipe) {
  const string kHelloWorld = "Hello World\r\n";
  const string s = "|#1Hello World\r\n";
  EXPECT_EQ(kHelloWorld.size(), Puts(s));
  EXPECT_EQ(kHelloWorld, helper.io()->captured());
}
