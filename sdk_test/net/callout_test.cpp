/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#include "gtest/gtest.h"

#include "core/strings.h"
#include "core_test/file_helper.h"
#include "sdk/net/callout.h"
#include <string>

using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

class CalloutTest : public testing::Test {};

TEST_F(CalloutTest, SimpleLine) {
  net_call_out_rec con;
  const std::string line = "@1234 &";
  ASSERT_TRUE(ParseCalloutNetLine(line, &con));
  EXPECT_EQ(1234, con.sysnum);
}

TEST_F(CalloutTest, WithPassword) {
  net_call_out_rec con;
  const std::string line = "@1234 &* \"pass\"";
  ASSERT_TRUE(ParseCalloutNetLine(line, &con));
  EXPECT_EQ(1234, con.sysnum);
  EXPECT_NE(0, con.options & unused_options_dial_ten);
  EXPECT_NE(0, con.options & unused_options_sendback);
  EXPECT_EQ("pass", con.session_password);
}

TEST_F(CalloutTest, WithPassword_Bang) {
  net_call_out_rec con;
  const std::string line = "@1 & \"pass!\"";
  ASSERT_TRUE(ParseCalloutNetLine(line, &con));
  EXPECT_EQ(1, con.sysnum);
  EXPECT_NE(0, con.options & unused_options_sendback);
  EXPECT_EQ("pass!", con.session_password);
}

TEST_F(CalloutTest, OncePerDay) {
  net_call_out_rec con;
  const std::string line = "@1234 &!24* \"pass\"";
  ASSERT_TRUE(ParseCalloutNetLine(line, &con));
  EXPECT_EQ(1234, con.sysnum);
  EXPECT_NE(0, con.options & unused_options_dial_ten);
  EXPECT_NE(0, con.options & unused_options_sendback);
  EXPECT_EQ("pass", con.session_password);
}

TEST_F(CalloutTest, LotsOfOptions) {
  net_call_out_rec con;
  const std::string line = "@1234 &!24%21/60* \"pass\"";
  ASSERT_TRUE(ParseCalloutNetLine(line, &con));
  EXPECT_EQ(1234, con.sysnum);
  EXPECT_NE(0, con.options & unused_options_dial_ten);
  EXPECT_NE(0, con.options & unused_options_sendback);
  EXPECT_EQ("pass", con.session_password);
  EXPECT_EQ(21, con.macnum);
  EXPECT_EQ(60, con.call_every_x_minutes);
}

TEST_F(CalloutTest, MinMax) {
  net_call_out_rec con;
  const std::string line = "@1234 &(8)12* \"pass\"";
  ASSERT_TRUE(ParseCalloutNetLine(line, &con));
  EXPECT_EQ(1234, con.sysnum);
  EXPECT_NE(0, con.options & unused_options_dial_ten);
  EXPECT_NE(0, con.options & unused_options_sendback);
  EXPECT_EQ("pass", con.session_password);
  EXPECT_EQ(8, con.min_hr);
  EXPECT_EQ(12, con.max_hr);
}

TEST_F(CalloutTest, EveryWeekWith10k) {
  net_call_out_rec con;
  const std::string line = "@1234 &*|10 \"pass\"";
  ASSERT_TRUE(ParseCalloutNetLine(line, &con));
  EXPECT_EQ(1234, con.sysnum);
  EXPECT_NE(0, con.options & unused_options_dial_ten);
  EXPECT_NE(0, con.options & unused_options_sendback);
  EXPECT_EQ("pass", con.session_password);
  EXPECT_EQ(10, con.min_k);
}

TEST_F(CalloutTest, InvalidLine) {
  net_call_out_rec con;

  const std::string line = "*@1234 &&";
  ASSERT_FALSE(ParseCalloutNetLine(line, &con));
}

TEST_F(CalloutTest, NodeConfig) {
  FileHelper files;
  EXPECT_TRUE(files.Mkdir("network"));
  const std::string line("@1 \"foo\"");
  files.CreateTempFile("network/callout.net", line);
  Network net{};
  net.name = "Dummy Network";
  net.dir = files.DirName("network");
  const Callout callout(net, 0);
  const auto* con = callout.net_call_out_for(1);
  ASSERT_TRUE(con != nullptr);
  EXPECT_EQ("foo", con->session_password);
}
