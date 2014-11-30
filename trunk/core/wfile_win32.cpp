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
#include "core/wfile.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>

#include <io.h>
#include <share.h>

#include <sstream>
#include <string>
#include <sys/stat.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core/wfndfile.h"
#include "core/wwivassert.h"

/////////////////////////////////////////////////////////////////////////////
// Constants

const int WFile::shareDenyReadWrite = SH_DENYRW;
const int WFile::shareDenyWrite     = SH_DENYWR;
const int WFile::shareDenyRead      = SH_DENYRD;
const int WFile::shareDenyNone      = SH_DENYNO;

const int WFile::permReadWrite      = (_S_IREAD | _S_IWRITE);

const char WFile::pathSeparatorChar = '\\';
const char WFile::pathSeparatorString[] = "\\";
const char WFile::separatorChar     = ';';

/////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

bool WFile::IsDirectory() {
  DWORD dwAttributes = GetFileAttributes(full_path_name_.c_str());
  return (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false;
}

/////////////////////////////////////////////////////////////////////////////
// Static functions

bool WFile::Copy(const std::string sourceFileName, const std::string destFileName) {
  return ::CopyFileA(sourceFileName.c_str(), destFileName.c_str(), FALSE) ? true : false;
}

bool WFile::Move(const std::string sourceFileName, const std::string destFileName) {
  return ::MoveFileA(sourceFileName.c_str(), destFileName.c_str()) ? true : false;
}
