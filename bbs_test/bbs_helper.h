/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_HELPER_H__
#define __INCLUDED_BBS_HELPER_H__

#include <memory>
#include <string>
#include "bbs.h"
#include "file_helper.h"
#include "wuser.h"

class BbsHelper {
public:
    virtual void SetUp();
    virtual void TearDown();
    FileHelper& files() { return files_; }
    WUser* user() const { return user_; }
public:
    FileHelper files_;
    std::string dir_gfiles_;
    std::string dir_en_gfiles_;
    std::string dir_menus_;
    std::unique_ptr<WApplication> app_;
    WUser* user_;
};

#endif // __INCLUDED_BBS_HELPER_H__
