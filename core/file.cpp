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

string FilePath(const std::filesystem::path& directory_name, const string& file_name) {
  if (directory_name.empty()) {
    return file_name;
  }
  return PathFilePath(directory_name, file_name).string();
}

std::filesystem::path PathFilePath(const std::filesystem::path& directory_name,
                                   const string& file_name) {
  if (directory_name.empty()) {
    return file_name;
  }
  return directory_name / file_name;
}

bool backup_file(const std::filesystem::path& p) {
  fs::path from{p};
  fs::path to{p};
  to += StrCat(".backup.", DateTime::now().to_string("%Y%m%d%H%M%S"));
  VLOG(1) << "Backing up file: '" << from << "'; to: '" << to << "'";
  std::error_code ec;
  return wwiv::fs::copy_file(from, to, ec);
}

/////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

// File::File(const string& full_file_name) : full_path_name_(full_file_name) {}

/** Constructs a file from a path. */
File::File(const std::filesystem::path& p) : full_path_name_(p) {}

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

bool File::IsOpen() const noexcept { return File::IsFileHandleValid(handle_); }

void File::Close() noexcept {
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

bool File::Exists() const noexcept {
  std::error_code ec;
  return fs::exists(full_path_name_, ec);
}

void File::set_length(off_t l) {
  WWIV_ASSERT(File::IsFileHandleValid(handle_));
  auto _ = ftruncate(handle_, l);
}

// static
bool File::is_directory(const std::string& path) {
  std::error_code ec;
  return fs::is_directory(fs::path{path}, ec);
}

off_t File::length() {
  std::error_code ec;
  auto sz = static_cast<off_t>(fs::file_size(full_path_name_, ec));
  if (ec.value() != 0) {
    return 0;
  }
  return sz;
}

time_t File::creation_time() { return File::creation_time(full_path_name_); }

time_t File::last_write_time() { return File::last_write_time(full_path_name_); }

/////////////////////////////////////////////////////////////////////////////
// Static functions

// static
time_t File::last_write_time(const std::filesystem::path& path) {
  struct stat buf {};
  const auto p = path.string();
  return (stat(p.c_str(), &buf) == -1) ? 0 : buf.st_mtime;
}

// static
time_t File::creation_time(const std::filesystem::path& path) {
  struct stat buf {};
  // st_ctime is creation time on windows and status change time on posix
  // so that's probably the closest to what we want.
  const auto p = path.string();
  return (stat(p.c_str(), &buf) == -1) ? 0 : buf.st_ctime;
}

bool File::Rename(const std::filesystem::path& o, const std::filesystem::path& n) {
  std::error_code ec{};
  fs::rename(o, n, ec);
  return ec.value() == 0;
}

bool File::Remove(const std::filesystem::path& filename) {
  std::error_code ec;
  bool result = fs::remove(filename, ec);
  if (!result) {
    LOG(ERROR) << "File::Remove failed: error code: " << ec.value() << "; msg: " << ec.message();
  }
  return result;
}

bool File::Exists(const std::filesystem::path& p) {
  if (p.empty()) {
    // An empty filename can not exist.
    // The question is should we assert here?
    return false;
  }

  std::error_code ec;
  return fs::exists(p, ec);
}

// static
bool File::ExistsWildcard(const std::filesystem::path& wildcard) {
  WFindFile fnd;
  return fnd.open(wildcard.string(), WFindFileTypeMask::WFINDFILE_ANY);
}

bool File::SetFilePermissions(const std::filesystem::path& filename, int perm) {
  CHECK(!filename.empty());
  return chmod(filename.string().c_str(), perm) == 0;
}

bool File::IsFileHandleValid(int handle) noexcept { return handle != File::invalid_handle; }

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
std::string File::EnsureTrailingSlashPath(const std::filesystem::path& orig) {
  if (orig.empty()) {
    return {};
  }
  std::string path{orig.string()};
  if (path.back() == File::pathSeparatorChar) {
    return path;
  }
  path.push_back(File::pathSeparatorChar);
  return path;
}

// static
std::filesystem::path File::current_directory() {
  std::error_code ec;
  return fs::current_path(ec);
}

// static
bool File::set_current_directory(const std::filesystem::path& dir) {
  std::error_code ec;
  fs::current_path(dir, ec);
  return ec.value() == 0;
}

// static
std::string File::FixPathSeparators(const std::string& orig) {
  fs::path p{orig};
  return p.make_preferred().string();
}

// static
string File::absolute(const std::string& base, const std::string& relative) {
  fs::path r{relative};
  if (r.is_absolute()) {
    return relative;
  }
  return FilePath(base, relative);
}

// static
bool File::mkdir(const std::filesystem::path& p) {
  std::error_code ec;
  if (fs::exists(p, ec)) {
    return true;
  }

  if (fs::create_directory(p, ec)) {
    return true;
  }
  return ec.value() == 0;
}

// static
bool File::mkdirs(const std::filesystem::path& p) {
  std::error_code ec;
  if (fs::exists(p, ec)) {
    return true;
  }
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

bool File::Copy(const std::filesystem::path& from, const std::filesystem::path& to) {
  std::error_code ec;
  fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
  return ec.value() == 0;
}

bool File::Move(const std::filesystem::path& from, const std::filesystem::path& to) {
  return File::Rename(from, to);
}

// static
std::string File::canonical(const std::string& path) {
  fs::path p{path};
  std::error_code ec;
  return fs::canonical(p, ec).string();
}

long File::freespace_for_path(const std::filesystem::path& p) {
  std::error_code ec;
  auto devi = fs::space(p, ec);
  if (ec.value() != 0) {
    return 0;
  }
  return static_cast<long>(devi.available / 1024);
}

} // namespace core
} // namespace wwiv
