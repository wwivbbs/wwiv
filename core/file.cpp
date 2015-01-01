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

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#ifdef _WIN32
#include <direct.h>
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
#include "Shlwapi.h"
#endif  // _WIN32

#include "core/wfndfile.h"
#include "core/wwivassert.h"

#if !defined( O_BINARY )
#define O_BINARY 0
#endif
#if !defined( O_TEXT )
#define O_TEXT 0
#endif

#ifdef _WIN32

// Needed for PathIsRelative
#pragma comment(lib, "Shlwapi.lib")

#if !defined(ftruncate)
#define ftruncate chsize
#endif

#define flock(h, m)
#else 

#define _stat stat
#define _fstat fstat
#define Sleep(x) usleep((x)*1000)
#define _sopen(n, f, s, p) open(n, f, 0644)

#endif  // _WIN32

using std::string;

/////////////////////////////////////////////////////////////////////////////
// Constants

const int File::modeDefault = (O_RDWR | O_BINARY);
const int File::modeAppend         = O_APPEND;
const int File::modeBinary         = O_BINARY;
const int File::modeCreateFile     = O_CREAT;
const int File::modeReadOnly       = O_RDONLY;
const int File::modeReadWrite      = O_RDWR;
const int File::modeText           = O_TEXT;
const int File::modeWriteOnly      = O_WRONLY;
const int File::modeTruncate       = O_TRUNC;

const int File::modeUnknown        = -1;
const int File::shareUnknown       = -1;

const int File::seekBegin          = SEEK_SET;
const int File::seekCurrent        = SEEK_CUR;
const int File::seekEnd            = SEEK_END;

const int File::invalid_handle     = -1;

WLogger*  File::logger_;
int File::debug_level_;

static const int WAIT_TIME_SECONDS = 10;
static const int TRIES = 100;

/////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

File::File() : open_(false), handle_(File::invalid_handle) {}

File::File(const string& full_file_name) : File() {
  this->SetName(full_file_name);
}

File::File(const string& dir, const string& filename) : File() {
  this->SetName(dir, filename);
}

File::~File() {
  if (this->IsOpen()) {
    this->Close();
  }
}

bool File::Open(int nFileMode, int nShareMode) {
  WWIV_ASSERT(this->IsOpen() == false);

  // Set default share mode
  if (nShareMode == File::shareUnknown) {
    nShareMode = shareDenyWrite;
    if ((nFileMode & File::modeReadWrite) ||
        (nFileMode & File::modeWriteOnly)) {
      nShareMode = File::shareDenyReadWrite;
    }
  }

  WWIV_ASSERT(nShareMode != File::shareUnknown);
  WWIV_ASSERT(nFileMode != File::modeUnknown);

  if (debug_level_ > 2) {
    logger_->LogMessage("\rSH_OPEN %s, access=%u\r\n", full_path_name_.c_str(), nFileMode);
  }

  handle_ = _sopen(full_path_name_.c_str(), nFileMode, nShareMode, _S_IREAD | _S_IWRITE);
  if (handle_ < 0) {
    int count = 1;
    if (access(full_path_name_.c_str(), 0) != -1) {
      Sleep(WAIT_TIME_SECONDS);
      handle_ = _sopen(full_path_name_.c_str(), nFileMode, nShareMode, _S_IREAD | _S_IWRITE);
      while ((handle_ < 0 && errno == EACCES) && count < TRIES) {
        Sleep((count % 2) ? WAIT_TIME_SECONDS : 0);
        if (debug_level_ > 0) {
          logger_->LogMessage("\rWaiting to access %s %d.  \r", full_path_name_.c_str(), TRIES - count);
        }
        count++;
        handle_ = _sopen(full_path_name_.c_str(), nFileMode, nShareMode, _S_IREAD | _S_IWRITE);
      }

      if ((handle_ < 0) && (debug_level_ > 0)) {
        logger_->LogMessage("\rThe file %s is busy.  Try again later.\r\n", full_path_name_.c_str());
      }
    }
  }

  if (debug_level_ > 1) {
    logger_->LogMessage("\rSH_OPEN %s, access=%u, handle=%d.\r\n", full_path_name_.c_str(), nFileMode, handle_);
  }

  open_ = File::IsFileHandleValid(handle_);
  if (open_) {
    flock(handle_, (nShareMode & shareDenyWrite) ? LOCK_EX : LOCK_SH);
  }

  if (handle_ == File::invalid_handle) {
    this->error_text_ = strerror(errno);
  }

  return open_;
}

void File::Close() {
  if (File::IsFileHandleValid(handle_)) {
    flock(handle_, LOCK_UN);
    close(handle_);
    handle_ = File::invalid_handle;
    open_ = false;
  }
}

/////////////////////////////////////////////////////////////////////////////
// Member functions

bool File::SetName(const string& fileName) {
  full_path_name_ = fileName;
  return true;
}

bool File::SetName(const string& dirName, const string& fileName) {
  std::stringstream fullPathName;
  fullPathName << dirName;
  if (!dirName.empty() && dirName[dirName.length() - 1] == pathSeparatorChar) {
    fullPathName << fileName;
  } else {
    fullPathName << pathSeparatorChar << fileName;
  }
  return SetName(fullPathName.str());
}

int File::Read(void* pBuffer, size_t nCount) {
  int ret = read(handle_, pBuffer, nCount);
  if (ret == -1) {
    std::cout << "[DEBUG: Read errno: " << errno << " -- Please screen capture this and email to Rushfan]\r\n";
  }
  // TODO: Make this an WWIV_ASSERT once we get rid of these issues
  return ret;
}

int File::Write(const void* pBuffer, size_t nCount) {
  int nRet = write(handle_, pBuffer, nCount);
  if (nRet == -1) {
    std::cout << "[DEBUG: Write errno: " << errno << " -- Please screen capture this and email to Rushfan]\r\n";
  }
  // TODO: Make this an WWIV_ASSERT once we get rid of these issues
  return nRet;
}

long File::Seek(long lOffset, int nFrom) {
  WWIV_ASSERT(nFrom == File::seekBegin || nFrom == File::seekCurrent || nFrom == File::seekEnd);
  WWIV_ASSERT(File::IsFileHandleValid(handle_));

  return lseek(handle_, lOffset, nFrom);
}

bool File::Exists() const {
  return File::Exists(full_path_name_);
}

bool File::Delete() {
  if (this->IsOpen()) {
    this->Close();
  }
  return unlink(full_path_name_.c_str()) == 0;
}

void File::SetLength(long lNewLength) {
  WWIV_ASSERT(File::IsFileHandleValid(handle_));
  ftruncate(handle_, lNewLength);
}

bool File::IsFile() const {
  return !this->IsDirectory();
}

bool File::SetFilePermissions(int nPermissions) {
  return chmod(full_path_name_.c_str(), nPermissions) == 0;
}

long File::GetLength() {
  // _stat/_fstat is the 32 bit version on WIN32
  struct _stat fileinfo;

  if (IsOpen()) {
    // File is open, use fstat
    if (_fstat(handle_, &fileinfo) != 0) {
      return -1;
    }
  } else {
    // stat works on filenames, not filehandles.
    if (_stat(full_path_name_.c_str(), &fileinfo) != 0) {
      return -1;
    }
  }
  return fileinfo.st_size;
}

time_t File::last_write_time() {
  bool bOpenedHere = false;
  if (!this->IsOpen()) {
    bOpenedHere = true;
    Open();
  }
  WWIV_ASSERT(File::IsFileHandleValid(handle_));

  // N.B. On Windows with _USE_32BIT_TIME_T defined _fstat == _fstat32.
  struct _stat buf;
  time_t nFileTime = (_stat(full_path_name_.c_str(), &buf) == -1) ? 0 : buf.st_mtime;

  if (bOpenedHere) {
    Close();
  }
  return nFileTime;
}
/////////////////////////////////////////////////////////////////////////////
// Static functions

bool File::Rename(const string& origFileName, const string& newFileName) {
  return rename(origFileName.c_str(), newFileName.c_str()) == 0;
}

bool File::Remove(const string& fileName) {
  return (unlink(fileName.c_str()) ? false : true);
}

bool File::Remove(const string& directoryName, const string& fileName) {
  string strFullFileName = directoryName;
  strFullFileName += fileName;
  return File::Remove(strFullFileName);
}

bool File::Exists(const string& original_pathname) {
  struct _stat buf;
  string fn(original_pathname);
  if (fn.back() == pathSeparatorChar) {
    // If the pathname ends in / or \, then remove the last character.
    fn.pop_back();
  }
  int ret = _stat(fn.c_str(), &buf);
  return ret == 0;
}

bool File::Exists(const string& directoryName, const string& fileName) {
  std::stringstream fullPathName;
  if (!directoryName.empty() && directoryName[directoryName.length() - 1] == pathSeparatorChar) {
    fullPathName << directoryName << fileName;
  } else {
    fullPathName << directoryName << pathSeparatorChar << fileName;
  }
  return Exists(fullPathName.str());
}

bool File::ExistsWildcard(const string& wildCard) {
  WFindFile fnd;
  return (fnd.open(wildCard.c_str(), 0));
}

bool File::SetFilePermissions(const string& fileName, int nPermissions) {
  WWIV_ASSERT(!fileName.empty());
  return (chmod(fileName.c_str(), nPermissions) == 0) ? true : false;
}

bool File::IsFileHandleValid(int hFile) {
  return (hFile != File::invalid_handle) ? true : false;
}

//static
void File::EnsureTrailingSlash(string* path) {
  char last_char = path->back();
  if (last_char != File::pathSeparatorChar) {
    path->push_back(File::pathSeparatorChar);
  }
}

// static 
string File::current_directory() {
  char s[MAX_PATH];
  getcwd(s, MAX_PATH);
  return string(s);
}

// static
bool File::set_current_directory(const string& dir) {
  return chdir(dir.c_str()) == 0;
}


// static
void File::MakeAbsolutePath(const string& base, string* relative) {
  if (!File::IsAbsolutePath(*relative)) {
    File dir(base, *relative);
    relative->assign(dir.full_pathname());
  }
}

// static
bool File::IsAbsolutePath(const string& path) {
  if (path.empty()) {
    return false;
  }
#ifdef _WIN32
  return ::PathIsRelative(path.c_str()) ? false : true;
#else
  return path.front() == File::pathSeparatorChar;
#endif  // _WIN32
}

#ifdef _WIN32
#define MKDIR(x) ::mkdir(x)
#else
#define MKDIR(x) ::mkdir(x, S_IRWXU | S_IRWXG)
#endif

// static
bool File::mkdir(const string& path) {
  int result = MKDIR(path.c_str());
  if (result != -1) {
    return true;
  }
  if (errno == EEXIST) {
    // still return true if the directory already existed.
    return true;
  }
  return false;
}

// static
// based loosely on http://stackoverflow.com/questions/675039/how-can-i-create-directory-tree-in-c-linux
bool File::mkdirs(const string& path) {
  int result = MKDIR(path.c_str());
  if (result != -1) {
    return true;
  }
  if (errno == ENOENT) {
    if (!mkdirs(path.substr(0, path.find_last_of(File::pathSeparatorChar)))) {
      return false;  // failed to create the parent, stop here.
    }
    return File::mkdir(path.c_str());
  } else if (errno == EEXIST) {
    return true;  // the path already existed.
  }
  return false;  // unknown error.
}

std::ostream& operator<<(std::ostream& os, const File& file) {
  os << file.full_pathname();
  return os;
}

