/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*               Copyright (C)2014-2015 WWIV Software Services            */
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
#include "file_helper.h"
#include "gtest/gtest.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/strings.h"

#include <iostream>
#include <string>

using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

TEST(DataFileTest, Basic) {
  struct T { int a; int b; };
  FileHelper file;
  string tmp = file.TempDir();

  File x(tmp, "Basic1");
  ASSERT_TRUE(x.Open(File::modeCreateFile|File::modeBinary|File::modeReadWrite));
  T t1{1, 2};
  T t2{3, 4};
  x.Write(&t1, sizeof(T));
  x.Write(&t2, sizeof(T));
  x.Close();

  {
    DataFile<T, sizeof(T)> datafile(tmp, "Basic1", File::modeReadOnly);
    ASSERT_TRUE(datafile.ok());
    T t{0, 0};
    EXPECT_TRUE(datafile.Read(&t));
    EXPECT_EQ(1, t.a);
    EXPECT_EQ(2, t.b);

    EXPECT_TRUE(datafile.Read(&t));
    EXPECT_EQ(3, t.a);
    EXPECT_EQ(4, t.b);
  }

}
