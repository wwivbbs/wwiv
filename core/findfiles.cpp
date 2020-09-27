/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services            */
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
#include "core/wfndfile.h"

namespace wwiv::core {

static WFindFileTypeMask FindFilesTypeToInt(FindFiles::FindFilesType type) {
  switch (type) {
  case FindFiles::FindFilesType::any:
    return WFindFileTypeMask::WFINDFILE_ANY;
  case FindFiles::FindFilesType::directories:
    return WFindFileTypeMask::WFINDFILE_DIRS;
  case FindFiles::FindFilesType::files:
    return WFindFileTypeMask::WFINDFILE_FILES;
  }
  LOG(FATAL) << "Invalid FindFilesType: " << static_cast<int>(type);
  // Make Compiler happy
  return WFindFileTypeMask::WFINDFILE_ANY;
}

FindFiles::FindFiles(const std::filesystem::path& mask, const FindFilesType type, WinNameType name_type) {
  WFindFile fnd;
  if (name_type == WinNameType::long_name) {
    fnd.set_use_long_filenames(true);
  }
  if (!fnd.open(mask.string(), FindFilesTypeToInt(type))) {
    VLOG(3) << "Unable to open mask: " << mask;
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
    entries_.emplace(fn, fnd.GetFileSize());
  }
  while (fnd.next());
}

FindFiles::FindFiles(const std::filesystem::path& mask, FindFilesType type)
: FindFiles(mask, type, WinNameType::short_name) {}


}
