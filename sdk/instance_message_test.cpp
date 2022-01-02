/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2007-2022, WWIV Software Services               */
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

#include "sdk/instance_message.h"
#include "sdk/sdk_helper.h"

using namespace wwiv::sdk;

class InstanceMessageTest : public testing::Test {
public:
  InstanceMessageTest() {}

  SdkHelper helper;
};

TEST_F(InstanceMessageTest, Smoke) {
  send_instance_string(helper.config(), instance_message_type_t::user, 2, 1, 1, "test");
  const auto im2 = read_all_instance_messages(helper.config(), 2);
  EXPECT_EQ(1u, im2.size());
  EXPECT_EQ("test", im2.front().message);

  const auto im1 = read_all_instance_messages(helper.config(), 1);
  EXPECT_TRUE(im1.empty());
}
