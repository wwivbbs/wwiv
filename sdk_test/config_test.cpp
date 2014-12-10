/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)2014, WWIV Software Services                  */
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

#include "core/file.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "sdk/config.h"
#include "sdk/networks.h"

using std::cout;
using std::endl;
using std::string;

using namespace wwiv::sdk;

// TODO(rushfan): These tests don't work yet - just testing locally
// for now. Need to create a tree under the tempdir containing a 
// stub BBS.
class ConfigTest : public testing::Test {
};


TEST_F(ConfigTest, Config) {
  const string saved_dir = File::current_directory();
  ASSERT_EQ(0, chdir("C:\\bbs"));

  Config config;
  EXPECT_TRUE(config.IsInitialized());
  std::cout << config.datadir() << endl;

  chdir(saved_dir.c_str());
}
