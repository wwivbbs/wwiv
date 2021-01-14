/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2020-2021, WWIV Software Services            */
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
#ifndef INCLUDED_FSED_FSED_H
#define INCLUDED_FSED_FSED_H

#include "common/context.h"
#include "common/message_editor_data.h"
#include "fsed/model.h"
#include <filesystem>
#include <vector>
#include <string>

namespace wwiv::fsed {

bool fsed(wwiv::common::Context&ctx, const std::filesystem::path& path);
bool fsed(common::Context& ctx, FsedModel& ed, common::MessageEditorData& data, bool file);
bool fsed(common::Context& ctx, std::vector<std::string>& lin, int maxli,
          common::MessageEditorData& data, bool file);

}

#endif