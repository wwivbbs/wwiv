#include "gtest/gtest.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "networkb/binkp_config.h"
#include "sdk/config.h"

#include <cstdint>
#include <string>

using std::string;
using namespace wwiv::net;
using namespace wwiv::strings;
using namespace wwiv::sdk;

class ParseBinkConfigLineTest : public testing::Test {};

TEST_F(ParseBinkConfigLineTest, NoPort) {
  uint16_t node;
  BinkNodeConfig config;

  string line = "@1234 myhost";
  ASSERT_TRUE(ParseBinkConfigLine(line, &node, &config));
  EXPECT_EQ(1234, node);
  EXPECT_EQ("myhost", config.host);
  EXPECT_EQ(24554, config.port);
}

TEST_F(ParseBinkConfigLineTest, Port) {
  uint16_t node;
  BinkNodeConfig config;

  string line = "@1234 myhost:2345";
  ASSERT_TRUE(ParseBinkConfigLine(line, &node, &config));
  EXPECT_EQ(1234, node);
  EXPECT_EQ("myhost", config.host);
  EXPECT_EQ(2345, config.port);
}

TEST_F(ParseBinkConfigLineTest, InvalidLine) {
  uint16_t node;
  BinkNodeConfig config;

  string line = "*@1234 myhost";
  ASSERT_FALSE(ParseBinkConfigLine(line, &node, &config));
}

TEST(BinkConfigTest, NodeConfig) {
  FileHelper files;
  files.Mkdir("network");
  const string line("@2 example.com");
  files.CreateTempFile("network/binkp.net", line);
  const string network_dir = files.DirName("network");
  Config wwiv_config;
  BinkConfig config(1, wwiv_config, network_dir);
  const BinkNodeConfig* node_config = config.node_config_for(2);
  ASSERT_TRUE(node_config != nullptr);
  EXPECT_EQ("example.com", node_config->host);
  EXPECT_EQ(24554, node_config->port);
}
