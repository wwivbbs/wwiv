/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services            */
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


/////////////////////////////////////////////////////////////////////////////
// Static functions

bool File::Copy(const std::string& source_filename, const std::string& dest_filename) {
  if (source_filename != dest_filename && File::Exists(source_filename) && !File::Exists(dest_filename)) {
    auto *buffer = static_cast<char *>(malloc(16400));
    if (buffer == nullptr) {
      return false;
    }
    int src_fd = open(source_filename.c_str(), O_RDONLY | O_BINARY);
    if (!src_fd) {
      free(buffer);
      return false;
    }

    int dest_fd = open(dest_filename.c_str(), O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
    if (!dest_fd) {
      free(buffer);
      close(src_fd);
      return false;
    }

    auto i = read(src_fd, buffer, 16384);

    while (i > 0) {
      write(dest_fd, buffer, static_cast<size_t>(i));
      i = read(src_fd, buffer, 16384);
    }

    close(src_fd);
    close(dest_fd);
    free(buffer);
  }

  // I'm not sure about the logic here since you would think we should return true
  // in the last block, and false here.  This seems fishy
  return true;
}

bool File::Move(const std::string& source_filename, const std::string& dest_filename) {
  if (Copy(source_filename, dest_filename)) {
    return Remove(source_filename);
  }
  return false;
}

bool File::canonical(const std::string& path, std::string* resolved) {
  if (resolved == nullptr) {
    return false;
  }

  auto result = ::realpath(path.c_str(), nullptr);
  resolved->assign(result);
  free(result);
  return true;
}

long File::freespace_for_path(const string& path) {
  struct statvfs fs{};
  if (statvfs(path.c_str(), &fs)) {
    perror("statfs()");
    return 0;
  }
  return static_cast<long>((long) fs.f_frsize * (double) fs.f_bavail / 1024);
}
