/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2020-2021, WWIV Software Services           */
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
#include "sdk/user.h"
#include "sdk/acs/eval.h"
#include <string>

using namespace wwiv::core;
using namespace wwiv::core::parser;
using namespace wwiv::sdk::acs;
using namespace wwiv::sdk::value;

class ValueTest : public ::testing::Test {
public:
  ValueTest() = default;
};

#define EXPECT_VALUE_TRUE(l, op, r) EXPECT_TRUE(Value::eval(l, op, r).as_boolean())
#define EXPECT_VALUE_FALSE(l, op, r) EXPECT_FALSE(Value::eval(l, op, r).as_boolean())

TEST_F(ValueTest, Smoke) {
  const Value l("hello");
  const Value r("hello");
  const Value r2("foo");
  EXPECT_VALUE_TRUE(l, Operator::eq, r);
  EXPECT_VALUE_FALSE(l, Operator::eq, r2);
}
