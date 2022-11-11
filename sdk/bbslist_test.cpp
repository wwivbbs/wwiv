/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2022-2022, WWIV Software Services           */
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

#include "sdk/bbslist.h"
#include "sdk/net/legacy_net.h"
#include <string>

using namespace wwiv::sdk;

TEST(SdkBbsListTest, Smoke) {
  net_system_list_rec node_config{};
  int32_t reg_number;
  std::string line = "@513        *415-754-LNET #33600        !      [11259]  \"Mystic Rhythms\"";
  ASSERT_TRUE(ParseBbsListNetLine(line, &node_config, &reg_number));

  EXPECT_EQ(11259, reg_number);
  EXPECT_EQ(513, node_config.sysnum);
  EXPECT_STREQ("Mystic Rhythms", node_config.name); // truncated to 40 chars
  EXPECT_STREQ("415-754-LNET", node_config.phone);
  EXPECT_TRUE(node_config.other & other_direct);
}

TEST(SdkBbsListTest, TruncatedName) {
  net_system_list_rec node_config{};
  int32_t reg_number;
  std::string line =
      "@137 *999-999-INET #33600 ! [ ] \"Public Electronic Networked Information System\"";
  ASSERT_TRUE(ParseBbsListNetLine(line, &node_config, &reg_number));

  EXPECT_STREQ("Public Electronic Networked Information",
               node_config.name); // truncated to 40 chars
}
