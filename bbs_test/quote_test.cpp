/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2017-2021, WWIV Software Services            */
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

#include "bbs/bbs.h"
#include "bbs_test/bbs_helper.h"
#include "common/quote.h"
#include <iostream>
#include <string>

using std::cout;
using std::endl;
using std::string;
using namespace wwiv::common;
using namespace wwiv::sdk;

class QuoteTest : public ::testing::Test {
protected:
  void SetUp() override {}
};

TEST_F(QuoteTest, Smoke) {
  EXPECT_EQ("RF", GetQuoteInitials("Rush Fan"));
  EXPECT_EQ("R", GetQuoteInitials("RushFan"));
  EXPECT_EQ("R", GetQuoteInitials("Rushfan"));
  EXPECT_EQ("RfD", GetQuoteInitials("Rush fan Dude"));
  EXPECT_EQ("RfDM", GetQuoteInitials("Rush fan Dude Man"));
  EXPECT_EQ("", GetQuoteInitials(""));
}

TEST_F(QuoteTest, WithUserNumber) {
  EXPECT_EQ("RF", GetQuoteInitials("Rush Fan #21"));
  EXPECT_EQ("R", GetQuoteInitials("RushFan #21"));
  EXPECT_EQ("R", GetQuoteInitials("Rushfan #21"));
  EXPECT_EQ("RfD", GetQuoteInitials("Rush fan Dude #21"));
}

TEST_F(QuoteTest, TrailingOrLeaderSpaces) {
  EXPECT_EQ("RF", GetQuoteInitials("Rush    Fan"));
  EXPECT_EQ("RF", GetQuoteInitials("    Rush Fan"));
  EXPECT_EQ("RF", GetQuoteInitials("Rush Fan     "));
  EXPECT_EQ("RF", GetQuoteInitials("Rush Fan     #21"));
}


TEST_F(QuoteTest, ParensForNodeOrRealName) {
  EXPECT_EQ("T", GetQuoteInitials("Tiny (21:1/130.4)"));
  EXPECT_EQ("RF", GetQuoteInitials("Rush Fan (21:1/130.4)"));
  EXPECT_EQ("RF", GetQuoteInitials("Rush Fan #1 (21:1/130.4)"));
  EXPECT_EQ("RF", GetQuoteInitials("Rush Fan #1 @1234 (21:1/130.4)"));

  EXPECT_EQ("JM", GetQuoteInitials("Rush Fan (Joe Mama)"));
}

TEST_F(QuoteTest, WithNode) {
  EXPECT_EQ("R", GetQuoteInitials("Rushfan #21 @1234"));
  EXPECT_EQ("RF", GetQuoteInitials("Rush Fan #21 @1234"));
}

TEST_F(QuoteTest, BracketsForApp) {
  EXPECT_EQ("R", GetQuoteInitials("Rushfan #21 @1234 [Cool Software]"));
  EXPECT_EQ("RF", GetQuoteInitials("Rush Fan #21 @1234 [Cool Software]"));
}

TEST_F(QuoteTest, EscapedNameFormat) {
  EXPECT_EQ("H", GetQuoteInitials("``Hobbit`` #1@4295.WWIVnet"));
  EXPECT_EQ("TH", GetQuoteInitials("``The Hobbit`` #1@4295.WWIVnet"));

  // Not sure if this is right but just as wrong as what we used to return.B
  EXPECT_EQ("", GetQuoteInitials("```Hobbit``` #1@4295.WWIVnet"));
  EXPECT_EQ("H", GetQuoteInitials("``Hobbit``` #1@4295.WWIVnet"));
}

TEST_F(QuoteTest, Matt) {
  EXPECT_EQ("M", GetQuoteInitials("Monk #1 @815 \xfe right winged conspirator \xfe"));
  EXPECT_EQ("TM", GetQuoteInitials("The Monk #1 @815 \xfe right winged conspirator \xfe"));
  EXPECT_EQ("M", GetQuoteInitials("Monk #1 @815 [ Windows to the People! ]"));
  EXPECT_EQ("TM", GetQuoteInitials("The Monk #1 @815 [ Windows to the People! ]"));
}

