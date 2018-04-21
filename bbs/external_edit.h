/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2014-2017, WWIV Software Services            */
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
#ifndef __INCLUDED_EXTERNAL_EDIT_H__
#define __INCLUDED_EXTERNAL_EDIT_H__

#include <string>
#include "sdk/vardec.h"

constexpr int MSGED_FLAG_NONE = 0;
constexpr int MSGED_FLAG_NO_TAGLINE = 1;
constexpr int MSGED_FLAG_HAS_REPLY_NAME = 2;
constexpr int MSGED_FLAG_HAS_REPLY_TITLE = 4;


bool ExternalMessageEditor(int maxli, int *setanon, std::string* title, const std::string& destination, int flags, bool is_email);

bool external_text_edit(const std::string& edit_filename, const std::string& new_directory, int numlines,
                        const std::string& destination, int flags);

#endif  // __INCLUDED_EXTERNAL_EDIT_H__

