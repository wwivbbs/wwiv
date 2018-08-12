/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2017, WWIV Software Services               */
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

#include <iostream>
#include <memory>
#include <string>

#include "core/strings.h"
#include "sdk/ansi/makeansi.h"

using std::cout;
using std::endl;
using std::string;
using namespace wwiv::sdk::ansi;

class MakeAnsiTest : public ::testing::Test {};

TEST_F(MakeAnsiTest, NOOP) {
  EXPECT_EQ("", makeansi(7, 7));
}

TEST_F(MakeAnsiTest, Foreground) {
  EXPECT_EQ("\x1B[30m", makeansi(0, 7));
  EXPECT_EQ("\x1B[34m", makeansi(1, 7));
  EXPECT_EQ("\x1B[32m", makeansi(2, 7));
  EXPECT_EQ("\x1B[36m", makeansi(3, 7));
  EXPECT_EQ("\x1B[31m", makeansi(4, 7));
  EXPECT_EQ("\x1B[35m", makeansi(5, 7));
  EXPECT_EQ("\x1B[33m", makeansi(6, 7));
  EXPECT_EQ("\x1B[37m", makeansi(7, 0));
  EXPECT_EQ("\x1B[0;30;40;1m", makeansi(8, 7));
  EXPECT_EQ("\x1B[34m", makeansi(9, 8));
  EXPECT_EQ("\x1B[32m", makeansi(10, 8));
  EXPECT_EQ("\x1B[36m", makeansi(11, 8));
  EXPECT_EQ("\x1B[31m", makeansi(12, 8));
  EXPECT_EQ("\x1B[35m", makeansi(13, 8));
  EXPECT_EQ("\x1B[33m", makeansi(14, 8));
  EXPECT_EQ("\x1B[37m", makeansi(15, 8));
}

TEST_F(MakeAnsiTest, Foreground_BrightToDim) {
  EXPECT_EQ("\x1B[0;37;40m", makeansi(7, 9));
}

TEST_F(MakeAnsiTest, Foreground_DimToBright) {
  EXPECT_EQ("\x1B[0;34;40;1m", makeansi(9, 7));
}

TEST_F(MakeAnsiTest, Background) {
  EXPECT_EQ("\x1B[0;37;44m", makeansi(23, 9));
  EXPECT_EQ("\x1B[0;33;44;1m", makeansi(30, 23));
}
