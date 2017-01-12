/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2017, WWIV Software Services               */
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

#include "bbs/output.h"
#include "bbs/bbs.h"
#include "bbs_test/bbs_helper.h"
#include "core/strings.h"
#include "core_test/file_helper.h"

using std::cout;
using std::endl;
using std::string;

class BPutsTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        helper.SetUp();
    }

    virtual int Puts(const string& s) {
      auto size = bout.bputs(s);
      std::cerr << "Puts: [" << s << "]; size: " << size << "\r\n";
      return size;

    }

    BbsHelper helper;
};

TEST_F(BPutsTest, SingleLetter) {
  EXPECT_EQ(1, bout.bputs("A"));
  EXPECT_STREQ("A", helper.io()->captured().c_str());
}

TEST_F(BPutsTest, MultipleLetters) {
  const string kHelloWorld = "Hello World\r\n";
  EXPECT_EQ(kHelloWorld.size(), Puts(kHelloWorld));
  EXPECT_EQ(kHelloWorld, helper.io()->captured());
}

TEST_F(BPutsTest, SinglePipe) {
  const string kPlainHelloWorld = "Hello World\r\n";
  const string kAnsiHelloWorld = "\x1B[0;36;40;1mHello World\r\x1B[0;37;40m\n";
  const string s = "|#1Hello World\r\n";
  Puts(s);
  EXPECT_EQ(kPlainHelloWorld, helper.io()->captured());
  EXPECT_EQ(kAnsiHelloWorld, helper.io()->rcaptured());
}
