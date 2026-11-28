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
#include "sdk/ssm.h"

#include "core/file.h"
#include "sdk/config.h"
#include "sdk/net/net.h"
#include "sdk/net/packets.h"
#include "sdk/sdk_helper.h"
#include "sdk/usermanager.h"
#include "gtest/gtest.h"

#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;

TEST(SsmTest, SendRemote_WritesPacketInNetworkDir) {
  SdkHelper helper;
  const auto net = helper.CreateTestNetwork(network_type_t::wwivnet);
  UserManager um(helper.config());
  SSM ssm(helper.config(), um);
  ASSERT_TRUE(ssm.send_remote(net, 5, 1, 2, "Hello"));

  File f(FilePath(net.dir, "p0.net"));
  ASSERT_TRUE(f.Open(File::modeReadOnly | File::modeBinary));
  auto [packet, response] = read_packet(f, false);
  ASSERT_EQ(ReadNetPacketResponse::OK, response);
  EXPECT_EQ(main_type_ssm, packet.nh.main_type);
  EXPECT_EQ(5, packet.nh.tosys);
  EXPECT_EQ(2, packet.nh.touser);
  EXPECT_EQ(net.sysnum, packet.nh.fromsys);
  EXPECT_EQ(1, packet.nh.fromuser);
  EXPECT_EQ("Hello", packet.text());
}
