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
#include <sys/stat.h>
#include "core/log.h"
#include "core/strings.h"
#include "core/wwivassert.h"

static WFindFileTypeMask s_typemask;
static const char* filespec_ptr;
static std::string s_path;

using std::string;
using namespace wwiv::strings;

//////////////////////////////////////////////////////////////////////////////
//
// Local functions
//

#if defined(__sun)
#define TYPE_UNKNOWN   0
#define TYPE_DIRECTORY 1
#define TYPE_FILE      2
#endif

static int fname_ok(const struct dirent *ent) {
  if (IsEquals(ent->d_name, ".") || IsEquals(ent->d_name, "..")) {
    return 0;
  }

  if (s_typemask != WFindFileTypeMask::WFINDFILE_ANY) {
    mode_t mode = 0;
#ifdef _DIRENT_HAVE_D_TYPE
    mode = DTTOIF(ent->d_type);
#else
    struct stat s;
    std::string fullpath = s_path + "/" + ent->d_name;
    stat(fullpath.c_str(), &s);
    mode = s.st_mode;
#endif  // _DIRENT_HAVE_D_TYPE
    if ((S_ISDIR(mode)) && !(s_typemask == WFindFileTypeMask::WFINDFILE_DIRS)) {
      return 0;
    } else if ((S_ISREG(mode)) && !(s_typemask == WFindFileTypeMask::WFINDFILE_FILES)) {
      return 0;
    }
  }
#if defined(__sun)
  int result = fnmatch(filespec_ptr, ent->d_name, FNM_PATHNAME | FNM_IGNORECASE);
#else
  int result = fnmatch(filespec_ptr, ent->d_name, FNM_PATHNAME | FNM_CASEFOLD);
#endif
  VLOG(3) << "fnmatch: " << filespec_ptr << ";" << ent->d_name << "; " << result << "\r\n";

  if (result == 0) {
    // fnmatch returns 0 on match. We return nonzero.
    return 1;
  }
  return 0;
}

bool WFindFile::open(const string& filespec, WFindFileTypeMask nTypeMask) {
  __open(filespec, nTypeMask);
  filename_.clear();

  {
    char path[FILENAME_MAX];
    to_char_array(path, filespec);
    dir_ = dirname(path);
  }
  {
    char path[FILENAME_MAX];
    to_char_array(path, filespec);
    filespec_ = basename(path);
    filespec_ptr = filespec_.c_str();
  }

  s_typemask = type_mask_;
  s_path = dir_;
  nMatches = scandir(dir_.c_str(), &entries, fname_ok, alphasort);
  if (nMatches < 0) {
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

#ifdef _DIRENT_HAVE_D_TYPE
  file_type_ = entry->d_type;

#else
  struct stat s;
  string fullpath = dir_ + "/" + entry->d_name;
  if (stat(fullpath.c_str(), &s) == 0) {
#if defined(__sun)
    if (S_ISDIR(s.st_mode)) {
      file_type_ = TYPE_DIRECTORY;
    } else if (S_ISREG(s.st_mode)) {
      file_type_ = TYPE_FILE;
    } else {
      file_type_ = TYPE_UNKNOWN;
    }
  } else {
    file_type_ = TYPE_UNKNOWN;
  }
#else
    file_type_ = IFTODT(s.st_mode);
  } else {
    file_type_ = DT_UNKNOWN;
  }
#endif // __sun
#endif  // _DIRENT_HAVE_D_TYPE

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
#if defined(__sun)
  return (file_type_ == TYPE_DIRECTORY);
#else
  return (file_type_ & DT_DIR);
#endif
}

bool WFindFile::IsFile() {
  if (nCurrentEntry > nMatches) {
    return false;
  }
#if defined(__sun)
  return (file_type_ == TYPE_FILE);
#else
  return (file_type_ & DT_REG);
#endif
}


