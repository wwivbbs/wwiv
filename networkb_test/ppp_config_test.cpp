/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*                Copyright (C)2015 WWIV Software Services                */
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
