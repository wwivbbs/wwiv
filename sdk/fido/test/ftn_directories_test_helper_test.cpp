/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2021-2022, WWIV Software Services             */
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

#include "core/file.h"
#include "core/test/file_helper.h"
#include "sdk/fido/test/ftn_directories_test_helper.h"

using namespace wwiv::core;
using namespace wwiv::core::test;
using namespace wwiv::sdk::fido::test;

class FtnDirectoriesTestHelperTest : public testing::Test {
public:
  FtnDirectoriesTestHelperTest() : file_helper_(), helper_(file_helper_) {}

protected:
  FileHelper file_helper_;
  FtnDirectoriesTestHelper helper_;
  std::filesystem::path net_dir_;
};

TEST_F(FtnDirectoriesTestHelperTest, Name) { ASSERT_EQ("TestNET", helper_.net().name); }

TEST_F(FtnDirectoriesTestHelperTest, Address) {
  ASSERT_EQ("21:12/2112", helper_.net().fido.fido_address);
}

TEST_F(FtnDirectoriesTestHelperTest, In) {
  ASSERT_EQ(FilePath(file_helper_.TempDir(), "network/in"), helper_.dirs().inbound_dir());
}

TEST_F(FtnDirectoriesTestHelperTest, Out) {
  ASSERT_EQ(FilePath(file_helper_.TempDir(), "network/out"), helper_.dirs().outbound_dir());
}

TEST_F(FtnDirectoriesTestHelperTest, NetDir) {
  ASSERT_EQ(FilePath(file_helper_.TempDir(), "network"), helper_.dirs().net_dir());
}
