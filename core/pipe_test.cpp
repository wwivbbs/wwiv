/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services            */
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
#include "core/pipe.h"

#include "gtest/gtest.h"
#include "core/log.h"
#include "core/pipe.h"

#include <chrono>
#include <string>
#include <thread>

using namespace wwiv::core;

static void client() {
  Pipe cp("1");
  if (!cp.Open()) {
    return;
  }
  cp.write("Hello\0", 6);
}

TEST(PipeTest, Smoke) {
  Pipe sp("1");
  auto client_thread = std::thread(client);
  ASSERT_TRUE(sp.Create());
  EXPECT_TRUE(sp.WaitForClient(std::chrono::seconds(2)));

  char s[81];
  auto o = sp.read(s, 6);
  ASSERT_TRUE(o);
  EXPECT_STREQ("Hello", s);

  client_thread.join();
}