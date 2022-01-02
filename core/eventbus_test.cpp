/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2022, WWIV Software Services           */
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

using namespace wwiv::core;

class EventBusTest : public ::testing::Test {
public:
  EventBus b;
};

struct MessagePosted {
  int num;
};

TEST_F(EventBusTest, Function) {
  auto num = 1;
  b.add_handler<MessagePosted>([&num](MessagePosted m) { num += m.num; });

  const MessagePosted m{1};
  b.invoke(m);
  EXPECT_EQ(2, num);
}

TEST_F(EventBusTest, Function_Zero_Args) {
  auto num = 1;
  b.add_handler<MessagePosted>([&num]() { num++; });

  const MessagePosted m{1};
  b.invoke(m);
  EXPECT_EQ(2, num);
}
TEST_F(EventBusTest, Function_Const) {
  auto num = 1;
  b.add_handler<MessagePosted>([&num](const MessagePosted& m) { num += m.num; });

  const MessagePosted m{1};
  b.invoke(m);
  EXPECT_EQ(2, num);
}
TEST_F(EventBusTest, Function_Any) {
  auto num = 1;
  b.add_handler<MessagePosted>([&num](std::any m) {
    const auto n = std::any_cast<MessagePosted>(m).num;
    std::cout << "n: " << n << std::endl;
    num += n;
  });

  const MessagePosted m{1};
  b.invoke(m);
  EXPECT_EQ(2, num);
}

static int method_num = 1;
void Method(MessagePosted m) { method_num += m.num; };

TEST_F(EventBusTest, Method) {
  b.add_handler<MessagePosted>(Method);

  const MessagePosted m{1};
  b.invoke(m);
  EXPECT_EQ(2, method_num);
}

TEST_F(EventBusTest, Class) {
  struct Empty {
    int x;
  };

  class Class {
  public:
    int num{1};
    void Method(MessagePosted m) { num += m.num; }
  };

  Class c;
  b.add_handler<MessagePosted>(&Class::Method, &c);
  b.invoke(MessagePosted{1});
  EXPECT_EQ(2, c.num);
}
