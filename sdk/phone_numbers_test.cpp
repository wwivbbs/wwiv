/**************************************************************************/
/*                                                                        */
/*                           WWIV Version 5.x                             */
/*            Copyright (C)2015-2021, WWIV Software Services              */
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

#include "core/file.h"
#include "core/strings.h"
#include "core/file_helper.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/phone_numbers.h"
#include "sdk/vardec.h"
#include "sdk/sdk_helper.h"
#include <string>
#include <vector>

using namespace std;

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

class PhoneNumbersTest : public testing::Test {
public:
  bool CreatePhoneNumDat(const Config& config) {
    File file(FilePath(config.datadir(), PHONENUM_DAT));
    file.Open(File::modeBinary|File::modeWriteOnly|File::modeCreateFile, File::shareDenyNone);
    if (!file) {
      return false;
    }
  
    phonerec rec{};
    rec.usernum = 1;
    to_char_array(rec.phone, "111-111-1111");
    file.Write(&rec, sizeof(phonerec));

    rec.usernum = 2;
    to_char_array(rec.phone, "222-222-2222");
    file.Write(&rec, sizeof(phonerec));

    return true;
  }

  SdkHelper helper;
};

TEST_F(PhoneNumbersTest, Find) {
  const Config config(helper.root());
  ASSERT_TRUE(config.IsInitialized());
  ASSERT_TRUE(CreatePhoneNumDat(config));

  const PhoneNumbers phone_numbers(config);
  ASSERT_TRUE(phone_numbers.IsInitialized());
  EXPECT_EQ(0, phone_numbers.find("455-555-1234"));
  EXPECT_EQ(1, phone_numbers.find("111-111-1111"));
  EXPECT_EQ(2, phone_numbers.find("222-222-2222"));
  EXPECT_EQ(0, phone_numbers.find("333-333-3333"));  // not found.
}

TEST_F(PhoneNumbersTest, Insert) {
  const Config config(helper.root());
  ASSERT_TRUE(config.IsInitialized());
  ASSERT_TRUE(CreatePhoneNumDat(config));

  {
    PhoneNumbers phone_numbers(config);
    ASSERT_TRUE(phone_numbers.IsInitialized());
    EXPECT_EQ(2, phone_numbers.find("222-222-2222"));
    EXPECT_TRUE(phone_numbers.insert(3, "333-333-3333"));
  }
  const PhoneNumbers phone_numbers(config);
  EXPECT_EQ(2, phone_numbers.find("222-222-2222")); // still found.
  EXPECT_EQ(3, phone_numbers.find("333-333-3333")); // still found.
}

TEST_F(PhoneNumbersTest, Erase) {
  const Config config(helper.root());
  ASSERT_TRUE(config.IsInitialized());
  ASSERT_TRUE(CreatePhoneNumDat(config));

  {
    PhoneNumbers phone_numbers(config);
    ASSERT_TRUE(phone_numbers.IsInitialized());
    EXPECT_EQ(2, phone_numbers.find("222-222-2222"));
    EXPECT_TRUE(phone_numbers.erase(2, "222-222-2222"));
  }
  const PhoneNumbers phone_numbers(config);
  EXPECT_EQ(0, phone_numbers.find("222-222-2222"));  // not found anymore.
  EXPECT_EQ(1, phone_numbers.find("111-111-1111"));  // still found.
}
