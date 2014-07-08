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

const int WFile::shareDenyReadWrite = SH_DENYRW;
const int WFile::shareDenyWrite     = SH_DENYWR;
const int WFile::shareDenyRead      = SH_DENYRD;
const int WFile::shareDenyNone      = SH_DENYNO;

const int WFile::permWrite          = _S_IWRITE;
const int WFile::permRead           = _S_IREAD;
const int WFile::permReadWrite      = (_S_IREAD | _S_IWRITE);

const char WFile::pathSeparatorChar = '\\';
const char WFile::separatorChar     = ';';

// WAIT_TIME is 10 seconds
#define WAIT_TIME 10
#define TRIES 100

/////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

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
    this->m_errorText = strerror(errno);
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

bool WFile::IsDirectory() {
  DWORD dwAttributes = GetFileAttributes(m_szFileName);
  return (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false;
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

bool WFile::Rename(const std::string origFileName, const std::string newFileName) {
  return MoveFile(origFileName.c_str(), newFileName.c_str()) ? true : false;
}

bool WFile::Exists(const std::string fileName) {
  return (access(fileName.c_str(), 0) != -1) ? true : false;
}

bool WFile::CopyFile(const std::string sourceFileName, const std::string destFileName) {
  return ::CopyFileA(sourceFileName.c_str(), destFileName.c_str(), FALSE) ? true : false;
}

bool WFile::MoveFile(const std::string sourceFileName, const std::string destFileName) {
  return ::MoveFileA(sourceFileName.c_str(), destFileName.c_str()) ? true : false;
}
