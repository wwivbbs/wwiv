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
#include "core/file_lock.h"
#ifdef _WIN32
// Always declare wwiv_windows.h first to avoid collisions on defines.
#include "core/wwiv_windows.h"
#include "Shlwapi.h"
#endif  // _WIN32

#include "core/log.h"
#include "core/os.h"
#include <algorithm>
#include <string>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else // _WIN32
#include <sys/file.h>
#include <unistd.h>
#include <utime.h>
#endif // _WIN32

using std::chrono::milliseconds;
using namespace wwiv::os;


namespace wwiv {
namespace core {

FileLock::FileLock(int fd, const std::string& filename, FileLockType lock_type)
  : fd_(fd), filename_(filename), lock_type_(lock_type) {
}

FileLock::~FileLock() {
#ifdef _WIN32
  auto h = reinterpret_cast<HANDLE>(_get_osfhandle(fd_));
  OVERLAPPED overlapped = {0};
  if (!UnlockFileEx(h, 0, MAXDWORD, MAXDWORD, &overlapped)) {
    LOG(ERROR) << "Error Unlocking file: " << filename_;
  }
#else

  // TODO: unlock here

#endif  // _WIN32
}


} // namespace core
} // namespace wwiv
