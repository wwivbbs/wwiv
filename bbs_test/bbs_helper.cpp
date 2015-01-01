/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*           Copyright (C)2014-2015 WWIV Software Services                */
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
#include "bbs_helper.h"
#include <sstream>

#include <algorithm>
#include <iostream>
#include <string>

#include "bbs/bbs.h"
#include "core/wwivport.h"
#include "core/file.h"
#include "bbs/vars.h"
#include "bbs/wuser.h"
#include "bbs/wsession.h"

#include "gtest/gtest.h"

using std::string;

void BbsHelper::SetUp() {
    string temp = files_.TempDir();
    // We want the "BBS Home" to be our temp dir.
    chdir(files_.TempDir().c_str());

    ASSERT_TRUE(files_.Mkdir("data"));
    ASSERT_TRUE(files_.Mkdir("gfiles"));
    ASSERT_TRUE(files_.Mkdir("en"));
    ASSERT_TRUE(files_.Mkdir("en/gfiles"));
    // Use our own local IO class that will capture the output.
    io_.reset(new TestIO());
    // Without local_echo, we won't capture anything.
    local_echo = true;
    app_.reset(CreateApplication(io_->local_io()));

    dir_data_ = files_.DirName("data");
    dir_gfiles_ = files_.DirName("gfiles");
    dir_en_gfiles_ = files_.DirName("en/gfiles");
#ifdef _WIN32
    std::replace(std::begin(dir_gfiles_), std::end(dir_gfiles_), '/', File::pathSeparatorChar);
    std::replace(std::begin(dir_en_gfiles_), std::end(dir_en_gfiles_), '/', File::pathSeparatorChar);
#endif  // _WIN32

    syscfg.datadir = const_cast<char*>(dir_data_.c_str());
    syscfg.gfilesdir = const_cast<char*>(dir_gfiles_.c_str());
    WSession* s = session();

    s->language_dir = dir_en_gfiles_;
    user_ = s->user();
}

void BbsHelper::TearDown() {
}

TestIO::TestIO() {
  local_io_ = new TestLocalIO(&this->captured_);
}

string TestIO::captured() {
  string captured(captured_);
  captured_.clear();
  return captured;
}

TestLocalIO::TestLocalIO(string* captured) : WLocalIO(), captured_(captured) {}

void TestLocalIO::LocalPutch(unsigned char ch) {
  captured_->push_back(ch);
}
