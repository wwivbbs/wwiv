/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
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

#include <iostream>
#include "core/strings.h"
#include "core/wwivassert.h"

bool WFindFile::open(const std::string& file_spec, unsigned int nTypeMask) {
  __open(file_spec, nTypeMask);

  hFind = FindFirstFile(file_spec.c_str(), &ffdata);
  if (hFind == INVALID_HANDLE_VALUE) {
    return false;
  }

  if (ffdata.cAlternateFileName[0] == '\0') {
    filename_ = ffdata.cFileName;
  } else {
    filename_ = ffdata.cAlternateFileName;
  }
  lFileSize = (ffdata.nFileSizeHigh * MAXDWORD) + ffdata.nFileSizeLow;
  return true;
}

bool WFindFile::next() {
  if (!FindNextFile(hFind, &ffdata)) {
    return false;
  }
  if (hFind == INVALID_HANDLE_VALUE) {
    return false;
  }

  if (ffdata.cAlternateFileName[0] == '\0') {
    filename_ = ffdata.cFileName;
  } else {
    filename_ = ffdata.cAlternateFileName;
  }
  lFileSize = (ffdata.nFileSizeHigh * MAXDWORD) + ffdata.nFileSizeLow;
  return true;
}

bool WFindFile::close() {
  __close();
  FindClose(hFind);
  return true;
}

bool WFindFile::IsDirectory() {
  return !IsFile();
}

bool WFindFile::IsFile() {
  return (ffdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? false : true;
}
