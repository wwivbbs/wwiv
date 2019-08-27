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
#ifdef _WIN32
// Always declare wwiv_windows.h first to avoid collisions on defines.
#include "core/wwiv_windows.h"

#include "Shlwapi.h"
#endif // _WIN32

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#ifdef _WIN32
#include "sys/utime.h"
#include <direct.h>
#include <io.h>
#include <share.h>

#else
#include <sys/file.h>
#include <unistd.h>
#include <utime.h>

#endif // _WIN32

#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>

#include "core/datetime.h"
#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/wfndfile.h"
#include "core/wwivassert.h"

#ifdef _WIN32

// Needed for PathIsRelative
#pragma comment(lib, "Shlwapi.lib")

#if !defined(ftruncate)
#define ftruncate chsize
#endif // ftruncate
#define flock(h, m)                                                                                \
  { (h), (m); }
static constexpr int LOCK_SH = 1;
static constexpr int LOCK_EX = 2;
static constexpr int LOCK_NB = 4;
static constexpr int LOCK_UN = 8;
static constexpr int F_OK = 0;

#define S_ISREG(m) (((m)&S_IFMT) == _S_IFREG)
#define S_ISDIR(m) (((m)&S_IFMT) == _S_IFDIR)

#else

// Not Win32
#define _sopen(n, f, s, p) open(n, f, 0644)

#endif // _WIN32

using std::string;
using std::chrono::milliseconds;
using namespace wwiv::os;
namespace fs = std::filesystem;

namespace wwiv {
namespace core {

/////////////////////////////////////////////////////////////////////////////
// Constants

const int File::modeDefault = (O_RDWR | O_BINARY);
const int File::modeAppend = O_APPEND;
const int File::modeBinary = O_BINARY;
const int File::modeCreateFile = O_CREAT;
const int File::modeReadOnly = O_RDONLY;
const int File::modeReadWrite = O_RDWR;
const int File::modeText = O_TEXT;
const int File::modeWriteOnly = O_WRONLY;
const int File::modeTruncate = O_TRUNC;
const int File::modeExclusive = O_EXCL;

const int File::modeUnknown = -1;
const int File::shareUnknown = -1;

const int File::invalid_handle = -1;

static const std::chrono::milliseconds wait_time(10);

static constexpr int TRIES = 100;

using namespace wwiv::strings;

string FilePath(const string& dirname, const string& filename) {
  return (dirname.empty()) ? filename : StrCat(File::EnsureTrailingSlash(dirname), filename);
}

bool backup_file(const std::string& filename) {
  fs::path from{filename};
  fs::path to{filename};
  to += StrCat(".backup.", DateTime::now().to_string("%Y%m%d%H%M%S"));
  VLOG(1) << "Backing up file: '" << from << "'; to: '" << to << "'";
  std::error_code ec;
  return wwiv::fs::copy_file(from, to, ec);
}

/////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

File::File(const string& full_file_name) : full_path_name_(full_file_name) {}

File::~File() {
  if (this->IsOpen()) {
    this->Close();
  }
}

bool File::Open(int file_mode, int share_mode) {
  DCHECK_EQ(this->IsOpen(), false) << "File " << full_path_name_ << " is already open.";

  // Set default share mode
  if (share_mode == File::shareUnknown) {
    share_mode = shareDenyWrite;
    if ((file_mode & File::modeReadWrite) || (file_mode & File::modeWriteOnly)) {
      share_mode = File::shareDenyReadWrite;
    }
  }

  CHECK_NE(share_mode, File::shareUnknown);
  CHECK_NE(file_mode, File::modeUnknown);

  VLOG(3) << "SH_OPEN " << full_path_name_ << ", access=" << file_mode;

  handle_ = _sopen(full_path_name_.string().c_str(), file_mode, share_mode, _S_IREAD | _S_IWRITE);
  if (handle_ < 0) {
    VLOG(3) << "1st _sopen: handle: " << handle_ << "; error: " << strerror(errno);
    int count = 1;
    if (access(full_path_name_.string().c_str(), 0) != -1) {
      sleep_for(wait_time);
      handle_ =
          _sopen(full_path_name_.string().c_str(), file_mode, share_mode, _S_IREAD | _S_IWRITE);
      while ((handle_ < 0 && errno == EACCES) && count < TRIES) {
        sleep_for((count % 2) ? wait_time : milliseconds(0));
        VLOG(3) << "Waiting to access " << full_path_name_ << "  " << TRIES - count;
        count++;
        handle_ =
            _sopen(full_path_name_.string().c_str(), file_mode, share_mode, _S_IREAD | _S_IWRITE);
      }

      if (handle_ < 0) {
        VLOG(3) << "The file " << full_path_name_ << " is busy.  Try again later.";
      }
    }
  }

  VLOG(3) << "SH_OPEN " << full_path_name_ << ", access=" << file_mode << ", handle=" << handle_;

  if (File::IsFileHandleValid(handle_)) {
    flock(handle_,
          (share_mode == shareDenyReadWrite || share_mode == shareDenyWrite) ? LOCK_EX : LOCK_SH);
  }

  if (handle_ == File::invalid_handle) {
    this->error_text_ = strerror(errno);
  }

  return File::IsFileHandleValid(handle_);
}

void File::Close() {
  VLOG(3) << "CLOSE " << full_path_name_ << ", handle=" << handle_;
  if (File::IsFileHandleValid(handle_)) {
    flock(handle_, LOCK_UN);
    close(handle_);
    handle_ = File::invalid_handle;
  }
}

/////////////////////////////////////////////////////////////////////////////
// Member functions

ssize_t File::Read(void* buffer, size_t size) {
  ssize_t ret = read(handle_, buffer, size);
  if (ret == -1) {
    LOG(ERROR) << "[DEBUG: Read errno: " << errno << " filename: " << full_path_name_
               << " size: " << size;
    LOG(ERROR) << " -- Please screen capture this and attach to a bug here: " << std::endl;
    LOG(ERROR) << "https://github.com/wwivbbs/wwiv/issues" << std::endl;
  }
  return ret;
}

ssize_t File::Write(const void* buffer, size_t size) {
  ssize_t r = write(handle_, buffer, size);
  if (r == -1) {
    LOG(ERROR) << "[DEBUG: Write errno: " << errno << " filename: " << full_path_name_
               << " size: " << size;
    LOG(ERROR) << " -- Please screen capture this and attach to a bug here: " << std::endl;
    LOG(ERROR) << "https://github.com/wwivbbs/wwiv/issues" << std::endl;
  }
  return r;
}

off_t File::Seek(off_t offset, Whence whence) {
  CHECK(whence == File::Whence::begin || whence == File::Whence::current ||
        whence == File::Whence::end);
  CHECK(File::IsFileHandleValid(handle_));

  return lseek(handle_, offset, static_cast<int>(whence));
}

off_t File::current_position() const { return lseek(handle_, 0, SEEK_CUR); }

bool File::Exists() const { return fs::exists(full_path_name_); }

bool File::Delete() {
  if (this->IsOpen()) {
    this->Close();
  }
  return fs::remove(full_path_name_);
}

void File::set_length(off_t lNewLength) {
  WWIV_ASSERT(File::IsFileHandleValid(handle_));
  auto _ = ftruncate(handle_, lNewLength);
}

bool File::IsFile() const { return !fs::is_directory(full_path_name_); }

bool File::SetFilePermissions(int perm) {
  return chmod(full_path_name_.string().c_str(), perm) == 0;
}

bool File::IsDirectory() const { return fs::is_directory(full_path_name_); }

// static
bool File::is_directory(const std::string& path) { return fs::is_directory(fs::path{path}); }

off_t File::length() {
  std::error_code ec;
  off_t sz = static_cast<off_t>(fs::file_size(full_path_name_, ec));
  if (ec.value() != 0) {
    return 0;
  }
  return sz;
}

time_t File::creation_time() {
  struct stat buf {};
  // st_ctime is creation time on windows and status change time on posix
  // so that's probably the closest to what we want.
  return (stat(full_path_name_.string().c_str(), &buf) == -1) ? 0 : buf.st_ctime;
}

time_t File::last_write_time() {
  struct stat buf {};
  return (stat(full_path_name_.string().c_str(), &buf) == -1) ? 0 : buf.st_mtime;
}

/////////////////////////////////////////////////////////////////////////////
// Static functions

bool File::Rename(const string& orig_fn, const string& new_fn) {
  fs::path o{orig_fn};
  fs::path n{new_fn};
  std::error_code ec{};
  fs::rename(o, n, ec);
  return ec.value() == 0;
}

bool File::Remove(const string& filename) {
  fs::path p{filename};
  return fs::remove(p);
}

bool File::Remove(const string& dir, const string& file) {
  fs::path p = FilePath(dir, file);
  return fs::remove(p);
}

bool File::Exists(const string& original_pathname) {
  if (original_pathname.empty()) {
    // An empty filename can not exist.
    // The question is should we assert here?
    return false;
  }

  fs::path p{original_pathname};
  return fs::exists(p);
}

// static
bool File::Exists(const string& dir, const string& file) {
  fs::path p{FilePath(dir, file)};
  return fs::exists(p);
}

// static
bool File::ExistsWildcard(const string& wildcard) {
  WFindFile fnd;
  return fnd.open(wildcard, WFindFileTypeMask::WFINDFILE_ANY);
}

bool File::SetFilePermissions(const string& filename, int perm) {
  CHECK(!filename.empty());
  return chmod(filename.c_str(), perm) == 0;
}

bool File::IsFileHandleValid(int handle) { return handle != File::invalid_handle; }

// static
std::string File::EnsureTrailingSlash(const std::string& orig) {
  if (orig.empty()) {
    return {};
  }
  if (orig.back() == File::pathSeparatorChar) {
    return orig;
  }
  std::string path{orig};
  path.push_back(File::pathSeparatorChar);
  return path;
}

// static
string File::current_directory() { return fs::current_path().string(); }

// static
bool File::set_current_directory(const string& dir) {
  std::error_code ec;
  fs::current_path({dir}, ec);
  return ec.value() == 0;
}

// static
std::string File::FixPathSeparators(const std::string& orig) {
  auto name = orig;
#ifdef _WIN32
  std::replace(std::begin(name), std::end(name), '/', File::pathSeparatorChar);
#else
  std::replace(std::begin(name), std::end(name), '\\', File::pathSeparatorChar);
#endif // _WIN32
  return name;
}

// static
string File::absolute(const std::string& base, const std::string& relative) {
  if (File::is_absolute(relative)) {
    return relative;
  }
  return FilePath(base, relative);
}

// static
bool File::is_absolute(const string& path) {
  if (path.empty()) {
    return false;
  }
  fs::path p{path};
  return p.is_absolute();
}

// static
bool File::mkdir(const string& s) {
  fs::path p = s;
  if (fs::exists(p)) {
    return true;
  }

  std::error_code ec;
  if (fs::create_directory(p, ec)) {
    return true;
  }
  return ec.value() == 0;
}

// static
bool File::mkdirs(const string& s) {
  fs::path p = s;
  if (fs::exists(p)) {
    return true;
  }
  std::error_code ec;
  if (fs::create_directories(p, ec)) {
    return true;
  }
  return ec.value() == 0;
}

std::ostream& operator<<(std::ostream& os, const File& file) {
  os << file.full_pathname();
  return os;
}

bool File::set_last_write_time(time_t last_write_time) {
  struct utimbuf ut {};
  ut.actime = ut.modtime = last_write_time;
  return utime(full_path_name_.string().c_str(), &ut) != -1;
}

std::unique_ptr<wwiv::core::FileLock> File::lock(wwiv::core::FileLockType lock_type) {
#ifdef _WIN32
  HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(handle_));
  OVERLAPPED overlapped = {0};
  DWORD dwLockType = 0;
  if (lock_type == wwiv::core::FileLockType::write_lock) {
    dwLockType = LOCKFILE_EXCLUSIVE_LOCK;
  }
  if (!::LockFileEx(h, dwLockType, 0, MAXDWORD, MAXDWORD, &overlapped)) {
    LOG(ERROR) << "Error Locking file: " << full_path_name_;
  }
#else

  // TODO: unlock here

#endif // _WIN32
  return std::make_unique<wwiv::core::FileLock>(handle_, full_path_name_.string(), lock_type);
}

bool File::Copy(const std::string& sourceFileName, const std::string& destFileName) {
  fs::path from{sourceFileName};
  fs::path to{destFileName};
  std::error_code ec;
  fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
  return ec.value() == 0;
}

bool File::Move(const std::string& sourceFileName, const std::string& destFileName) {
  fs::path from{sourceFileName};
  fs::path to{destFileName};
  std::error_code ec;
  fs::rename(from, to, ec);
  return ec.value() == 0;
}

// static
std::string File::canonical(const std::string& path) {
  fs::path p{path};
  return fs::canonical(p).string();
}

} // namespace core
} // namespace wwiv
