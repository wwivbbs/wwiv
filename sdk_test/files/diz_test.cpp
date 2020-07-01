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

#include "core_test/file_helper.h"
#include "sdk/files/diz.h"


TEST(DizTest, Smoke) {
  FileHelper helper;

  const std::string kDIZ = R"(Line1
Line2
Line3)";

  const auto file = helper.CreateTempFile("file_id.diz", kDIZ);

  wwiv::sdk::files::DizParser p(true);
  auto o = p.parse(file);
  ASSERT_TRUE(o);

  auto d = o.value();
  EXPECT_EQ(d.description(), "Line1");
  EXPECT_EQ(d.extended_description(), "Line2\nLine3\n");
}

TEST(DizTest, Smoke_NotDescription) {
  FileHelper helper;

  const std::string kDIZ = R"(Line1
Line2
Line3)";

  const auto file = helper.CreateTempFile("file_id.diz", kDIZ);

  wwiv::sdk::files::DizParser p(false);
  auto o = p.parse(file);
  ASSERT_TRUE(o);

  auto d = o.value();
  EXPECT_EQ(d.description(), "");
  EXPECT_EQ(d.extended_description(), "Line1\nLine2\nLine3\n");
}
