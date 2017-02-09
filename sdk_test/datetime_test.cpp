/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2014-2017, WWIV Software Services            */
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

#include <chrono>
#include <ctime>

#include "sdk/datetime.h"

using namespace std::chrono;
using namespace wwiv::sdk;

static const time_t t20140704 = 1404460800; // 1404457200;

TEST(DateTime, ToString) {
  EXPECT_EQ("1ms", to_string(milliseconds(1)));
  EXPECT_EQ("2ms", to_string(milliseconds(2)));

  EXPECT_EQ("1s 498ms", to_string(milliseconds(1498)));
    
  EXPECT_EQ("2m 1s 498ms", to_string(milliseconds(121498)));

  EXPECT_EQ("3h 2m 1s 498ms", to_string(milliseconds(10921498)));
}

// TODO(rushfan): Fix this now that the VM runs in PDT
#if 0
TEST(DateTime, date_to_daten) {
  ASSERT_EQ(t20140704, date_to_daten("07/04/14"));
}
#endif  // 0

