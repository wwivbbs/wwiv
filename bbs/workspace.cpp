/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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
#include "bbs/workspace.h"
#include <memory>
#include <string>

#include "core/file.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "bbs/bbs.h"
#include "bbs/utility.h"
#include "bbs/application.h"

#include "sdk/filenames.h"

bool use_workspace;

using std::unique_ptr;
using namespace wwiv::core;

void LoadFileIntoWorkspace(const std::string& filename, bool bNoEditAllowed, bool silent_mode) {
  File fileOrig(filename);
  if (!fileOrig.Open(File::modeBinary | File::modeReadOnly)) {
    bout << "\r\nFile not found.\r\n\n";
    return;
  }

  long lOrigSize = fileOrig.length();
  unique_ptr<char[]> b(new char[lOrigSize + 1024]);
  fileOrig.Read(b.get(), lOrigSize);
  fileOrig.Close();
  if (b[lOrigSize - 1] != CZ) {
    b[lOrigSize++] = CZ;
  }

  File fileOut(PathFilePath(a()->temp_directory(), INPUT_MSG));
  fileOut.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
  fileOut.Write(b.get(), lOrigSize);
  fileOut.Close();

  use_workspace = (bNoEditAllowed || !okfsed()) ? true : false;

  if (!silent_mode) {
    bout << "\r\nFile loaded into workspace.\r\n\n";
    if (!use_workspace) {
      bout << "Editing will be allowed.\r\n";
    }
  }
}
