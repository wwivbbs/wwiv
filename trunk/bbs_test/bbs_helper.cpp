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
#include "bbs_helper.h"
#include <sstream>

#include <algorithm>
#include <iostream>
#include <string>

#include "bbs/bbs.h"
#include "core/wwivport.h"
#include "core/wfile.h"
#include "bbs/vars.h"
#include "bbs/wuser.h"
#include "bbs/wsession.h"

#include "gtest/gtest.h"

void BbsHelper::SetUp() {
    std::string temp = files_.TempDir();
    ASSERT_TRUE(files_.Mkdir("gfiles"));
    ASSERT_TRUE(files_.Mkdir("en"));
    ASSERT_TRUE(files_.Mkdir("en/gfiles"));
    app_.reset(CreateApplication());

    dir_gfiles_ = files_.DirName("gfiles");
    dir_en_gfiles_ = files_.DirName("en/gfiles");
#ifdef _WIN32
    std::replace(dir_gfiles_.begin(), dir_gfiles_.end(), '/', WWIV_FILE_SEPERATOR_CHAR);
    std::replace(dir_en_gfiles_.begin(), dir_en_gfiles_.end(), '/', WWIV_FILE_SEPERATOR_CHAR);
#endif  // _WIN32

    syscfg.gfilesdir = const_cast<char*>(dir_gfiles_.c_str());
    WSession* session = GetSession();

    session->language_dir = dir_en_gfiles_;
    user_ = GetSession()->GetCurrentUser();
}

void BbsHelper::TearDown() {
}
