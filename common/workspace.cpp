/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "common/workspace.h"

#include "common/context.h"
#include "common/output.h"
#include "core/file.h"
#include "core/textfile.h"
#include "sdk/filenames.h"
#include <string>

using namespace wwiv::core;

namespace wwiv::common {

bool use_workspace;

void LoadFileIntoWorkspace(Context& context, const std::filesystem::path& filename,
                           bool no_edit_allowed, bool silent_mode) {
  TextFile tf(filename, "rt");
  if (!tf) {
    return;
  }
  const auto contents = tf.ReadFileIntoString();
  if (contents.empty()) {
    return;
  }

  TextFile input_msg(FilePath(context.session_context().dirs().temp_directory(), INPUT_MSG), "wt");
  input_msg.Write(contents);

  const auto ok_fsed = context.u().default_editor() != 0;
  use_workspace = (no_edit_allowed || !ok_fsed);

  if (!silent_mode) {
    bout << "\r\nFile loaded into workspace.\r\n\n";
    if (!use_workspace) {
      bout << "Editing will be allowed.\r\n";
    }
  }
}

} // namespace wwiv::common
