/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015,WWIV Software Services             */
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
#include "core/file.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <string>
#include <sstream>
#include <string>
#include <sys/stat.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core/wfndfile.h"
#include "core/wwivassert.h"

using std::string;

/////////////////////////////////////////////////////////////////////////////
// Constants

const int File::shareDenyReadWrite = SH_DENYRW;
const int File::shareDenyWrite     = SH_DENYWR;
const int File::shareDenyRead      = SH_DENYRD;
const int File::shareDenyNone      = SH_DENYNO;

const int File::permReadWrite      = (_S_IREAD | _S_IWRITE);

const char File::pathSeparatorChar = '\\';
const char File::pathSeparatorString[] = "\\";
const char File::separatorChar     = ';';

/////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

bool File::IsDirectory() const {
  DWORD dwAttributes = GetFileAttributes(full_path_name_.c_str());
  return (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false;
}

/////////////////////////////////////////////////////////////////////////////
// Static functions

bool File::Copy(const std::string& sourceFileName, const std::string& destFileName) {
  return ::CopyFileA(sourceFileName.c_str(), destFileName.c_str(), FALSE) ? true : false;
}

bool File::Move(const std::string& sourceFileName, const std::string& destFileName) {
  return ::MoveFileA(sourceFileName.c_str(), destFileName.c_str()) ? true : false;
}

bool File::RealPath(const std::string& path, std::string* resolved) {
  const int BUFSIZE = 4096;
  CHAR szBuffer[BUFSIZE];
  CHAR** lppPart = { nullptr };

  DWORD result = GetFullPathName(path.c_str(), BUFSIZE, szBuffer, lppPart);
  if (result == 0) {
    resolved->assign(path);
    return false;
  }

  resolved->assign(szBuffer);
  return true;
}

// static
long File::GetFreeSpaceForPath(const string& path) {
  uint64_t i64FreeBytesToCaller, i64TotalBytes, i64FreeBytes;
  BOOL result = GetDiskFreeSpaceEx(path.c_str(),
      reinterpret_cast<PULARGE_INTEGER>(&i64FreeBytesToCaller),
      reinterpret_cast<PULARGE_INTEGER>(&i64TotalBytes),
      reinterpret_cast<PULARGE_INTEGER>(&i64FreeBytes));
  return (result) ? static_cast<long>(i64FreeBytesToCaller / 1024) : 0;
}
