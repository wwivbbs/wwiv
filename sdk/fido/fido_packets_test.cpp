/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2021-2022, WWIV Software Services             */
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

#include "core/file.h"
#include "core/test/file_helper.h"
#include "core/test/wwivtest.h"
#include "sdk/fido/fido_packets.h"
#include "gtest/gtest.h"

class FidoPacketsTestDataTest : public wwiv::core::test::TestDataTest {};

using namespace wwiv::core;
using namespace wwiv::core::test;
using namespace wwiv::sdk::fido;
using namespace wwiv::sdk::net;

TEST_F(FidoPacketsTestDataTest, OneMessage) {
  const auto path = FilePath(FileHelper::TestData(), "fido/0d73f767.pkt");
  auto f = File(path);
  ASSERT_TRUE(f.Exists()) << f;

  auto o = FidoPacket::Open(path);
  ASSERT_TRUE(o.has_value());
  auto& packet = o.value();
  EXPECT_EQ(packet.header().orig_zone, 21);
  EXPECT_EQ(packet.header().orig_net, 1);
  EXPECT_EQ(packet.header().orig_node, 2);

  EXPECT_EQ(packet.header().dest_zone, 21);
  EXPECT_EQ(packet.header().dest_net, 1);
  EXPECT_EQ(packet.header().dest_node, 1);

  {
    auto [result, msg] = packet.Read();
    ASSERT_EQ(ReadNetPacketResponse::OK, result);
    EXPECT_EQ("All", msg.vh.to_user_name);
    EXPECT_EQ("test 4", msg.vh.subject);
  }
  
  {
    auto [result, msg] = packet.Read();
    ASSERT_EQ(ReadNetPacketResponse::END_OF_FILE, result);
  }

}

TEST_F(FidoPacketsTestDataTest, ThreeMessages) {
  const auto path = FilePath(FileHelper::TestData(), "fido/0e7c5b69.pkt");
  auto f = File(path);
  ASSERT_TRUE(f.Exists()) << f;

  auto o = FidoPacket::Open(path);
  ASSERT_TRUE(o.has_value());
  auto& packet = o.value();
  EXPECT_EQ(packet.header().orig_zone, 21);
  EXPECT_EQ(packet.header().orig_net, 1);
  EXPECT_EQ(packet.header().orig_node, 2);

  EXPECT_EQ(packet.header().dest_zone, 21);
  EXPECT_EQ(packet.header().dest_net, 1);
  EXPECT_EQ(packet.header().dest_node, 1);

  {
    auto [result, msg] = packet.Read();
    ASSERT_EQ(ReadNetPacketResponse::OK, result);
    EXPECT_EQ("All", msg.vh.to_user_name);
    EXPECT_EQ("test5", msg.vh.subject);
  }

  {
    auto [result, msg] = packet.Read();
    ASSERT_EQ(ReadNetPacketResponse::OK, result);
    EXPECT_EQ("All", msg.vh.to_user_name);
    EXPECT_EQ("test 6", msg.vh.subject);
  }
  {
    auto [result, msg] = packet.Read();
    ASSERT_EQ(ReadNetPacketResponse::OK, result);
    EXPECT_EQ("All", msg.vh.to_user_name);
    EXPECT_EQ("test 7", msg.vh.subject);
  }
  {
    auto [result, msg] = packet.Read();
    ASSERT_EQ(ReadNetPacketResponse::END_OF_FILE, result);
  }
}