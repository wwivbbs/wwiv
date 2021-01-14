/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services            */
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

#include "core/strings.h"
#include "core/wwiv_windows.h"

bool WFindFile::open(const std::string& file_spec, WFindFileTypeMask nTypeMask) {
  ffdata_ =  std::make_any<WIN32_FIND_DATA>();
  __open(file_spec, nTypeMask);

  auto* f = std::any_cast<WIN32_FIND_DATA>(&ffdata_);

  hFind = FindFirstFile(file_spec.c_str(), f);
  if (hFind == INVALID_HANDLE_VALUE) {
    return false;
  }

  if (use_long_filenames_ || f->cAlternateFileName[0] == '\0') {
    filename_ = f->cFileName;
  } else {
    filename_ = f->cAlternateFileName;
  }
  file_size_ = f->nFileSizeHigh * MAXDWORD + std::any_cast<
                 WIN32_FIND_DATA>(ffdata_).nFileSizeLow;
  return true;
}

bool WFindFile::next() {
  auto* f = std::any_cast<WIN32_FIND_DATA>(&ffdata_);
  if (!FindNextFile(hFind, f)) {
    return false;
  }
  if (hFind == INVALID_HANDLE_VALUE) {
    return false;
  }

  if (use_long_filenames_ || f->cAlternateFileName[0] == '\0') {
    filename_ = f->cFileName;
  } else {
    filename_ = f->cAlternateFileName;
  }
  file_size_ = static_cast<decltype(file_size_)>((f->nFileSizeHigh * MAXDWORD) + f->nFileSizeLow);
  return true;
}

bool WFindFile::close() {
  __close();
  FindClose(hFind);
  return true;
}

bool WFindFile::IsDirectory() const {
  return !IsFile();
}

bool WFindFile::IsFile() const {
  const auto* f = std::any_cast<WIN32_FIND_DATA>(&ffdata_);
  return f->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY
           ? false
           : true;
}
