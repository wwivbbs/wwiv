/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2020, WWIV Software Services               */
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

#include "bbs/basic/util.h"
#include "bbs_test/bbs_helper.h"
#include "core/stl.h"
#include "core/strings.h"
#include "deps/my_basic/core/my_basic.h"
#include <iostream>
#include <string>

using std::cout;
using std::endl;
using std::string;
using namespace wwiv::stl;
using namespace wwiv::bbs::basic;

TEST(BasicUtilTest, MakeString) {
  const auto s = wwiv_mb_make_string("Hello");
  EXPECT_STREQ("Hello", s.value.string);
}

TEST(BasicUtilTest, MakeInt) {
  const auto s = wwiv_mb_make_int(1234);
  EXPECT_EQ(1234, s.value.integer);
}

TEST(BasicUtilTest, MakeReal) {
  const auto s = wwiv_mb_make_real(1234);
  EXPECT_FLOAT_EQ(1234.0, s.value.float_point);
}
