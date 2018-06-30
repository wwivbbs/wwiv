/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services            */
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
#include "core/findfiles.h"

#include <iostream>
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "core/wfndfile.h"

namespace wwiv {
namespace core {

FindFiles::FindFiles(const std::string& dir, const std::string& mask, const FindFilesType type) 
  : FindFiles(FilePath(dir, mask), type) {}

static WFindFileTypeMask FindFilesTypeToInt(FindFilesType type) {
  switch (type) {
  case FindFilesType::any: return WFindFileTypeMask::WFINDFILE_ANY;
  case FindFilesType::directories: return WFindFileTypeMask::WFINDFILE_DIRS;
  case FindFilesType::files: return WFindFileTypeMask::WFINDFILE_FILES;
  default: 
    LOG(FATAL) << "Invalid FindFilesType: " << static_cast<int>(type);
    // Make Compiler happy
    return WFindFileTypeMask::WFINDFILE_ANY;
  }
}

FindFiles::FindFiles(const std::string& mask, const FindFilesType type) {
  WFindFile fnd;
  if (!fnd.open(mask, FindFilesTypeToInt(type))) {
    VLOG(1) << "Unable to open mask: " << mask;
    return;
  }
  do {
    // file_mask_ doesn't work on windows... sigh.
    // good thing WFindFiles will go away.
    if (fnd.IsDirectory() && type == FindFilesType::files) {
      continue;
    }
    if (fnd.IsFile() && type == FindFilesType::directories) {
      continue;
    }
    std::string fn = fnd.GetFileName();
    if (fnd.IsDirectory()) {
      if (fn == "." || fn == "..") {
        // Skip . and .. directories.
        continue;
      }
    }
    entries_.push_back({fn, fnd.GetFileSize() });
  } while (fnd.next());
}

}
}
