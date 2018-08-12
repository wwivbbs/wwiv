/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2018, WWIV Software Services                 */
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
#include "sdk/ansi/ansi.h"
#include "sdk/ansi/framebuffer.h"
#include "sdk_test/sdk_helper.h"

using namespace wwiv::strings;
using namespace wwiv::sdk::ansi;

class AnsiTest : public testing::Test {};

TEST_F(AnsiTest, Smoke) { 
  const std::string s = "\x1b[34;1mHello";
  FrameBuffer b{10};
  Ansi ansi(&b, 0x07);
  ansi.write(s);
  b.close();
  EXPECT_EQ(5, b.pos());
  EXPECT_EQ(1, b.rows());
  EXPECT_EQ("Hello", b.row_as_text(0));
  EXPECT_EQ(9, b.curatr());
}

