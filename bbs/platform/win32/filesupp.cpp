/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014,WWIV Software Services             */
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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>

#include <string>

#include "bbs.h"
#include "core/wwivport.h"
#include "core/strings.h"

long WWIV_WIN32_FreeSpaceForDriveLetter(int nDrive);

/**
 * Returns the free disk space for a drive letter, where nDrive is the drive number
 * 1 = 'A', 2 = 'B', etc.
 *
 * @param nDrive The drive number to get the free disk space for.
 */
// GetDiskFreeSpaceEx
// GetDriveType
long WWIV_WIN32_FreeSpaceForDriveLetter(int nDrive) {
  char s[] = "X:\\";
  s[0] = static_cast< char >('A' + static_cast< char >(nDrive - 1));
  char *pszDrive = (nDrive) ? s : nullptr;
  UINT driveType = GetDriveType(pszDrive);

  // check if the drive exists
  if (driveType == DRIVE_UNKNOWN || driveType == DRIVE_NO_ROOT_DIR) {
    return 0;
  }

  unsigned __int64 i64FreeBytesToCaller, i64TotalBytes, i64FreeBytes;
  BOOL result = GetDiskFreeSpaceEx(pszDrive,
      reinterpret_cast<PULARGE_INTEGER>(&i64FreeBytesToCaller),
      reinterpret_cast<PULARGE_INTEGER>(&i64TotalBytes),
      reinterpret_cast<PULARGE_INTEGER>(&i64FreeBytes));
  if (result) {
    return static_cast<long>(i64FreeBytesToCaller / 1024);
  }

  return 0;
}

long WWIV_GetFreeSpaceForPath(const char * szPath) {
#ifndef NOT_BBS
  int nDrive = GetApplication()->GetHomeDir()[0];
  if (szPath[1] == ':') {
    nDrive = szPath[0];
  }
  nDrive = wwiv::UpperCase<int> (nDrive - 'A' + 1);
  return WWIV_WIN32_FreeSpaceForDriveLetter(nDrive);
#else
  return 0;
#endif
}

void WWIV_GetFileNameFromPath(const char *pszPath, char *pszFileName) {
  _splitpath(pszPath, nullptr, nullptr, pszFileName, nullptr);
}
