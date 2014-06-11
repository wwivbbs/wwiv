/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include "platform/wfndfile.h"

bool WFindFile::open(const char * pszFileSpec, UINT32 nTypeMask) {
  __open(pszFileSpec, nTypeMask);
  // Set this with the initial value
  hFind = INVALID_HANDLE_VALUE;

  hFind = FindFirstFile(pszFileSpec, &ffdata);
  if (hFind == INVALID_HANDLE_VALUE) {
    return false;
  }

  if (ffdata.cAlternateFileName[0] == '\0') {
    strcpy(szFileName, ffdata.cFileName);
  } else {
    strcpy(szFileName, ffdata.cAlternateFileName);
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
    strcpy(szFileName, ffdata.cFileName);
  } else {
    strcpy(szFileName, ffdata.cAlternateFileName);
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
  if (IsFile()) {
    return false;
  }
  return true;
}

bool WFindFile::IsFile() {
  if (ffdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    return false;
  }
  return true;
}

