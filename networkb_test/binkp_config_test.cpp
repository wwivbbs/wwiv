#include "gtest/gtest.h"
#include "core_test/file_helper.h"
#include "networkb/binkp_config.h"

#include <cstdint>
#include <string>

using std::string;
using wwiv::net::BinkConfig;
using wwiv::net::BinkNodeConfig;
using wwiv::net::ParseBinkConfigLine;
using wwiv::net::config_error;

class ParseBinkConfigLineTest : public testing::Test {};

class BinkConfigTest : public testing::Test {
protected:
   virtual void SetUp() {
     const ::testing::TestInfo* const test_info =
         ::testing::UnitTest::GetInstance()->current_test_info();
      test_name_ = test_info->name();
    }

    const string dir() { return files_.TempDir(); }
    string CreateConfigFile(const string& contents) {
      return files_.CreateTempFile(test_name_, contents);
    }

    FileHelper files_;
    std::string test_name_;
};

TEST_F(ParseBinkConfigLineTest, NoPort) {
  uint16_t node;
  BinkNodeConfig config;

  string line = "@1234 myhost -";
  ASSERT_TRUE(ParseBinkConfigLine(line, &node, &config));
  EXPECT_EQ(1234, node);
  EXPECT_STREQ("myhost", config.host.c_str());
  EXPECT_EQ(24554, config.port);
  EXPECT_STREQ("-", config.password.c_str());
}

TEST_F(ParseBinkConfigLineTest, Port) {
  uint16_t node;
  BinkNodeConfig config;

  string line = "@1234 myhost:2345 -";
  ASSERT_TRUE(ParseBinkConfigLine(line, &node, &config));
  EXPECT_EQ(1234, node);
  EXPECT_STREQ("myhost", config.host.c_str());
  EXPECT_EQ(2345, config.port);
  EXPECT_STREQ("-", config.password.c_str());
}

TEST_F(ParseBinkConfigLineTest, InvalidLine) {
  uint16_t node;
  BinkNodeConfig config;

  string line = "*@1234 myhost -";
  ASSERT_FALSE(ParseBinkConfigLine(line, &node, &config));
}

TEST_F(BinkConfigTest, Basic) {
  const string filename = CreateConfigFile("@1 example.com -\r\n""@2 foo.com:1234 welcome\r\n");
  BinkConfig config(filename);

  const BinkNodeConfig* one = config.node_config_for(1);
  ASSERT_STREQ("example.com", one->host.c_str());
  EXPECT_EQ(24554, one->port);
  EXPECT_STREQ("-", one->password.c_str());

  const BinkNodeConfig* two = config.node_config_for(2);
  ASSERT_STREQ("foo.com", two->host.c_str());
  EXPECT_EQ(1234, two->port);
  EXPECT_STREQ("welcome", two->password.c_str());
}

TEST_F(BinkConfigTest, BadConfigFile) {
  const string filename = CreateConfigFile("");
  try {
    BinkConfig config(filename + "badfilename");
    FAIL() << "config_error expected";
  } catch (config_error expected) {}
}
