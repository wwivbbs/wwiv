/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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
#include "core/wfndfile.h"

#include <cstring>
#include <string>

#include <dirent.h>
#include <fnmatch.h>
#include <libgen.h>
#include <limits.h>
#include <iostream>
#include "core/log.h"
#include "core/strings.h"
#include "core/wwivassert.h"

static long lTypeMask;
static const char* filespec_ptr;

#define TYPE_DIRECTORY  DT_DIR
#define TYPE_FILE DT_BLK

using std::string;
using namespace wwiv::strings;

//////////////////////////////////////////////////////////////////////////////
//
// Local functions
//
static int fname_ok(const struct dirent *ent) {
  if (wwiv::strings::IsEquals(ent->d_name, ".") ||
      wwiv::strings::IsEquals(ent->d_name, "..")) {
    return 0;
  }

  if (lTypeMask) {
    if ((ent->d_type & TYPE_DIRECTORY) && !(lTypeMask & WFINDFILE_DIRS)) {
      return 0;
    } else if ((ent->d_type & TYPE_FILE) && !(lTypeMask & WFINDFILE_FILES)) {
      return 0;
    }
  }

  int result = fnmatch(filespec_ptr, ent->d_name, FNM_PATHNAME|FNM_CASEFOLD);
  std::cerr << "fnmatch: " << filespec_ptr << ";" << ent->d_name << "; " << result << "\r\n";

  if (result == 0) {
    // fnmatch returns 0 on match. We return nonzero.
    return 1;
  }
  return 0;
}

bool WFindFile::open(const string& filespec, unsigned int nTypeMask) {
  string dir;

  __open(filespec, nTypeMask);
  filename_.clear();

  {
    char path[FILENAME_MAX];
    to_char_array(path, filespec);
    dir = dirname(path);
  }
  {
    char path[FILENAME_MAX];
    to_char_array(path, filespec);
    filespec_ = basename(path);
    filespec_ptr = filespec_.c_str();
  }

  nMatches = scandir(dir.c_str(), &entries, fname_ok, alphasort);
  if (nMatches < 0) {
    std::cout << "could not open dir '" << dir << "'\r\n";
    perror("scandir");
    return false;
  }
  nCurrentEntry = 0;

  next();
  return nMatches > 0;
}

bool WFindFile::next() {
  if (nCurrentEntry >= nMatches) {
    return false;
  }
  struct dirent *entry = entries[nCurrentEntry++];

  filename_ = entry->d_name;
  file_size_ = entry->d_reclen;
  nFileType = entry->d_type;

  return true;
}

bool WFindFile::close() {
  __close();
  return true;
}

bool WFindFile::IsDirectory() {
  if (nCurrentEntry > nMatches) {
    return false;
  }

  return (nFileType & DT_DIR);
}

bool WFindFile::IsFile() {
  if (nCurrentEntry > nMatches) {
    return false;
  }

  return (nFileType & DT_REG);
}


