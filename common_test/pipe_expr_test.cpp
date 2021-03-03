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
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "common/pipe_expr.h"
#include "common_test/common_helper.h"

using namespace wwiv::common;
using namespace testing;

TEST(PipeExprTest, Smoke) {
  CommonHelper helper;
  helper.user()->set_name("Rushfan");
  PipeEval eval(helper.context());
  EXPECT_EQ("Rushfan", eval.eval("{user.name}"));
  EXPECT_EQ("{unknown}", eval.eval("{unknown}"));
}

TEST(PipeExprTest, If) {
  CommonHelper helper;
  helper.user()->set_name("Rushfan");
  helper.user()->sl(200);
  helper.sess().effective_sl(200);
  PipeEval eval(helper.context());
  EXPECT_EQ("Senior Level", eval.eval(R"({if "user.sl >= 200", "Senior Level", "Junior Level"})"));
  helper.user()->sl(100);
  helper.sess().effective_sl(100);
  EXPECT_EQ("Junior Level", eval.eval(R"({if "user.sl >= 200", "Senior Level", "Junior Level"})"));
}
