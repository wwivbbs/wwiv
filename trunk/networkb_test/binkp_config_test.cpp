#include "gtest/gtest.h"
#include "networkb/binkp_config.h"

#include <cstdint>
#include <string>

using std::string;
using wwiv::net::BinkConfig;
using wwiv::net::BinkNodeConfig;
using wwiv::net::config_error;

TEST(ParseBinkConfigLineTest, NoPort) {
  uint16_t node;
  BinkNodeConfig config;

  string line = "@1234 myhost -";
  ASSERT_TRUE(ParseBinkConfigLine(line, &node, &config));
  EXPECT_EQ(1234, node);
  EXPECT_STREQ("myhost", config.host.c_str());
  EXPECT_EQ(24554, config.port);
  EXPECT_STREQ("-", config.password.c_str());
}

TEST(ParseBinkConfigLineTest, Port) {
  uint16_t node;
  BinkNodeConfig config;

  string line = "@1234 myhost:2345 -";
  ASSERT_TRUE(ParseBinkConfigLine(line, &node, &config));
  EXPECT_EQ(1234, node);
  EXPECT_STREQ("myhost", config.host.c_str());
  EXPECT_EQ(2345, config.port);
  EXPECT_STREQ("-", config.password.c_str());
}

TEST(ParseBinkConfigLineTest, InvalidLine) {
  uint16_t node;
  BinkNodeConfig config;

  string line = "*@1234 myhost -";
  ASSERT_FALSE(ParseBinkConfigLine(line, &node, &config));
}
