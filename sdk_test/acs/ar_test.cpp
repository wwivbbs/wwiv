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

#include "core/parser/ast.h"
#include "core/parser/lexer.h"
#include "sdk/user.h"
#include "sdk/acs/eval.h"
#include "sdk/acs/uservalueprovider.h"
#include <string>
#include <iostream>

using std::string;
using namespace wwiv::core;
using namespace wwiv::core::parser;
using namespace wwiv::sdk::acs;

class ArTest : public ::testing::Test {
public:
  ArTest() {}

};

TEST_F(ArTest, Eq) { 
  Ar a('A');
  Ar abcd("ABCD");
  EXPECT_EQ(abcd, a);
  EXPECT_EQ(a, abcd);
}

TEST_F(ArTest, Ne) {
  Ar a('A');
  Ar cde("CDE");
  EXPECT_NE(cde, a);
  EXPECT_NE(a, cde);
}

TEST_F(ArTest, Eq_Conversion) {
  Ar a('A');
  Ar abcd("ABCD");

  EXPECT_EQ(abcd, 'A');
  EXPECT_EQ(abcd, "A");
  EXPECT_EQ("A", abcd);
  EXPECT_EQ("A", abcd);
}

TEST_F(ArTest, Ne_Conversion) {
  Ar a('A');
  Ar cde("CDE");
  EXPECT_NE(cde, 'A');
  EXPECT_NE(cde, "A");
  EXPECT_NE('A', cde);
  EXPECT_NE("A", cde);
}

TEST_F(ArTest, Eq_Empty) {
  Ar e(0);
  EXPECT_EQ(e, "A");
  EXPECT_EQ("A", e);
}
