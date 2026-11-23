/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                   Copyright (C)2023, WWIV Software Services            */
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

#include "core/net.h"
#include <string>

using namespace wwiv::core;

class NetTest : public ::testing::Test {
public:
  NetTest() {}
};

TEST_F(NetTest, Rfc1918_In) {
  ASSERT_TRUE(is_rfc1918_private_address("10.2.3.4"));
  ASSERT_TRUE(is_rfc1918_private_address("192.168.12.34"));
  ASSERT_TRUE(is_rfc1918_private_address("172.16.3.3"));
  ASSERT_TRUE(is_rfc1918_private_address("172.17.1.2"));
}

TEST_F(NetTest, Rfc1918_Out) {
  ASSERT_FALSE(is_rfc1918_private_address("172.15.1.2"));
  ASSERT_FALSE(is_rfc1918_private_address("1.2.3.4"));
  ASSERT_FALSE(is_rfc1918_private_address("192.162.12.34"));
}

TEST_F(NetTest, Rfc1918_Malformed) {
  ASSERT_FALSE(is_rfc1918_private_address(""));
  ASSERT_FALSE(is_rfc1918_private_address("1.2.3.4.5"));
  ASSERT_FALSE(is_rfc1918_private_address("Geddy Lee"));
}
