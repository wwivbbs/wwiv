#include "gtest/gtest.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "networkb/ppp_config.h"

#include <cstdint>
#include <string>

using std::string;
using namespace wwiv::net;
using namespace wwiv::strings;

class ParsePPPConfigLineTest : public testing::Test {};

TEST_F(ParsePPPConfigLineTest, Basic) {
  uint16_t node;
  PPPNodeConfig config;

  string line = "@1234 addy@example.com";
  ASSERT_TRUE(ParseAddressNetLine(line, &node, &config));
  EXPECT_EQ(1234, node);
  EXPECT_EQ("addy@example.com", config.email_address);
}

TEST_F(ParsePPPConfigLineTest, InvalidLine) {
  uint16_t node;
  PPPNodeConfig config;

  string line = "*@1234 myhost -";
  ASSERT_FALSE(ParseAddressNetLine(line, &node, &config));
}

TEST(PPPConfigTest, NodeConfig) {
  FileHelper files;
  files.Mkdir("network");
  const string line("@2 foo@example.com");
  files.CreateTempFile("network/address.net", line);
  const string network_dir = files.DirName("network");
  PPPConfig config(1, "mybbs", network_dir);
  const PPPNodeConfig* node_config = config.node_config_for(2);
  ASSERT_TRUE(node_config != nullptr);
  EXPECT_EQ("foo@example.com", node_config->email_address);
}
