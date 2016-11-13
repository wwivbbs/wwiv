/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2016 WWIV Software Services                 */
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
#include "networkb/fido_util.h"
#include "sdk/fido/fido_address.h"

#include <cstdint>
#include <memory>
#include <string>

using std::endl;
using std::string;
using std::unique_ptr;
using namespace wwiv::strings;
using namespace wwiv::net;
using namespace wwiv::net::fido;
using namespace wwiv::sdk::fido;

class FidoUtilTest: public testing::Test {
public:
  FidoUtilTest() {}
protected:
  FileHelper helper_;
};


TEST_F(FidoUtilTest, PacketName) {
  time_t now = time(nullptr);
  auto tm = localtime(&now);
  string actual = packet_name(now);

  string expected = StringPrintf("%2.2d%2.2d%2.2d%2.2d", tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
  EXPECT_EQ(expected, actual);
}

TEST_F(FidoUtilTest, BundleName_Extension) {
  FidoAddress source("1:105/6");
  FidoAddress dest("1:105/42");
  string actual = bundle_name(source, dest, "su0");

  EXPECT_EQ("0000ffdc.su0", actual);
}

TEST_F(FidoUtilTest, BundleName_NoExtension) {
  FidoAddress source("1:105/6");
  FidoAddress dest("1:105/42");
  string actual = bundle_name(source, dest, 1, 3);

  EXPECT_EQ("0000ffdc.mo3", actual);
}

TEST_F(FidoUtilTest, DowExtension) {
  EXPECT_EQ("mo0", dow_extension(1, 0));
  EXPECT_EQ("sa9", dow_extension(6, 9));
  EXPECT_EQ("sua", dow_extension(0, 10));
  EXPECT_EQ("suz", dow_extension(0, 35));
}

TEST_F(FidoUtilTest, ControlFileName) {
  FidoAddress dest("1:105/42");
  EXPECT_EQ("0069002a.flo", control_file_name(dest, FidoBundleStatus::normal));
  EXPECT_EQ("0069002a.clo", control_file_name(dest, FidoBundleStatus::crash));
  EXPECT_EQ("0069002a.dlo", control_file_name(dest, FidoBundleStatus::direct));
  EXPECT_EQ("0069002a.hlo", control_file_name(dest, FidoBundleStatus::hold));
}
