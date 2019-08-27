/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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
// Always declare wwiv_windows.h first to avoid collisions on defines.
#include "core/wwiv_windows.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sstream>
#include <string>
#include <sys/stat.h>

#include "core/wwivassert.h"

using std::string;
namespace fs = std::filesystem;

/////////////////////////////////////////////////////////////////////////////
// Constants

namespace wwiv {
namespace core {

const int File::shareDenyReadWrite = SH_DENYRW;
const int File::shareDenyWrite = SH_DENYWR;
const int File::shareDenyRead = SH_DENYRD;
const int File::shareDenyNone = SH_DENYNO;

const int File::permReadWrite = (_S_IREAD | _S_IWRITE);

const char File::pathSeparatorChar = '\\';
const char File::pathSeparatorString[] = "\\";
const char File::separatorChar = ';';

/////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

/////////////////////////////////////////////////////////////////////////////
// Member functions

/////////////////////////////////////////////////////////////////////////////
// Static functions

// static
std::string File::canonical(const std::string& path) {
  fs::path p{path};
  return fs::canonical(p).string();
}

// static
long File::freespace_for_path(const string& path) {
  uint64_t i64FreeBytesToCaller, i64TotalBytes, i64FreeBytes;
  BOOL result =
      GetDiskFreeSpaceEx(path.c_str(), reinterpret_cast<PULARGE_INTEGER>(&i64FreeBytesToCaller),
                         reinterpret_cast<PULARGE_INTEGER>(&i64TotalBytes),
                         reinterpret_cast<PULARGE_INTEGER>(&i64FreeBytes));
  return (result) ? static_cast<long>(i64FreeBytesToCaller / 1024) : 0;
}

} // namespace core
} // namespace wwiv
