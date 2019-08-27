/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services            */
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

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/statfs.h>
#include <sys/vfs.h>

#ifdef __linux__
#include <sys/statvfs.h>
#endif  // __linux__

#include "core/wwivassert.h"

using std::string;

namespace wwiv {
namespace core {


/////////////////////////////////////////////////////////////////////////////
// Constants

const int File::shareDenyReadWrite = S_IWRITE;
const int File::shareDenyWrite     = 0;
const int File::shareDenyRead      = S_IREAD;
const int File::shareDenyNone      = 0;

const int File::permReadWrite      = O_RDWR;

const char File::pathSeparatorChar = '/';
const char File::pathSeparatorString[] = "/";

const char File::separatorChar     = ':';

// static
std::string File::canonical(const std::string& path) {
  auto result = ::realpath(path.c_str(), nullptr);
  std::string c{result};
  free(result);
  return c;
}

// static
long File::freespace_for_path(const string& path) {
  struct statvfs fs{};
  if (statvfs(path.c_str(), &fs)) {
    perror("statfs()");
    return 0;
  }
  return static_cast<long>((long) fs.f_frsize * (double) fs.f_bavail / 1024);
}

}
}
