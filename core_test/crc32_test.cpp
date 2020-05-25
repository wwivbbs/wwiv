/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2020, WWIV Software Services           */
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
#include "core/crc32.h"
#include "core/file.h"
#include "core_test/file_helper.h"
#include <map>
#include <string>
#include <vector>

using std::map;
using std::string;
using std::vector;

using namespace wwiv::core;

TEST(Crc32Test, Simple) {
  FileHelper file;
  const auto path = file.CreateTempFile("helloworld.txt", "Hello World");

  ASSERT_TRUE(File::Exists(path));

  const auto crc = crc32file(path.string());
  const uint32_t expected = 0x4a17b156;

  // use wwiv/scripts/crc32.py to generate golden values as needed.
  EXPECT_EQ(expected, crc) << " was " << std::hex << crc;
}
