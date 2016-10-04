/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016,WWIV Software Services             */
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
#include "bbs/wwiv_windows.h"

#include "Shlwapi.h"
#endif  // _WIN32

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <share.h>
#include "sys/utime.h"
#endif  // _WIN32
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#include "core/log.h"
#include "core/wfndfile.h"

#ifndef _WIN32
#include <sys/file.h>
#include <unistd.h>
#include <utime.h>
#endif  // _WIN32

#include "core/os.h"
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
#endif  // ftruncate
#define flock(h, m)
#define LOCK_SH		1
#define LOCK_EX		2
#define LOCK_NB		4
#define LOCK_UN		8
#define F_OK 0

#else 
#define _sopen(n, f, s, p) open(n, f, 0644)
#endif  // _WIN32


using std::string;
using std::chrono::milliseconds;
using namespace wwiv::os;

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
const int File::modeExclusive      = O_EXCL;

const int File::modeUnknown        = -1;
const int File::shareUnknown       = -1;

const int File::seekBegin          = SEEK_SET;
const int File::seekCurrent        = SEEK_CUR;
const int File::seekEnd            = SEEK_END;

const int File::invalid_handle     = -1;

static constexpr int WAIT_TIME_MILLIS = 10;
static constexpr int TRIES = 100;

/////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

File::File() : handle_(File::invalid_handle) {}

File::File(const string& full_file_name) : File() {
  this->SetName(full_file_name);
}

File::File(const string& dir, const string& filename) : File() {
  this->SetName(dir, filename);
}

File::~File() {
  VLOG(3) << "~File " << full_path_name_ << ", handle=" << handle_;
  if (this->IsOpen()) {
    this->Close();
  }
}

bool File::Open(int nFileMode, int nShareMode) {
  DCHECK_EQ(this->IsOpen(), false) << "File " << full_path_name_ << " is already open.";

  // Set default share mode
  if (nShareMode == File::shareUnknown) {
    nShareMode = shareDenyWrite;
    if ((nFileMode & File::modeReadWrite) ||
        (nFileMode & File::modeWriteOnly)) {
      nShareMode = File::shareDenyReadWrite;
    }
  }

  CHECK_NE(nShareMode, File::shareUnknown);
  CHECK_NE(nFileMode, File::modeUnknown);

  VLOG(3) << "SH_OPEN " << full_path_name_ << ", access=" << nFileMode;

  handle_ = _sopen(full_path_name_.c_str(), nFileMode, nShareMode, _S_IREAD | _S_IWRITE);
  if (handle_ < 0) {
    int count = 1;
    if (access(full_path_name_.c_str(), 0) != -1) {
      sleep_for(milliseconds(WAIT_TIME_MILLIS));
      handle_ = _sopen(full_path_name_.c_str(), nFileMode, nShareMode, _S_IREAD | _S_IWRITE);
      while ((handle_ < 0 && errno == EACCES) && count < TRIES) {
        sleep_for(milliseconds((count % 2) ? WAIT_TIME_MILLIS : 0));
        VLOG(3) << "Waiting to access " << full_path_name_ << "  " << TRIES - count;
        count++;
        handle_ = _sopen(full_path_name_.c_str(), nFileMode, nShareMode, _S_IREAD | _S_IWRITE);
      }

      if (handle_ < 0) {
        VLOG(3) << "The file " << full_path_name_ << " is busy.  Try again later.";
      }
    }
  }

  VLOG(3) << "SH_OPEN " << full_path_name_ << ", access=" << nFileMode << ", handle=" << handle_;

  if (File::IsFileHandleValid(handle_)) {
    int mode = (nShareMode == shareDenyReadWrite || nShareMode == shareDenyWrite) ? LOCK_EX : LOCK_SH;
    flock(handle_, mode);
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

bool File::SetName(const string& fileName) {
  full_path_name_ = fileName;
  return true;
}

bool File::SetName(const string& dirName, const string& fileName) {
  std::stringstream full_path_name;
  full_path_name << dirName;
  if (!dirName.empty() && dirName[dirName.length() - 1] == pathSeparatorChar) {
    full_path_name << fileName;
  } else {
    full_path_name << pathSeparatorChar << fileName;
  }
  return SetName(full_path_name.str());
}

int File::Read(void* pBuffer, size_t nCount) {
  int ret = read(handle_, pBuffer, nCount);
  if (ret == -1) {
    LOG(ERROR) << "[DEBUG: Read errno: " << errno
      << " filename: " << full_path_name_;
    LOG(ERROR) << " -- Please screen capture this and attach to a bug here: " << std::endl;
    LOG(ERROR) << "https://github.com/wwivbbs/wwiv/issues" << std::endl;
  }
  // TODO: Make this an CHECK once we get rid of these issues
  return ret;
}

int File::Write(const void* pBuffer, size_t nCount) {
  int nRet = write(handle_, pBuffer, nCount);
  if (nRet == -1) {
    LOG(ERROR) << "[DEBUG: Write errno: " << errno
      << " filename: " << full_path_name_;
    LOG(ERROR) << " -- Please screen capture this and attach to a bug here: " << std::endl;
    LOG(ERROR) << "https://github.com/wwivbbs/wwiv/issues" << std::endl;
  }
  // TODO: Make this an WWIV_ASSERT once we get rid of these issues
  return nRet;
}

long File::Seek(long lOffset, int nFrom) {
  CHECK(nFrom == File::seekBegin || nFrom == File::seekCurrent || nFrom == File::seekEnd);
  CHECK(File::IsFileHandleValid(handle_));

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
  if (original_pathname.empty()) {
    // An empty filename can not exist.
    // The question is should we assert here?
    return false;
  }

  string fn(original_pathname);
  if (fn.back() == pathSeparatorChar) {
    // If the pathname ends in / or \, then remove the last character.
    fn.pop_back();
  }
  int ret = access(fn.c_str(), F_OK);
  return ret == 0;
}

bool File::Exists(const string& directoryName, const string& fileName) {
  std::stringstream full_path_name;
  if (!directoryName.empty() && directoryName[directoryName.length() - 1] == pathSeparatorChar) {
    full_path_name << directoryName << fileName;
  } else {
    full_path_name << directoryName << pathSeparatorChar << fileName;
  }
  return Exists(full_path_name.str());
}

bool File::ExistsWildcard(const string& wildCard) {
  WFindFile fnd;
  return fnd.open(wildCard.c_str(), 0);
}

bool File::SetFilePermissions(const string& fileName, int nPermissions) {
  CHECK(!fileName.empty());
  return chmod(fileName.c_str(), nPermissions) == 0;
}

bool File::IsFileHandleValid(int hFile) {
  return hFile != File::invalid_handle;
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
	  string::size_type pos = path.find_last_of(File::pathSeparatorChar);
	  if (pos == string::npos) {
		  return false;
	  }
	  string s = path.substr(0, pos);
    if (!mkdirs(s)) {
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

bool File::set_last_write_time(time_t last_write_time) {
  struct utimbuf ut;
  ut.actime = ut.modtime = last_write_time;
  return utime(full_path_name_.c_str(), &ut) != -1;
}
