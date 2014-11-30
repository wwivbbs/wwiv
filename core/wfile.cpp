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

const int WFile::modeDefault = (O_RDWR | O_BINARY);
const int WFile::modeAppend         = O_APPEND;
const int WFile::modeBinary         = O_BINARY;
const int WFile::modeCreateFile     = O_CREAT;
const int WFile::modeReadOnly       = O_RDONLY;
const int WFile::modeReadWrite      = O_RDWR;
const int WFile::modeText           = O_TEXT;
const int WFile::modeWriteOnly      = O_WRONLY;
const int WFile::modeTruncate       = O_TRUNC;

const int WFile::modeUnknown        = -1;
const int WFile::shareUnknown       = -1;

const int WFile::seekBegin          = SEEK_SET;
const int WFile::seekCurrent        = SEEK_CUR;
const int WFile::seekEnd            = SEEK_END;

const int WFile::invalid_handle     = -1;

WLogger*  WFile::logger_;
int WFile::debug_level_;

static const int WAIT_TIME_SECONDS = 10;
static const int TRIES = 100;

/////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

WFile::WFile() {
  init();
}

WFile::WFile(const string& fileName) {
  init();
  this->SetName(fileName);
}

WFile::WFile(const string& dirName, const string& fileName) {
  init();
  this->SetName(dirName, fileName);
}

void WFile::init() {
  open_ = false;
  handle_ = WFile::invalid_handle;
}

WFile::~WFile() {
  if (this->IsOpen()) {
    this->Close();
  }
}

bool WFile::Open(int nFileMode, int nShareMode) {
  WWIV_ASSERT(this->IsOpen() == false);

  // Set default share mode
  if (nShareMode == WFile::shareUnknown) {
    nShareMode = shareDenyWrite;
    if ((nFileMode & WFile::modeReadWrite) ||
        (nFileMode & WFile::modeWriteOnly)) {
      nShareMode = WFile::shareDenyReadWrite;
    }
  }

  WWIV_ASSERT(nShareMode   != WFile::shareUnknown);
  WWIV_ASSERT(nFileMode    != WFile::modeUnknown);

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

  open_ = WFile::IsFileHandleValid(handle_);
  if (open_) {
    flock(handle_, (nShareMode & shareDenyWrite) ? LOCK_EX : LOCK_SH);
  }

  if (handle_ == WFile::invalid_handle) {
    this->error_text_ = strerror(errno);
  }

  return open_;
}

void WFile::Close() {
  if (WFile::IsFileHandleValid(handle_)) {
    flock(handle_, LOCK_UN);
    close(handle_);
    handle_ = WFile::invalid_handle;
    open_ = false;
  }
}

/////////////////////////////////////////////////////////////////////////////
// Member functions

bool WFile::SetName(const string& fileName) {
  full_path_name_ = fileName;
  return true;
}

bool WFile::SetName(const string& dirName, const string& fileName) {
  std::stringstream fullPathName;
  fullPathName << dirName;
  if (!dirName.empty() && dirName[dirName.length() - 1] == pathSeparatorChar) {
    fullPathName << fileName;
  } else {
    fullPathName << pathSeparatorChar << fileName;
  }
  return SetName(fullPathName.str());
}

int WFile::Read(void* pBuffer, size_t nCount) {
  int ret = read(handle_, pBuffer, nCount);
  if (ret == -1) {
    std::cout << "[DEBUG: Read errno: " << errno << " -- Please screen capture this and email to Rushfan]\r\n";
  }
  // TODO: Make this an WWIV_ASSERT once we get rid of these issues
  return ret;
}

int WFile::Write(const void* pBuffer, size_t nCount) {
  int nRet = write(handle_, pBuffer, nCount);
  if (nRet == -1) {
    std::cout << "[DEBUG: Write errno: " << errno << " -- Please screen capture this and email to Rushfan]\r\n";
  }
  // TODO: Make this an WWIV_ASSERT once we get rid of these issues
  return nRet;
}

long WFile::Seek(long lOffset, int nFrom) {
  WWIV_ASSERT(nFrom == WFile::seekBegin || nFrom == WFile::seekCurrent || nFrom == WFile::seekEnd);
  WWIV_ASSERT(WFile::IsFileHandleValid(handle_));

  return lseek(handle_, lOffset, nFrom);
}

bool WFile::Exists() const {
  return WFile::Exists(full_path_name_);
}

bool WFile::Delete() {
  if (this->IsOpen()) {
    this->Close();
  }
  return unlink(full_path_name_.c_str()) == 0;
}

void WFile::SetLength(long lNewLength) {
  WWIV_ASSERT(WFile::IsFileHandleValid(handle_));
  ftruncate(handle_, lNewLength);
}

bool WFile::IsFile() {
  return !this->IsDirectory();
}

bool WFile::SetFilePermissions(int nPermissions) {
  return chmod(full_path_name_.c_str(), nPermissions) == 0;
}

long WFile::GetLength() {
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

time_t WFile::last_write_time() {
  bool bOpenedHere = false;
  if (!this->IsOpen()) {
    bOpenedHere = true;
    Open();
  }
  WWIV_ASSERT(WFile::IsFileHandleValid(handle_));

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

bool WFile::Rename(const string origFileName, const string newFileName) {
  return rename(origFileName.c_str(), newFileName.c_str()) == 0;
}

bool WFile::Remove(const string fileName) {
  return (unlink(fileName.c_str()) ? false : true);
}

bool WFile::Remove(const string directoryName, const string fileName) {
  string strFullFileName = directoryName;
  strFullFileName += fileName;
  return WFile::Remove(strFullFileName);
}

bool WFile::Exists(const string original_pathname) {
  struct _stat buf;
  string fn(original_pathname);
  if (fn.back() == pathSeparatorChar) {
    // If the pathname ends in / or \, then remove the last character.
    fn.pop_back();
  }
  int ret = _stat(fn.c_str(), &buf);
  return ret == 0;
}

bool WFile::Exists(const string directoryName, const string fileName) {
  std::stringstream fullPathName;
  if (!directoryName.empty() && directoryName[directoryName.length() - 1] == pathSeparatorChar) {
    fullPathName << directoryName << fileName;
  } else {
    fullPathName << directoryName << pathSeparatorChar << fileName;
  }
  return Exists(fullPathName.str());
}

bool WFile::ExistsWildcard(const string wildCard) {
  WFindFile fnd;
  return (fnd.open(wildCard.c_str(), 0));
}

bool WFile::SetFilePermissions(const string fileName, int nPermissions) {
  WWIV_ASSERT(!fileName.empty());
  return (chmod(fileName.c_str(), nPermissions) == 0) ? true : false;
}

bool WFile::IsFileHandleValid(int hFile) {
  return (hFile != WFile::invalid_handle) ? true : false;
}

//static
void WFile::EnsureTrailingSlash(string* path) {
  char last_char = path->back();
  if (last_char != WFile::pathSeparatorChar) {
    path->push_back(WFile::pathSeparatorChar);
  }
}

// static 
void WFile::CurrentDirectory(string* current_dir) {
  char s[MAX_PATH];
  getcwd(s, MAX_PATH);
  current_dir->assign(s);
}

// static
void WFile::MakeAbsolutePath(const string& base, string* relative) {
  if (!WFile::IsAbsolutePath(*relative)) {
    WFile dir(base, *relative);
    relative->assign(dir.full_pathname());
  }
}

// static
bool WFile::IsAbsolutePath(const string& path) {
  if (path.empty()) {
    return false;
  }
#ifdef _WIN32
  return ::PathIsRelative(path.c_str()) ? false : true;
#else
  return path.front() == WFile::pathSeparatorChar;
#endif  // _WIN32
}


