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
#include "common/input_range.h"

#include "gtest/gtest.h"
#include "common/menus/menu_data_util.h"
#include <string>

using namespace wwiv::common;
using namespace wwiv::common::menus;
using namespace testing;

TEST(MenuDataUtil, NoOpts) {
  menu_data_and_options_t t("FILENAME");
  EXPECT_EQ(t.data(), "FILENAME");
  EXPECT_TRUE(t.opts_empty());
}

TEST(MenuDataUtil, OneOpt) {
  menu_data_and_options_t t("filename pause=on");
  EXPECT_EQ(t.data(), "filename");
  ASSERT_EQ(1u, t.size());
  auto o = t.opts("pause");
  ASSERT_EQ(1u, o.size());
  EXPECT_EQ(*std::begin(o), "on");
}

TEST(MenuDataUtil, OneOpt_UpperCase) {
  menu_data_and_options_t t("filename PAUSE=ON");
  EXPECT_EQ(t.data(), "filename");
  ASSERT_EQ(1u, t.size());
  auto o = t.opts("pause");
  ASSERT_EQ(1u, o.size());
  EXPECT_EQ(*std::begin(o), "on");
}

TEST(MenuDataUtil, TwoOpt) {
  menu_data_and_options_t t("filename pause=on mci=off");
  EXPECT_EQ(t.data(), "filename");
  EXPECT_EQ(2u, t.size());
  auto pause = t.opts("pause");
  EXPECT_EQ(*std::begin(pause), "on");
  ASSERT_EQ(1u, pause.size());

  auto mci = t.opts("mci");
  EXPECT_EQ(*std::begin(mci), "off");
  ASSERT_EQ(1u, mci.size());
}

TEST(MenuDataUtil, TwoOpt_SameKey) {
  menu_data_and_options_t t("filename pause=off pause=end");
  EXPECT_EQ(t.data(), "filename");
  EXPECT_EQ(2u, t.size());
  auto pause = t.opts("pause");
  EXPECT_EQ(2u, pause.size());
  auto it = std::begin(pause);
  EXPECT_EQ(*it++, "end");
  EXPECT_EQ(*it++, "off");
  EXPECT_EQ(std::end(pause), it);
}

TEST(MenuDataUtil, MissingOpt) {
  menu_data_and_options_t t("filename");
  EXPECT_EQ(t.data(), "filename");
  ASSERT_EQ(0u, t.size());
  auto o = t.opts("pause");
  ASSERT_TRUE(o.empty());
}

TEST(MenuDataUtil, Nothing) {
  menu_data_and_options_t t("");
  ASSERT_EQ(0u, t.size());
  auto o = t.opts("pause");
  ASSERT_TRUE(o.empty());
}
