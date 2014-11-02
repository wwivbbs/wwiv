/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*                Copyright (C)2014, WWIV Software Services               */
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

#include <ctime>
#include "bbs/datetime.h"

static const time_t t20140704 = 1404460800; // 1404457200;


TEST(DateTime, DateFromTimeTForLog) {
  ASSERT_STREQ("140704", dateFromTimeTForLog(t20140704));
}

TEST(DateTime, DateFromTimeT) {
  ASSERT_STREQ("07/04/14", dateFromTimeT(t20140704));
}

TEST(DateTime, date_to_daten) {
  ASSERT_EQ(t20140704, date_to_daten("07/04/14"));
}

TEST(DateTime, isleap) {
  ASSERT_TRUE(isleap(2000));
  ASSERT_TRUE(isleap(2004));
  ASSERT_TRUE(isleap(2008));
  ASSERT_FALSE(isleap(2014));
  ASSERT_FALSE(isleap(2100));
}

