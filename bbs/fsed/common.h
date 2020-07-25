/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                   Copyright (C)2020, WWIV Software Services            */
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
#ifndef __INCLUDED_BBS_FSED_COMMON_H__
#define __INCLUDED_BBS_FSED_COMMON_H__

#include "bbs/full_screen.h"
#include "bbs/message_editor_data.h"
#include "bbs/fsed/editor.h"
#include "bbs/fsed/line.h"
#include <filesystem>
#include <functional>
#include <vector>
#include <string>

namespace wwiv::bbs::fsed {

std::vector<line_t> read_file(const std::filesystem::path& path, int line_length);

void gotoxy(const editor_t& ed, const FullScreenView& fs);

}

#endif