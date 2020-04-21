/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2020, WWIV Software Services               */
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

#include "bbs/bbs.h"
#include "bbs/output.h"
#include "bbs_test/bbs_helper.h"
#include <iostream>
#include <string>

using std::cout;
using std::endl;
using std::string;

class BPutsTest : public ::testing::Test {
protected:
  void SetUp() override { helper.SetUp(); }

  virtual int Puts(const string& s) {
    const auto size = bout.bputs(s);
    std::cerr << "Puts: [" << s << "]; size: " << size << "\r\n";
    return size;
  }

  BbsHelper helper{};
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
  const string kPlainHelloWorld = "Hello World";
  const string kAnsiHelloWorld = "\x1B[0;36;40;1mHello World";
  const string s = "|#1Hello World";
  Puts(s);
  EXPECT_EQ(kPlainHelloWorld, helper.io()->captured());
  EXPECT_EQ(kAnsiHelloWorld, helper.io()->rcaptured());
}

TEST_F(BPutsTest, SinglePipe_WithEndLine) {
  const string kPlainHelloWorld = "Hello World\r\n";
  const string kAnsiHelloWorld = "\x1B[0;36;40;1mHello World\r\x1B[0;37;40m\n";
  // TODO(rushfan): Our wonky linux handling adds the \r before all \n, however
  // this causes deltas here. The output is OK, but just has an extra \r
  const string kAnsiHelloWorldUnix = "\x1B[0;36;40;1mHello World\r\x1B[0;37;40m\r\n";
  const string s = "|#1Hello World\r\n";
  Puts(s);
  EXPECT_EQ(kPlainHelloWorld, helper.io()->captured());
#ifdef _WIN32
  EXPECT_EQ(kAnsiHelloWorld, helper.io()->rcaptured());
#else
  EXPECT_EQ(kAnsiHelloWorldUnix, helper.io()->rcaptured());
#endif
}

TEST_F(BPutsTest, RepeatedPipe) {
  const string kPlainHelloWorld = "Hello World";
  const string kAnsiHelloWorld = "\x1B[0;36;40;1mHello World";
  const string s = "|#1Hello |#1World";
  Puts(s);
  EXPECT_EQ(kPlainHelloWorld, helper.io()->captured());
  EXPECT_EQ(kAnsiHelloWorld, helper.io()->rcaptured());
}

TEST_F(BPutsTest, RepeatedPipe_WithEndLine) {
  const string kPlainHelloWorld = "Hello World\r\n";
  const string kAnsiHelloWorld = "\x1B[0;36;40;1mHello World\r\x1B[0;37;40m\n";
  const string kAnsiHelloWorldUnix = "\x1B[0;36;40;1mHello World\r\x1B[0;37;40m\r\n";
  const string s = "|#1Hello |#1World\r\n";
  Puts(s);
  EXPECT_EQ(kPlainHelloWorld, helper.io()->captured());
#ifdef _WIN32
  EXPECT_EQ(kAnsiHelloWorld, helper.io()->rcaptured());
#else
  EXPECT_EQ(kAnsiHelloWorldUnix, helper.io()->rcaptured());
#endif
}
