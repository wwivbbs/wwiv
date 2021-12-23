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
#include "core/test/file_helper.h"
#include "binkp/binkp_config.h"
#include "sdk/config.h"
#include <string>

using namespace wwiv::net;
using namespace wwiv::strings;
using namespace wwiv::sdk;

class ParseBinkConfigLineTest : public testing::Test {};

TEST_F(ParseBinkConfigLineTest, NoPort) {
  const std::string line = "@1234 myhost";
  auto r = ParseBinkConfigLine(line);
  ASSERT_TRUE(r.has_value());
  auto& [node, config] = r.value();
  EXPECT_EQ("1234", node);
  EXPECT_EQ("myhost", config.host);
  EXPECT_EQ(24554, config.port);
}

TEST_F(ParseBinkConfigLineTest, Port) {
  const std::string line = "@1234 myhost:2345";
  auto r = ParseBinkConfigLine(line);
  ASSERT_TRUE(r.has_value());
  auto& [node, config] = r.value();
  EXPECT_EQ("1234", node);
  EXPECT_EQ("myhost", config.host);
  EXPECT_EQ(2345, config.port);
}

TEST_F(ParseBinkConfigLineTest, InvalidLine) {
  const std::string line = "*@1234 myhost";
  const auto r = ParseBinkConfigLine(line);
  ASSERT_FALSE(r.has_value());
}

TEST(BinkConfigTest, NodeConfig) {
  wwiv::core::test::FileHelper files;
  ASSERT_TRUE(files.Mkdir("network"));
  const std::string line("@2 example.com");
  files.CreateTempFile("network/binkp.net", line);
  const auto network_dir = files.DirName("network");
  const Config wwiv_config(files.TempDir());
  const BinkConfig config(1, wwiv_config, network_dir);
  const auto* node_config = config.binkp_session_config_for(2);
  ASSERT_TRUE(node_config != nullptr);
  EXPECT_EQ("example.com", node_config->host);
  EXPECT_EQ(24554, node_config->port);
}
