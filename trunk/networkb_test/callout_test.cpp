#include "gtest/gtest.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "networkb/callout.h"

#include <cstdint>
#include <string>

using std::string;
using namespace wwiv::net;
using namespace wwiv::strings;

class CalloutTest : public testing::Test {};

TEST_F(CalloutTest, SimpleLine) {
  net_call_out_rec con;
  string line = "@1234 &";
  ASSERT_TRUE(ParseCalloutNetLine(line, &con));
  EXPECT_EQ(1234, con.sysnum);
}

TEST_F(CalloutTest, WithPassword) {
  net_call_out_rec con;
  const string line = "@1234 &* \"pass\"";
  ASSERT_TRUE(ParseCalloutNetLine(line, &con));
  EXPECT_EQ(1234, con.sysnum);
  EXPECT_TRUE(con.options & options_dial_ten);
  EXPECT_TRUE(con.options & options_sendback);
  EXPECT_STREQ("pass", con.password);
}

TEST_F(CalloutTest, OncePerDay) {
  net_call_out_rec con;
  const string line = "@1234 &!24* \"pass\"";
  ASSERT_TRUE(ParseCalloutNetLine(line, &con));
  EXPECT_EQ(1234, con.sysnum);
  EXPECT_TRUE(con.options & options_dial_ten);
  EXPECT_TRUE(con.options & options_sendback);
  EXPECT_TRUE(con.options & options_once_per_day);
  EXPECT_STREQ("pass", con.password);
  EXPECT_EQ(24, con.times_per_day);
}

TEST_F(CalloutTest, LotsOfOptions) {
  net_call_out_rec con;
  const string line = "@1234 &!24%21/1* \"pass\"";
  ASSERT_TRUE(ParseCalloutNetLine(line, &con));
  EXPECT_EQ(1234, con.sysnum);
  EXPECT_TRUE(con.options & options_dial_ten);
  EXPECT_TRUE(con.options & options_sendback);
  EXPECT_TRUE(con.options & options_once_per_day);
  EXPECT_STREQ("pass", con.password);
  EXPECT_EQ(24, con.times_per_day);
  EXPECT_EQ(21, con.macnum);
  EXPECT_EQ(1, con.call_anyway);
}

TEST_F(CalloutTest, MinMax) {
  net_call_out_rec con;
  const string line = "@1234 &(8)12* \"pass\"";
  ASSERT_TRUE(ParseCalloutNetLine(line, &con));
  EXPECT_EQ(1234, con.sysnum);
  EXPECT_TRUE(con.options & options_dial_ten);
  EXPECT_TRUE(con.options & options_sendback);
  EXPECT_STREQ("pass", con.password);
  EXPECT_EQ(8, con.min_hr);
  EXPECT_EQ(12, con.max_hr);
}

TEST_F(CalloutTest, EveryWeekWith10k) {
  net_call_out_rec con;
  const string line = "@1234 &*#7|10 \"pass\"";
  ASSERT_TRUE(ParseCalloutNetLine(line, &con));
  EXPECT_EQ(1234, con.sysnum);
  EXPECT_TRUE(con.options & options_dial_ten);
  EXPECT_TRUE(con.options & options_sendback);
  EXPECT_STREQ("pass", con.password);
  EXPECT_EQ(7, con.call_x_days);
  EXPECT_EQ(10, con.min_k);
}

TEST_F(CalloutTest, InvalidLine) {
  net_call_out_rec con;

  string line = "*@1234 &&";
  ASSERT_FALSE(ParseCalloutNetLine(line, &con));
}

TEST_F(CalloutTest, NodeConfig) {
  FileHelper files;
  files.Mkdir("network");
  const string line("@1 & \"foo\"");
  files.CreateTempFile("network/callout.net", line);
  const string network_dir = files.DirName("network");
  Callout callout(network_dir);
  const net_call_out_rec* con = callout.node_config_for(1);
  ASSERT_TRUE(con != nullptr);
  EXPECT_EQ(options_sendback, con->options);
  EXPECT_STREQ("foo", con->password);
}
