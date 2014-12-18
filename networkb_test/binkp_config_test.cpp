#include "gtest/gtest.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "networkb/binkp_config.h"

#include <cstdint>
#include <string>

using std::string;
using namespace wwiv::net;
using namespace wwiv::strings;

class ParseBinkConfigLineTest : public testing::Test {};

class BinkConfigTest : public testing::Test {
protected:
   virtual void SetUp() {
     const ::testing::TestInfo* const test_info =
         ::testing::UnitTest::GetInstance()->current_test_info();
      test_name_ = test_info->name();
      ini_filename_ = CreateConfigFile("ini", "[NETWORK]\nNODE=1\nSYSTEM_NAME=Test BBS\n");
    }

    const string dir() { return files_.TempDir(); }
    string CreateConfigFile(const string& name, const string& contents) {
      return files_.CreateTempFile(StrCat(test_name_, "_", name), contents);
    }

    FileHelper files_;
    std::string test_name_;
    std::string ini_filename_;
};

TEST_F(ParseBinkConfigLineTest, NoPort) {
  uint16_t node;
  BinkNodeConfig config;

  string line = "@1234 myhost -";
  ASSERT_TRUE(ParseBinkConfigLine(line, &node, &config));
  EXPECT_EQ(1234, node);
  EXPECT_EQ("myhost", config.host);
  EXPECT_EQ(24554, config.port);
  EXPECT_EQ("-", config.password);
}

TEST_F(ParseBinkConfigLineTest, Port) {
  uint16_t node;
  BinkNodeConfig config;

  string line = "@1234 myhost:2345 -";
  ASSERT_TRUE(ParseBinkConfigLine(line, &node, &config));
  EXPECT_EQ(1234, node);
  EXPECT_EQ("myhost", config.host);
  EXPECT_EQ(2345, config.port);
  EXPECT_EQ("-", config.password);
}

TEST_F(ParseBinkConfigLineTest, InvalidLine) {
  uint16_t node;
  BinkNodeConfig config;

  string line = "*@1234 myhost -";
  ASSERT_FALSE(ParseBinkConfigLine(line, &node, &config));
}

TEST_F(BinkConfigTest, IniFile) {
}

TEST_F(BinkConfigTest, NodeConfig) {
}

TEST_F(BinkConfigTest, BadNodeConfigFile) {
}

TEST_F(BinkConfigTest, BadIniConfigFile) {
}
