/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2022, WWIV Software Services           */
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
#include "gmock/gmock.h"

#include "fsed/model.h"

using namespace wwiv::fsed;
using namespace testing;


TEST(Model, NoWrap) { 
 FsedModel model(100);

 model.emplace_back({false, "Hello"});
 model.emplace_back({false, "World"});

 auto lines = model.to_lines(false);
 EXPECT_THAT(lines.size(), Eq(2));
 EXPECT_THAT(lines[0], Eq("Hello"));
 EXPECT_THAT(lines[1], Eq("World"));
}

TEST(Model, NoWrapNeeded) {
 FsedModel model(100);

 model.emplace_back({false, "Hello"});
 model.emplace_back({false, "World"});

 auto lines = model.to_lines(true);
 EXPECT_THAT(lines.size(), Eq(2));
 EXPECT_THAT(lines[0], Eq("Hello"));
 EXPECT_THAT(lines[1], Eq("World"));
}

TEST(Model, Wrap_NotWWIV) {
 FsedModel model(10);

 model.emplace_back({true, "Hello"});
 model.emplace_back({false, "World"});

 auto lines = model.to_lines(false);
 EXPECT_THAT(lines.size(), Eq(1));
 EXPECT_THAT(lines[0], Eq("Hello World"));
}

TEST(Model, Wrap_NotWWIV_End) {
 FsedModel model(10);

 model.emplace_back({true, "Hello"});
 model.emplace_back({true, "World"});

 auto lines = model.to_lines(false);
 EXPECT_THAT(lines.size(), Eq(1));
 EXPECT_THAT(lines[0], Eq("Hello World"));
}

TEST(Model, Wrap_WWIV) {
 FsedModel model(10);

 model.emplace_back({true, "Hello"});
 model.emplace_back({false, "World"});

 auto lines = model.to_lines(true);
 EXPECT_THAT(lines.size(), Eq(2));
 EXPECT_THAT(lines[0], Eq("Hello\x1"));
 EXPECT_THAT(lines[1], Eq("World"));
}

TEST(Model, Wrap_WWIV_End) {
 FsedModel model(10);

 model.emplace_back({true, "Hello"});
 model.emplace_back({true, "World"});

 auto lines = model.to_lines(true);
 EXPECT_THAT(lines.size(), Eq(2));
 EXPECT_THAT(lines[0], Eq("Hello\x1"));
 EXPECT_THAT(lines[1], Eq("World\x1"));
}
