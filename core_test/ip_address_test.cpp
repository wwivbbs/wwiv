/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                    Copyright (C)2020, WWIV Software Services           */
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

#include "core/log.h"
#include "core/ip_address.h"
#include <string>

using std::string;
using namespace wwiv::core;

class IpAddressTest : public ::testing::Test {
public:
  IpAddressTest()  {}
};

TEST_F(IpAddressTest, Smoke_Home_V4) {
  auto o = ip_address::from_string("127.0.0.1");
  EXPECT_TRUE(o.has_value());
  auto a = o.value();
  EXPECT_EQ("127.0.0.1", a.to_string());
}

TEST_F(IpAddressTest, Smoke_Home_V6) {
  auto o = ip_address::from_string("::1");
  EXPECT_TRUE(o.has_value());
  auto a = o.value();
  EXPECT_EQ("[::1]", a.to_string());
}
