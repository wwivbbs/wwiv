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
#include "core/wfile.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#ifdef _WIN32
#include <io.h>
#include <share.h>
#endif  // _WIN32
#include <sstream>
#include <string>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/file.h>
#include <unistd.h>
#endif  // _WIN32

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef CopyFile
#undef GetFileTime
#undef GetFullPathName
#undef MoveFile
#endif  // _WIN32

#include "core/wfndfile.h"
#include "core/wwivassert.h"

#if defined( __APPLE__ )
#if !defined( O_BINARY )
#define O_BINARY 0
#endif
#endif

/////////////////////////////////////////////////////////////////////////////
// Constants

const int WFile::shareDenyReadWrite = SH_DENYRW;
const int WFile::shareDenyWrite     = SH_DENYWR;
const int WFile::shareDenyRead      = SH_DENYRD;
const int WFile::shareDenyNone      = SH_DENYNO;

const int WFile::permWrite          = _S_IWRITE;
const int WFile::permRead           = _S_IREAD;
const int WFile::permReadWrite      = (_S_IREAD | _S_IWRITE);

const char WFile::pathSeparatorChar = '\\';
const char WFile::separatorChar     = ';';

/////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

bool WFile::IsDirectory() {
  DWORD dwAttributes = GetFileAttributes(m_szFileName);
  return (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false;
}


/////////////////////////////////////////////////////////////////////////////
// Static functions

bool WFile::CopyFile(const std::string sourceFileName, const std::string destFileName) {
  return ::CopyFileA(sourceFileName.c_str(), destFileName.c_str(), FALSE) ? true : false;
}

bool WFile::MoveFile(const std::string sourceFileName, const std::string destFileName) {
  return ::MoveFileA(sourceFileName.c_str(), destFileName.c_str()) ? true : false;
}
