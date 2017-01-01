/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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
#include "networkb/net_util.h"

#include <cstdint>
#include <string>

using std::endl;
using std::string;
using std::unique_ptr;
using namespace wwiv::net;
using namespace wwiv::strings;


class NetworkUtilTest: public testing::Test {
public:
  NetworkUtilTest() {}
protected:
  FileHelper helper_;
};

TEST_F(NetworkUtilTest, GetMessageField) {
  char raw[] = {'a', '\0', 'b', 'c', '\r', '\n', 'd'};
  string text(raw, 7);
  auto iter = text.begin();
  string a = get_message_field(text, iter, {'\0', '\r', '\n'}, 80);
  EXPECT_STREQ("a", a.c_str());
  string bc = get_message_field(text, iter, {'\0', '\r', '\n'}, 80);
  EXPECT_STREQ("bc", bc.c_str());
  string remaining = string(iter, text.end());
  EXPECT_STREQ("d", remaining.c_str());
}
