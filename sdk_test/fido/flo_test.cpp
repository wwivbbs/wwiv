/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*            Copyright (C)2016-2021, WWIV Software Services              */
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
#include "core/datetime.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core_test/file_helper.h"
#include "sdk/fido/flo_file.h"
#include "gtest/gtest.h"
#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk::fido;
using namespace wwiv::sdk::net;

class FloTest : public testing::Test {
public:
  FloTest() {
    net.dir = helper_.TempDir().string();
    net.fido.fido_address = "11:1/211";
    p = helper_.CreateTempFile("00010064.flo", "^C:\\db\\outbound\\0000006f.su0");
  }

protected:
  FileHelper helper_;
  Network net{};

  std::filesystem::path p{};
};

TEST_F(FloTest, FloFile_Exists) {
  {
    const FloFile flo(net, p);
    ASSERT_TRUE(flo.exists());
  }
  { 
    p.replace_filename("00010063.flo");
    const FloFile flo(net, p);
    ASSERT_FALSE(flo.exists());
  }
}

TEST_F(FloTest, FloFile_Smoke) {
  FloFile flo(net, p);
  EXPECT_FALSE(flo.poll());
  ASSERT_EQ(1, wwiv::stl::ssize(flo.flo_entries()));

  {
    auto e = flo.flo_entries().front();
    EXPECT_EQ("C:\\db\\outbound\\0000006f.su0", e.first);
    EXPECT_EQ(flo_directive::delete_file, e.second);
    EXPECT_TRUE(flo.clear());
    EXPECT_TRUE(flo.empty());
    EXPECT_TRUE(flo.Save());
    EXPECT_FALSE(File::Exists(p)) << p;
  }

  EXPECT_TRUE(flo.insert("C:\\db\\outbound\\0000006f.mo0", flo_directive::truncate_file));
  {
    auto e = flo.flo_entries().front();
    EXPECT_EQ("C:\\db\\outbound\\0000006f.mo0", e.first);
    EXPECT_EQ(flo_directive::truncate_file, e.second);
    flo.Save();
    EXPECT_TRUE(File::Exists(p));
    EXPECT_TRUE(flo.clear());
    File::Remove(p);
  }

  {
    flo.set_poll(true);
    flo.Save();
    EXPECT_TRUE(File::Exists(p));

    TextFile tf(p, "r");
    const auto contents = tf.ReadFileIntoString();
    EXPECT_EQ("", contents);
  }
}

TEST_F(FloTest, Poll) {
  FloFile flo(net, p);
  ASSERT_FALSE(flo.poll());
  flo.set_poll(true);
  ASSERT_TRUE(flo.poll());
  flo.set_poll(false);
  ASSERT_FALSE(flo.poll());
}
