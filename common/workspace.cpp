/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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

#include <memory>
#include <string>
#include "core/file.h"
#include "bbs/bbs.h"
#include "bbs/utility.h"
#include "bbs/application.h"
#include "local_io/keycodes.h"
#include "sdk/filenames.h"

bool use_workspace;

using std::unique_ptr;
using namespace wwiv::core;

// TODO(rushfan): Rewrite this using TextFile::ReadFileIntoString
void LoadFileIntoWorkspace(const std::string& filename, bool no_edit_allowed, bool silent_mode) {
  File fileOrig(filename);
  if (!fileOrig.Open(File::modeBinary | File::modeReadOnly)) {
    bout << "\r\nFile not found.\r\n\n";
    return;
  }

  auto lOrigSize = fileOrig.length();
  unique_ptr<char[]> b(new char[lOrigSize + 1024]);
  fileOrig.Read(b.get(), lOrigSize);
  fileOrig.Close();
  if (b[lOrigSize - 1] != CZ) {
    b[lOrigSize++] = CZ;
  }

  File fileOut(FilePath(a()->context().dirs().temp_directory(), INPUT_MSG));
  fileOut.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
  fileOut.Write(b.get(), lOrigSize);
  fileOut.Close();

  use_workspace = (no_edit_allowed || !okfsed()) ? true : false;

  if (!silent_mode) {
    bout << "\r\nFile loaded into workspace.\r\n\n";
    if (!use_workspace) {
      bout << "Editing will be allowed.\r\n";
    }
  }
}

void LoadFileIntoWorkspace(const std::filesystem::path& file_path, bool no_edit_allowed, bool silent_mode) {
  LoadFileIntoWorkspace(file_path.string(), no_edit_allowed, silent_mode);
}
