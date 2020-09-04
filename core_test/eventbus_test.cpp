/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2020, WWIV Software Services           */
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
#include "core/eventbus.h"
#include <iostream>
#include <string>

using std::string;

using namespace wwiv::core;

class EventBusTest : public ::testing::Test {
public:
  EventBus b;
};

TEST_F(EventBusTest, Const_Class_Handler) { 
  struct MessagePosted {
    int num;
  };
  int num = 1;
  b.add_handler<MessagePosted>([&num](const MessagePosted& m) {
    //const auto n = std::any_cast<MessagePosted>(m).num;
    std::cout << "m.num: " << m.num << std::endl;
    num += m.num;
  });

  MessagePosted m{1};
  b.invoke(m);
  EXPECT_EQ(2, num);
}

TEST_F(EventBusTest, Any) {
  struct MessagePosted {
    int num;
  };
  int num = 1;
  b.add_handler<MessagePosted>([&num](std::any m) {
    const auto n = std::any_cast<MessagePosted>(m).num;
    std::cout << "n: " << n << std::endl;
    num += n;
  });

  MessagePosted m{1};
  b.invoke(m);
  EXPECT_EQ(2, num);
}

TEST_F(EventBusTest, Class) {
  struct MessagePosted {
    int num;
  };
  int num = 1;
  b.add_handler<MessagePosted>([&num](MessagePosted m) {
    // const auto n = std::any_cast<MessagePosted>(m).num;
    std::cout << "m.num: " << m.num << std::endl;
    num += m.num;
  });

  MessagePosted m{1};
  b.invoke(m);
  EXPECT_EQ(2, num);
}

TEST_F(EventBusTest, Empty) {
  struct Empty {
    int x;
  };
  int num = 1;
  b.add_handler<Empty>([&num](Empty) { num++; });
  b.invoke(Empty{});
  EXPECT_EQ(2, num);
}
