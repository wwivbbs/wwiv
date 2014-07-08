/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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
#undef CopyFile
#undef GetFileTime
#undef GetFullPathName
#undef MoveFile
#endif  // _WIN32

#include "core/wfndfile.h"
#include "core/wwivassert.h"

#if defined( __APPLE__ )
#if !defined( O_BINARY )
#define O_BINARY 0
#endif
#endif

/////////////////////////////////////////////////////////////////////////////
// Constants

const int WFile::modeDefault        = (_O_RDWR | _O_BINARY);
const int WFile::modeUnknown        = -1;
const int WFile::modeAppend         = _O_APPEND;
const int WFile::modeBinary         = _O_BINARY;
const int WFile::modeCreateFile     = _O_CREAT;
const int WFile::modeReadOnly       = _O_RDONLY;
const int WFile::modeReadWrite      = _O_RDWR;
const int WFile::modeText           = _O_TEXT;
const int WFile::modeWriteOnly      = _O_WRONLY;
const int WFile::modeTruncate       = _O_TRUNC;

const int WFile::shareUnknown       = -1;
const int WFile::shareDenyReadWrite = SH_DENYRW;
const int WFile::shareDenyWrite     = SH_DENYWR;
const int WFile::shareDenyRead      = SH_DENYRD;
const int WFile::shareDenyNone      = SH_DENYNO;

const int WFile::permUnknown        = -1;
const int WFile::permWrite          = _S_IWRITE;
const int WFile::permRead           = _S_IREAD;
const int WFile::permReadWrite      = (_S_IREAD | _S_IWRITE);

const int WFile::seekBegin          = SEEK_SET;
const int WFile::seekCurrent        = SEEK_CUR;
const int WFile::seekEnd            = SEEK_END;

const int WFile::invalid_handle     = -1;

const char WFile::pathSeparatorChar = '\\';
const char WFile::separatorChar     = ';';

WLogger*  WFile::m_pLogger;
int       WFile::m_nDebugLevel;

// WAIT_TIME is 10 seconds
#define WAIT_TIME 10
#define TRIES 100

/////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

WFile::WFile() {
  init();
}

WFile::WFile(const std::string fileName) {
  init();
  this->SetName(fileName);
}

WFile::WFile(const std::string dirName, const std::string fileName) {
  init();
  this->SetName(dirName, fileName);
}

void WFile::init() {
  m_bOpen                 = false;
  m_hFile                 = WFile::invalid_handle;
  memset(m_szFileName, 0, MAX_PATH + 1);
}

WFile::~WFile() {
  if (this->IsOpen()) {
    this->Close();
  }
}

/////////////////////////////////////////////////////////////////////////////
// Member functions

bool WFile::SetName(const std::string fileName) {
  strncpy(m_szFileName, fileName.c_str(), MAX_PATH);
  return true;
}

bool WFile::SetName(const std::string dirName, const std::string fileName) {
  std::stringstream fullPathName;
  fullPathName << dirName;
  if (!dirName.empty() && dirName[ dirName.length() - 1 ] == pathSeparatorChar) {
    fullPathName << fileName;
  } else {
    fullPathName << pathSeparatorChar << fileName;
  }
  return SetName(fullPathName.str());
}

bool WFile::Open(int nFileMode, int nShareMode, int nPermissions) {
  WWIV_ASSERT(this->IsOpen() == false);

  // Set default permissions
  if (nPermissions == WFile::permUnknown) {
    // modeReadOnly is 0 on Win32, so the test with & always fails.
    // This means that readonly is always allowed on Win32
    // hence defaulting to permRead always
    nPermissions = WFile::permRead;
    if ((nFileMode & WFile::modeReadWrite) ||
        (nFileMode & WFile::modeWriteOnly)) {
      nPermissions |= WFile::permWrite;
    }
  }

  // Set default share mode
  if (nShareMode == WFile::shareUnknown) {
    nShareMode = shareDenyWrite;
    if ((nFileMode & WFile::modeReadWrite) ||
        (nFileMode & WFile::modeWriteOnly) ||
        (nPermissions & WFile::permWrite)) {
      nShareMode = WFile::shareDenyReadWrite;
    }
  }

  WWIV_ASSERT(nShareMode   != WFile::shareUnknown);
  WWIV_ASSERT(nFileMode    != WFile::modeUnknown);
  WWIV_ASSERT(nPermissions != WFile::permUnknown);

  if (m_nDebugLevel > 2) {
    m_pLogger->LogMessage("\rSH_OPEN %s, access=%u\r\n", m_szFileName, nFileMode);
  }

  m_hFile = _sopen(m_szFileName, nFileMode, nShareMode, nPermissions);
  if (m_hFile < 0) {
    int count = 1;
    if (access(m_szFileName, 0) != -1) {
      Sleep(WAIT_TIME);
      m_hFile = _sopen(m_szFileName, nFileMode, nShareMode, nPermissions);
      while ((m_hFile < 0 && errno == EACCES) && count < TRIES) {
        Sleep((count % 2) ? WAIT_TIME : 0);
        if (m_nDebugLevel > 0) {
          m_pLogger->LogMessage("\rWaiting to access %s %d.  \r", m_szFileName, TRIES - count);
        }
        count++;
        m_hFile = _sopen(m_szFileName, nFileMode, nShareMode, nPermissions);
      }

      if ((m_hFile < 0) && (m_nDebugLevel > 0)) {
        m_pLogger->LogMessage("\rThe file %s is busy.  Try again later.\r\n", m_szFileName);
      }
    }
  }

  if (m_nDebugLevel > 1) {
    m_pLogger->LogMessage("\rSH_OPEN %s, access=%u, handle=%d.\r\n", m_szFileName, nFileMode, m_hFile);
  }

  m_bOpen = WFile::IsFileHandleValid(m_hFile);
  if (m_hFile == -1) {
    std::cout << "error opening file: " << m_szFileName << "; error: " << strerror(errno) << std::endl;
    this->m_errorText = _strerror("_sopen");
  }

  return m_bOpen;
}

void WFile::Close() {
  if (WFile::IsFileHandleValid(m_hFile)) {
    close(m_hFile);
    m_hFile = WFile::invalid_handle;
    m_bOpen = false;
  }
}

int WFile::Read(void * pBuffer, size_t nCount) {
  int ret = read(m_hFile, pBuffer, nCount);
  if (ret == -1) {
    std::cout << "[DEBUG: errno: " << errno << " -- Please screen capture this and email to Rushfan]\r\n";
  }
  // TODO: Make this an WWIV_ASSERT once we get rid of these issues
  return ret;
}

int WFile::Write(const void * pBuffer, size_t nCount) {
  int nRet = _write(m_hFile, pBuffer, nCount);
  if (nRet == -1) {
    std::cout << "[DEBUG: errno: " << errno << " -- Please screen capture this and email to Rushfan]\r\n";
  }
  // TODO: Make this an WWIV_ASSERT once we get rid of these issues
  return nRet;
}

long WFile::GetLength() {
  // _stat/_fstat is the 32 bit version on WIN32
  struct _stat fileinfo;

  if (IsOpen()) {
    // File is open, use fstat
    if (_fstat(m_hFile, &fileinfo) != 0) {
      return -1;
    }
  } else {
    // stat works on filenames, not filehandles.
    if (_stat(m_szFileName, &fileinfo) != 0) {
      return -1;
    }
  }
  return fileinfo.st_size;
}

long WFile::Seek(long lOffset, int nFrom) {
  WWIV_ASSERT(nFrom == WFile::seekBegin || nFrom == WFile::seekCurrent || nFrom == WFile::seekEnd);
  WWIV_ASSERT(WFile::IsFileHandleValid(m_hFile));

  return lseek(m_hFile, lOffset, nFrom);
}

void WFile::SetLength(long lNewLength) {
  WWIV_ASSERT(WFile::IsFileHandleValid(m_hFile));
  _chsize(m_hFile, lNewLength);
}

bool WFile::Exists() const {
  return WFile::Exists(m_szFileName);
}

bool WFile::Delete() {
  if (this->IsOpen()) {
    this->Close();
  }
  return (unlink(m_szFileName) == 0) ? true : false;
}

bool WFile::IsDirectory() {
  DWORD dwAttributes = GetFileAttributes(m_szFileName);
  return (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false;
}

bool WFile::IsFile() {
  return !this->IsDirectory();
}

bool WFile::SetFilePermissions(int nPermissions) {
  return (chmod(m_szFileName, nPermissions) == 0) ? true : false;
}

time_t WFile::GetFileTime() {
  bool bOpenedHere = false;
  if (!this->IsOpen()) {
    bOpenedHere = true;
    Open();
  }
  WWIV_ASSERT(WFile::IsFileHandleValid(m_hFile));

  // N.B. On Windows with _USE_32BIT_TIME_T defined _fstat == _fstat32.
  struct _stat buf;
  time_t nFileTime = (_fstat(m_hFile, &buf) == -1) ? 0 : buf.st_mtime;

  if (bOpenedHere) {
    Close();
  }
  return nFileTime;
}

/////////////////////////////////////////////////////////////////////////////
// Static functions

bool WFile::Remove(const std::string fileName) {
  return (unlink(fileName.c_str()) ? false : true);
}

bool WFile::Remove(const std::string directoryName, const std::string fileName) {
  std::string strFullFileName = directoryName;
  strFullFileName += fileName;
  return WFile::Remove(strFullFileName);
}

bool WFile::Rename(const std::string origFileName, const std::string newFileName) {
  return MoveFile(origFileName.c_str(), newFileName.c_str()) ? true : false;
}

bool WFile::Exists(const std::string fileName) {
  return (access(fileName.c_str(), 0) != -1) ? true : false;
}

bool WFile::Exists(const std::string directoryName, const std::string fileName) {
  std::stringstream fullPathName;
  if (!directoryName.empty() && directoryName[directoryName.length() - 1] == pathSeparatorChar) {
    fullPathName << directoryName << fileName;
  } else {
    fullPathName << directoryName << pathSeparatorChar << fileName;
  }
  return Exists(fullPathName.str());
}

bool WFile::ExistsWildcard(const std::string wildCard) {
  WFindFile fnd;
  return (fnd.open(wildCard.c_str(), 0));
}

bool WFile::SetFilePermissions(const std::string fileName, int nPermissions) {
  WWIV_ASSERT(!fileName.empty());
  return (chmod(fileName.c_str(), nPermissions) == 0) ? true : false;
}

bool WFile::IsFileHandleValid(int hFile) {
  return (hFile != WFile::invalid_handle) ? true : false;
}

bool WFile::CopyFile(const std::string sourceFileName, const std::string destFileName) {
  return ::CopyFileA(sourceFileName.c_str(), destFileName.c_str(), FALSE) ? true : false;
}

bool WFile::MoveFile(const std::string sourceFileName, const std::string destFileName) {
  return ::MoveFileA(sourceFileName.c_str(), destFileName.c_str()) ? true : false;
}
