/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services            */
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

#include "core/datetime.h"
#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/wfndfile.h"
#include <cerrno>
#include <cstring>
#include <string>
#include "core/findfiles.h"

// Keep all of these
#ifdef _WIN32
// This makes it clear that we want the POSIX names without
// leading underscores  This makes resharper happy with fcntl.h too.
#define _CRT_DECLARE_NONSTDC_NAMES 1  
#endif // _WIN32

#include <fcntl.h>
#include <sys/stat.h>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include "sys/utime.h"
#include <io.h>

#else
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#endif // _WIN32


#ifdef _WIN32
#include "core/wwiv_windows.h"

static int flock(int, int) { return 0; }

static constexpr int LOCK_SH = 1;
static constexpr int LOCK_EX = 2;
//static constexpr int LOCK_NB = 4;
static constexpr int LOCK_UN = 8;

#else

// Not Win32
#define _sopen(n, f, s, p) open(n, f, 0644)

#endif // _WIN32

using std::chrono::milliseconds;
using namespace wwiv::os;
using namespace std::filesystem;

namespace wwiv::core {

/////////////////////////////////////////////////////////////////////////////
// Constants

const int File::modeDefault = O_RDWR | O_BINARY;
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

static const milliseconds wait_time(10);

static constexpr int TRIES = 100;

using namespace strings;

path FilePath(const path& directory_name, const path& file_name) {
  if (directory_name.empty()) {
    return file_name;
  }
  if (File::is_absolute(file_name)) {
    LOG(INFO) << "Passed absolute filename to FilePath: " << file_name;
    // TODO(rushfan): here once we are sure this won't break things.
    // return file_name; 
  }
  return directory_name / file_name;
}

void trim_backups(const path& from, int max_backups) {
  auto mask{from};
  mask += ".backup.*";
  FindFiles ff(mask, FindFiles::FindFilesType::files, FindFiles::WinNameType::long_name);
  if (!from.has_filename()) {
    LOG(WARNING) << "Called trim_backups on file without a filename: '" << from.string() << "'";
    return;
  }

  const auto tot = static_cast<int>(ff.size());
  if (tot <= max_backups) {
    return;
  }
  auto num_to_remove = tot - max_backups;
  for (const auto& f : ff) {
    if (num_to_remove-- == 0) {
      break;
    }
    auto file{from};
    VLOG(1) << "Delete backup: " << file.replace_filename(f.name);
    File::Remove(file.replace_filename(f.name));
  }
}

bool backup_file(const path& from, int max_backups) {
  auto to{from};
  to += StrCat(".backup.", DateTime::now().to_string("%Y%m%d%H%M%S"));
  VLOG(1) << "Backing up file: '" << from << "'; to: '" << to << "'";
  std::error_code ec;
  if (!copy_file(from, to, ec)) {
    return false;
  }
  if (max_backups > 0) {
    trim_backups(from, max_backups);
  }
  return true;
}

/////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

// File::File(const string& full_file_name) : full_path_name_(full_file_name) {}

/** Constructs a file from a path. */
File::File(std::filesystem::path full_path_name)
  : full_path_name_(std::move(full_path_name)) {
}

File::File(File&& other) noexcept
  : handle_(other.handle_) {
  other.handle_ = -1;
  full_path_name_.swap(other.full_path_name_);
  error_text_.swap(other.error_text_);
}

File& File::operator=(File&& other) noexcept {
  if (this != &other) {
    handle_ = other.handle_;
    full_path_name_.swap(other.full_path_name_);
    error_text_.swap(other.error_text_);
    other.handle_ = -1;
  }
  return *this;
}


File::~File() {
  if (this->IsOpen()) {
    this->Close();
  }
}

bool File::Open(int file_mode, int share_mode) {
  DCHECK_EQ(this->IsOpen(), false) << "File " << full_path_name_ << " is already open.";

  // Set default share mode
  if (share_mode == shareUnknown) {
    share_mode = shareDenyWrite;
    if (file_mode & modeReadWrite || file_mode & modeWriteOnly) {
      share_mode = shareDenyReadWrite;
    }
  }

  CHECK_NE(share_mode, File::shareUnknown);
  CHECK_NE(file_mode, File::modeUnknown);

  VLOG(5) << "File::Open (before _sopen) " << full_path_name_ << ", access=" << file_mode;

#if defined(__OS2__)
  if (file_mode & O_CREAT) {
    // See https://lists.mysql.com/internals/312
    VLOG(4) << "Using OS/2 O_CREAT path";
    handle_ = open(full_path_name_.string().c_str(), file_mode, S_IREAD | S_IWRITE);
    if (handle_ == invalid_handle) {
      this->error_text_ = strerror(errno);
    }
    
    return IsFileHandleValid(handle_);
  }
#endif  // __OS2__

  handle_ = _sopen(full_path_name_.string().c_str(), file_mode, share_mode, _S_IREAD | _S_IWRITE);
  if (handle_ < 0) {
    VLOG(4) << "1st _sopen: handle: " << handle_ << "; error: " << strerror(errno);
    auto count = 1;
    if (access(full_path_name_.string().c_str(), 0) != -1) {
      sleep_for(wait_time);
      handle_ =
          _sopen(full_path_name_.string().c_str(), file_mode, share_mode, _S_IREAD | _S_IWRITE);
      while (handle_ < 0 && errno == EACCES && count < TRIES) {
        sleep_for(count % 2 ? wait_time : milliseconds(0));
        VLOG(4) << "Waiting to access " << full_path_name_ << "  " << TRIES - count;
        count++;
        handle_ =
            _sopen(full_path_name_.string().c_str(), file_mode, share_mode, _S_IREAD | _S_IWRITE);
      }

      if (handle_ < 0) {
        VLOG(4) << "The file " << full_path_name_ << " is busy.  Try again later.";
      }
    }
  }

  VLOG(3) << "File::Open '" << full_path_name_ << "', access=" << file_mode << ", handle=" << handle_;

  if (IsFileHandleValid(handle_)) {
    flock(handle_,
          (share_mode == shareDenyReadWrite || share_mode == shareDenyWrite) ? LOCK_EX : LOCK_SH);
  }

  if (handle_ == invalid_handle) {
    this->error_text_ = strerror(errno);
  }

  return IsFileHandleValid(handle_);
}

bool File::IsOpen() const noexcept { return IsFileHandleValid(handle_); }

void File::Close() noexcept {
  VLOG(4) << "CLOSE " << full_path_name_ << ", handle=" << handle_;
  if (IsFileHandleValid(handle_)) {
    flock(handle_, LOCK_UN);
    close(handle_);
    handle_ = invalid_handle;
  }
}

/////////////////////////////////////////////////////////////////////////////
// Member functions

// ReSharper disable once CppMemberFunctionMayBeConst
File::size_type File::Read(void* buffer, File::size_type size) {
  const auto ret = read(handle_, buffer, static_cast<unsigned int>(size));
  if (ret == -1) {
    LOG(ERROR) << "[DEBUG]: Read errno: " << errno << " filename: " << full_path_name_
        << " size: " << size;
    LOG(ERROR) << "Error String:        " << strerror(errno);
#ifdef _WIN32
    LOG(ERROR) << "Error String (DOS):  " << strerror(_doserrno);
#endif 
    LOG(ERROR) << " -- Please screen capture this and attach to a bug here: " << std::endl;
    LOG(ERROR) << "https://github.com/wwivbbs/wwiv/issues" << std::endl;
  }
  return ret;
}

// ReSharper disable once CppMemberFunctionMayBeConst
File::size_type File::Write(const void* buffer, File::size_type size) {
  const auto r = write(handle_, buffer, static_cast<unsigned int>(size));
  if (r == -1) {
    LOG(ERROR) << "[DEBUG: Write errno: " << errno << " filename: " << full_path_name_
        << " size: " << size;
    LOG(ERROR) << "Error String:        " << strerror(errno);
#ifdef _WIN32
    LOG(ERROR) << "Error String (DOS):  " << strerror(_doserrno);
#endif 
    LOG(ERROR) << " -- Please screen capture this and attach to a bug here: " << std::endl;
    LOG(ERROR) << "https://github.com/wwivbbs/wwiv/issues" << std::endl;
  }
  return r;
}

// ReSharper disable once CppMemberFunctionMayBeConst
File::size_type File::Seek(size_type offset, Whence whence) {
  CHECK(File::IsFileHandleValid(handle_));
  CHECK(whence == File::Whence::begin || whence == File::Whence::current ||
      whence == File::Whence::end);

  return static_cast<size_type>(lseek(handle_, static_cast<long>(offset), static_cast<int>(whence)));
}

File::size_type File::current_position() const { return lseek(handle_, 0, SEEK_CUR); }

bool File::Exists() const noexcept {
  std::error_code ec;
  return exists(full_path_name_, ec);
}

// ReSharper disable once CppMemberFunctionMayBeConst
bool File::set_length(size_type l) {
  if (IsOpen()) {
#if defined (_WIN32) 
    return _chsize_s(handle_, l) == 0;
#else
    return ftruncate(handle_, l) == 0;
#endif
  }

  std::error_code ec;
  if (resize_file(full_path_name_, l, ec); ec.value() != 0) {
    LOG(WARNING) << "Errror on resize_file: '" << full_path_name_ << "': " << ec.value() << "; "
                 << ec.message() << "; open: " << IsOpen();
    return false;
  }
  return true;
}

// static
bool File::is_directory(const std::filesystem::path& path) noexcept {
  std::error_code ec;
  return std::filesystem::is_directory(path, ec);
}

File::size_type File::length() const noexcept {
  std::error_code ec;
  const auto sz = static_cast<size_type>(file_size(full_path_name_, ec));
  if (ec.value() != 0) {
    return 0;
  }
  return sz;
}

time_t File::last_write_time() const { return last_write_time(full_path_name_); }

/////////////////////////////////////////////////////////////////////////////
// Static functions


// static
time_t File::creation_time(const std::filesystem::path& path) {
  const auto p = path.string();
  // Stick with calling stat vs. filesystem:last_write_time until C++20 since
  // C++20 will allow portable output
  struct stat buf {};
  return stat(p.c_str(), &buf) == -1 ? 0 : buf.st_ctime;
}

// static
time_t File::last_write_time(const std::filesystem::path& path) {
  const auto p = path.string();
  // Stick with calling stat vs. filesystem:last_write_time until C++20 since
  // C++20 will allow portable output
  struct stat buf {};
  return stat(p.c_str(), &buf) == -1 ? 0 : buf.st_mtime;
}

bool File::Rename(const std::filesystem::path& o, const std::filesystem::path& n) {
  if (o == n) {
    // Nothing to do.
    return true;
  }
  std::error_code ec{};
  std::filesystem::rename(o, n, ec);
  return ec.value() == 0;
}

bool File::Remove(const std::filesystem::path& path, bool force) {
  if (!Exists(path)) {
    // Don't try to delete a file that doesn't exist.
    return true;
  }

  if (force) {
    // Reset permissions to read/write, some apps set funky permissions
    // that keep unlink from working.
    SetFilePermissions(path, permReadWrite);
  }
  std::error_code ec;
  const auto result = std::filesystem::remove(path, ec);
  if (!result) {
    LOG(ERROR) << "File::Remove failed: " << path.string() << "; error code: " << ec.value() << "; msg: " << ec.message();
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
  return exists(p, ec);
}

// static
bool File::ExistsWildcard(const std::filesystem::path& wildcard) {
  WFindFile fnd;
  return fnd.open(wildcard, WFindFileTypeMask::WFINDFILE_ANY);
}

bool File::SetFilePermissions(const std::filesystem::path& path, int perm) {
  CHECK(!path.empty());
  return chmod(path.string().c_str(), perm) == 0;
}

// static
bool File::IsFileHandleValid(int handle) noexcept { return handle != invalid_handle; }

// static
std::string File::EnsureTrailingSlash(const std::filesystem::path& path) {
  if (path.empty()) {
    return {};
  }
  auto newpath{path.string()};
  if (newpath.back() == pathSeparatorChar) {
    return newpath;
  }
  newpath.push_back(pathSeparatorChar);
  return newpath;
}

// static
path File::current_directory() {
  std::error_code ec;
  return current_path(ec);
}

// static
bool File::set_current_directory(const std::filesystem::path& dir) {
  std::error_code ec;
  current_path(dir, ec);
  return ec.value() == 0;
}

// static
std::string File::FixPathSeparators(const std::string& path) {
  std::filesystem::path p{path};
  return p.make_preferred().string();
}

// static
bool File::is_absolute(const std::filesystem::path& p) {
#ifdef __OS2__
  if (!p.empty()) {
    const auto s = p.string();
    if (s.length() >= 3) {
      // Maybe X:\\ or X://
      const auto s1 = s.at(1);
      const auto s2 = s.at(2);
      if (s1 == ':' && (s2 == '/' || s2 == '\\')) {
	return true;
      }
    }
    const auto s0 = s.front();
    if (s0 == '/' || s0 == '\\') {
      return true;
    }
  }
#endif

  return p.is_absolute();
}

// static
std::filesystem::path File::absolute(const std::filesystem::path& p) {
#ifdef __OS2__
  if (is_absolute(p)) {
    return p;
  }
#endif
  return std::filesystem::absolute(p);
}

// static
path File::absolute(const std::filesystem::path& base, const std::filesystem::path& relative) {
  if (is_absolute(relative)) {
    return relative;
  }
  return FilePath(base, relative);
}

// static
bool File::mkdir(const std::filesystem::path& p) {
  std::error_code ec;
  if (exists(p, ec)) {
    return true;
  }

  if (create_directory(p, ec)) {
    return true;
  }
  return ec.value() == 0;
}

// static
bool File::mkdirs(const std::filesystem::path& p) {
  std::error_code ec;
  if (exists(p, ec)) {
    return true;
  }
  if (create_directories(p, ec)) {
    return true;
  }
  return ec.value() == 0;
}

std::ostream& operator<<(std::ostream& os, const File& file) {
  os << file.full_pathname();
  return os;
}

// ReSharper disable once CppMemberFunctionMayBeConst
bool File::set_last_write_time(time_t last_write_time) noexcept {
  return File::set_last_write_time(full_path_name_, last_write_time);
}

// static 
bool File::set_last_write_time(const std::filesystem::path& path,
  time_t last_write_time) noexcept {
  // Stick with calling utime vs. filesystem:last_write_time until C++20 since
  // C++20 will allow portable output

  // ReSharper disable once CppInitializedValueIsAlwaysRewritten
  struct utimbuf ut {};
  ut.actime = ut.modtime = last_write_time;
  return utime(path.string().c_str(), &ut) != -1;
}

std::unique_ptr<FileLock> File::lock(FileLockType lock_type) {
#ifdef _WIN32
  auto* h = reinterpret_cast<HANDLE>(_get_osfhandle(handle_));
  OVERLAPPED overlapped{};
  DWORD dwLockType = 0;
  if (lock_type == FileLockType::write_lock) {
    dwLockType = LOCKFILE_EXCLUSIVE_LOCK;
  }
  if (!::LockFileEx(h, dwLockType, 0, MAXDWORD, MAXDWORD, &overlapped)) {
    LOG(ERROR) << "Error Locking file: " << full_path_name_;
  }
#else

  // TODO: unlock here

#endif // _WIN32
  return std::make_unique<FileLock>(handle_, full_path_name_.string(), lock_type);
}

std::string File::full_pathname() const noexcept {
  try {
    return full_path_name_.string();
  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception in File::full_pathname: " << e.what();
    DLOG(FATAL) << "Exception in File::full_pathname: " << e.what();
  }
  return {};
}

bool File::Copy(const std::filesystem::path& from, const std::filesystem::path& to) {
  std::error_code ec;
  copy_file(from, to, copy_options::overwrite_existing, ec);
  return ec.value() == 0;
}

bool File::Move(const std::filesystem::path& from, const std::filesystem::path& to) {
  return Rename(from, to);
}

// static
std::filesystem::path File::canonical(const std::filesystem::path& path) {
#if defined(__OS2__) 
  //TODO(rushfan): Hack until std::filesystem is fixed on OS/2
  {
    char buf[4000];
    char* p = _realrealpath(path.c_str(), buf, sizeof(buf));
    if (p != nullptr) {
      return std::filesystem::path(FixPathSeparators(p));
    }
  }
#endif 
  std::error_code ec;
  if (auto res = std::filesystem::canonical(path, ec).string(); ec.value() == 0) {
    return res;
  }
  // We can't make this canonical, so try to make it absolute instead.
  return absolute(path);
}

long File::freespace_for_path(const std::filesystem::path& p) {
  std::error_code ec;
  const auto devi = space(p, ec);
  if (ec.value() == EOVERFLOW) {
    // Hack for really large partitions that seems to return EOVERFLOW on some linux.
    // https://bugzilla.redhat.com/show_bug.cgi?id=1758001 is likely the bug.
    return 1024 * 1024;
  }
  if (ec.value() != 0) {
    return 0;
  }
  return static_cast<long>(devi.available / 1024);
}

} // namespace wwiv
