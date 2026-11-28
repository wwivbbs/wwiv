/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2026, WWIV Software Services               */
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
#include "bbs/netsup.h"

#include "sdk/net/legacy_net.h"
#include "sdk/net/net.h"
#include "gtest/gtest.h"

#include <string>

using namespace wwiv::sdk::net;

class GateMsgFromNameTest : public ::testing::Test {
protected:
  GateMsgFromNameTest() {
    nh.fromsys = 100;
    nh.fromuser = 0;
  }

  static constexpr uint16_t kToSysnum = 5;
  net_header_rec nh{};
  Network wwivnet{network_type_t::wwivnet, "wwivnet", "", 2};
};

TEST_F(GateMsgFromNameTest, FromThisSystem_ReplacesSystemNumber) {
  EXPECT_EQ("SYSOP #1 @5\r\n", gate_msg_from_name(nh, "SYSOP #1 @1234", true, wwivnet, kToSysnum));
  EXPECT_EQ(kToSysnum, nh.fromsys);
}

TEST_F(GateMsgFromNameTest, FromThisSystem_KeepsTextAfterSystemNumber) {
  EXPECT_EQ("SYSOP #1 @5.wwivnet\r\n",
            gate_msg_from_name(nh, "SYSOP #1 @1234.wwivnet", true, wwivnet, kToSysnum));
}

TEST_F(GateMsgFromNameTest, FromThisSystem_NoAt) {
  EXPECT_EQ("SYSOP\r\n", gate_msg_from_name(nh, "SYSOP", true, wwivnet, kToSysnum));
  EXPECT_EQ(100, nh.fromsys);
}

TEST_F(GateMsgFromNameTest, Remote_DropsUserNumber) {
  EXPECT_EQ("Joe Bloggs@100.wwivnet\r\n",
            gate_msg_from_name(nh, "Joe Bloggs  #12", false, wwivnet, kToSysnum));
  EXPECT_EQ(kToSysnum, nh.fromsys);
  EXPECT_EQ(0, nh.fromuser);
}

TEST_F(GateMsgFromNameTest, Remote_WithFromUser_QuotesName) {
  nh.fromuser = 7;
  EXPECT_EQ("``Joe Bloggs`` #7@100.wwivnet\r\n",
            gate_msg_from_name(nh, "Joe Bloggs #7", false, wwivnet, kToSysnum));
  EXPECT_EQ(0, nh.fromuser);
}

TEST_F(GateMsgFromNameTest, Remote_AlreadyQuoted) {
  EXPECT_EQ("``Real Name``  joe@100.wwivnet\r\n",
            gate_msg_from_name(nh, "``Real Name`` joe", false, wwivnet, kToSysnum));
}

TEST_F(GateMsgFromNameTest, Remote_FromInternetGateway) {
  const Network internet{network_type_t::internet, "internet", "", 1};
  EXPECT_EQ("joe@example.com", gate_msg_from_name(nh, "joe@example.com", false, internet, kToSysnum));
}

// The quoted name used to be built in a 200 byte buffer, which a long name
// from the network overflowed.
TEST_F(GateMsgFromNameTest, Remote_LongName) {
  nh.fromuser = 7;
  const std::string name(199, 'A');
  EXPECT_EQ("``" + name + "`` #7@100.wwivnet\r\n",
            gate_msg_from_name(nh, name, false, wwivnet, kToSysnum));
}
