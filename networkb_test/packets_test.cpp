/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*                Copyright (C)2017, WWIV Software Services               */
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
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "networkb/packets.h"
#include "sdk/datetime.h"

#include <cstdint>
#include <string>

using std::endl;
using std::string;
using std::unique_ptr;
using namespace wwiv::net;
using namespace wwiv::sdk;
using namespace wwiv::strings;


class PacketsTest: public testing::Test {
public:
  PacketsTest() {}
protected:
  FileHelper helper_;
};

TEST_F(PacketsTest, GetNetInfoFileInfo_Smoke) {
  string text;
  text.push_back(1);
  text.push_back(0);
  text += "BINKP";
  text.push_back(0);
  text += "Hello World";
  ASSERT_EQ(19, text.size());

  net_header_rec nh{};
  nh.daten = time_t_to_daten(time(nullptr));
  nh.method = 0;
  nh.main_type = main_type_file;
  nh.minor_type = net_info_file;
  Packet p(nh, {}, text);

  auto info = GetNetInfoFileInfo(p);
  ASSERT_TRUE(info.valid);
  EXPECT_EQ("BINKP.net", info.filename);
  EXPECT_TRUE(info.overwrite);
  EXPECT_EQ("Hello World", info.data);
}
