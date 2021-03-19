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

#include "bbs/bbs.h"
#include "common/output.h"
#include "bbs_test/bbs_helper.h"
#include <iostream>
#include <string>

using std::cout;
using std::endl;
using std::string;

class BPutsTest : public ::testing::Test {
protected:
  void SetUp() override { helper.SetUp(); }

  virtual int Puts(const std::string& s) {
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
  EXPECT_EQ(wwiv::stl::ssize(kHelloWorld), Puts(kHelloWorld));
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

TEST_F(BPutsTest, IfPipe_Yes) {
  const std::string s1 = R"(|{if "user.sl >= 200", "|@N", "|@R"})";
  helper.user()->set_name("Rushfan");
  helper.user()->real_name("RealName");
  helper.user()->sl(200);
  helper.sess().effective_sl(200);
  bout.bputs(s1);
  EXPECT_EQ("Rushfan", helper.io()->captured());
}

TEST_F(BPutsTest, IfPipe_No) {
  const std::string s1 = R"(|{if "user.sl >= 200", "|@N", "|@R"})";
  helper.user()->set_name("Rushfan");
  helper.user()->real_name("RealName");
  helper.user()->sl(100);
  helper.sess().effective_sl(100);
  bout.bputs(s1);
  EXPECT_EQ("RealName", helper.io()->captured());
}

TEST_F(BPutsTest, IfPipe_Embedded) {
  const std::string s1 = R"(Hello |{if "user.sl >= 200", "|@N", "|@R"})";
  helper.user()->set_name("Rushfan");
  helper.user()->real_name("RealName");
  helper.user()->sl(200);
  helper.sess().effective_sl(200);
  bout.bputs(s1);
  EXPECT_EQ("Hello Rushfan", helper.io()->captured());
}

TEST_F(BPutsTest, MapValue_Smoke) {
  helper.user()->set_name("Rushfan");
  helper.user()->real_name("RealName");
  helper.user()->sl(200);
  helper.sess().effective_sl(200);
  helper.context().add_context_variable("m", {{"num", "1234"}});
  bout.bputs(R"(Message # |{m.num})");
  EXPECT_EQ("Message # 1234", helper.io()->captured());
}

TEST_F(BPutsTest, Pipe_User) {
  helper.user()->set_name("Rushfan");
  helper.user()->real_name("RealName");
  bout.bputs(R"(Hello |{user.name})");
  EXPECT_EQ("Hello Rushfan", helper.io()->captured());
}

TEST_F(BPutsTest, Pipe_BBS) {
  helper.user()->set_name("Rushfan");
  auto cfg = helper.config().to_config_t();
  cfg.systemname = "TestBBS";
  helper.config().set_config(cfg, false);
  bout.bputs(R"(BBS Name: |{bbs.name})");
  EXPECT_EQ("BBS Name: TestBBS", helper.io()->captured());
}
