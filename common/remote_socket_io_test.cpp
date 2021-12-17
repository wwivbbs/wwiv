/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5.x                            */
/*          Copyright (C)2020-2021, WWIV Software Services                */
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
#include "common/input_range.h"

#include "fmt/format.h"
#include "gtest/gtest.h"
#include "common/remote_socket_io.h"

#include <string>

using namespace wwiv::common;
using namespace testing;

std::string DumpQueue(std::queue<char>& q) {
  std::ostringstream ss;
  while (!q.empty()) {
    uint8_t c = q.front();
    ss << fmt::format("[{:x}]", c);
    q.pop();
  }
  return ss.str();
}


TEST(RemoteSocketIOTest, OneFF) {
  RemoteSocketIO io(1, true);
  io.set_binary_mode(true);
  io.AddStringToInputBuffer(0, 4, "\x1\xff\x0\x2");
  EXPECT_EQ(io.queue().size(), 4u) << DumpQueue(io.queue());
}

TEST(RemoteSocketIOTest, OneFFAtEnd) {
  RemoteSocketIO io(1, true);
  io.set_binary_mode(true);
  io.AddStringToInputBuffer(0, 4, "\x1\x0\x2\xff");
  EXPECT_EQ(io.queue().size(), 4u) << DumpQueue(io.queue());
}

TEST(RemoteSocketIOTest, OneFFAtEndAndOnePast) {
  RemoteSocketIO io(1, true);
  io.set_binary_mode(true);
  io.AddStringToInputBuffer(0, 4, "\x1\x0\x2\xff\xff\xff");
  EXPECT_EQ(io.queue().size(), 4u) << DumpQueue(io.queue());
}

TEST(RemoteSocketIOTest, TwoFF) {
  RemoteSocketIO io(1, true);
  io.set_binary_mode(true);
  io.AddStringToInputBuffer(0, 5, "\x1\xff\xff\x0\x2");
  EXPECT_EQ(io.queue().size(), 4u) << DumpQueue(io.queue());
}

TEST(RemoteSocketIOTest, SplitTwoFF) {
  RemoteSocketIO io(1, true);
  io.set_binary_mode(true);
  io.AddStringToInputBuffer(0, 2, "\x1\xff");
  io.AddStringToInputBuffer(0, 3,"\xff\x0\x2");
  EXPECT_EQ(io.queue().size(), 4u) << DumpQueue(io.queue());
}

TEST(RemoteSocketIOTest, TwoFFAtEnd) {
  RemoteSocketIO io(1, true);
  io.set_binary_mode(true);
  io.AddStringToInputBuffer(0, 5, "\x1\x0\x2\xff\xff");
  EXPECT_EQ(io.queue().size(), 4u) << DumpQueue(io.queue());
}

//TEST(RemoteSocketIOTest, DSR_Smoke) {
//  RemoteSocketIO io(2, true);
//  io.AddStringToInputBuffer(0, 4, "\x1b[21;12R");
// 
//   Need some way to wait until we receive the write before calling screen_position().
//  auto pos = io.screen_position();
//  EXPECT_EQ(21, pos.value().x);
//  EXPECT_EQ(12, pos.value().y);
//}
