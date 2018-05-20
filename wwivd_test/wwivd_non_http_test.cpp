/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                   Copyright (C)2018, WWIV Software Services            */
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
#include "core/datafile.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core_test/file_helper.h"
#include "gtest/gtest.h"

#include "wwivd/wwivd_non_http.h"

#include <iostream>
#include <string>
#include <vector>

using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::wwivd;

TEST(GoodIps, IsAlwaysAllowed) { 
  vector<string> lines{"10.0.0.1", "192.168.0.1 # This is line #2"};
  GoodIp ip(lines);
  EXPECT_TRUE(ip.IsAlwaysAllowed("10.0.0.1"));
  EXPECT_TRUE(ip.IsAlwaysAllowed("192.168.0.1"));

  EXPECT_FALSE(ip.IsAlwaysAllowed("10.0.0.2"));
}

TEST(BadIps, Smoke) { 
  FileHelper helper;
  auto fn = helper.CreateTempFile("badip.txt", "10.0.0.1\r\n8.8.8.8\r\n");
  BadIp ip(fn);
  EXPECT_TRUE(ip.IsBlocked("10.0.0.1"));
  EXPECT_TRUE(ip.IsBlocked("8.8.8.8"));
  EXPECT_FALSE(ip.IsBlocked("4.4.4.4"));
  ip.Block("1.1.1.1");
  EXPECT_TRUE(ip.IsBlocked("1.1.1.1"));

  TextFile tf(fn, "rt");
  auto contents = tf.ReadFileIntoString();
  EXPECT_TRUE(contents.find("1.1.1.1") != contents.npos);
}