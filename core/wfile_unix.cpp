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

const int WFile::modeDefault        = O_RDWR;
const int WFile::modeAppend         = O_APPEND;
const int WFile::modeBinary         = 0;
const int WFile::modeCreateFile     = O_CREAT;
const int WFile::modeReadOnly       = O_RDONLY;
const int WFile::modeReadWrite      = O_RDWR;
const int WFile::modeText           = 0;
const int WFile::modeWriteOnly      = O_WRONLY;
const int WFile::modeTruncate       = O_TRUNC;

const int WFile::shareDenyReadWrite = S_IWRITE;
const int WFile::shareDenyWrite     = 0;
const int WFile::shareDenyRead      = S_IREAD;
const int WFile::shareDenyNone      = 0;

const int WFile::permWrite          = O_WRONLY;
const int WFile::permRead           = O_RDONLY;
const int WFile::permReadWrite      = O_RDWR;

const char WFile::pathSeparatorChar = '/';
const char WFile::separatorChar     = ':';

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
  }

  WWIV_ASSERT(nShareMode   != WFile::shareUnknown);
  WWIV_ASSERT(nFileMode    != WFile::modeUnknown);
  WWIV_ASSERT(nPermissions != WFile::permUnknown);

  if (m_nDebugLevel > 2) {
    m_pLogger->LogMessage("\rSH_OPEN %s, access=%u\r\n", m_szFileName, nFileMode);
  }

  m_hFile = open(m_szFileName, nFileMode, 0644);
  if (m_hFile < 0) {
    int count = 1;
    if (access(m_szFileName, 0) != -1) {
      usleep(WAIT_TIME * 1000);
      m_hFile = open(m_szFileName, nFileMode, 0644);
      while ((m_hFile < 0 && errno == EACCES) && count < TRIES) {
        usleep((count % 2) ? WAIT_TIME * 1000 : 0);
        if (m_nDebugLevel > 0) {
          m_pLogger->LogMessage("\rWaiting to access %s %d.  \r", m_szFileName, TRIES - count);
        }
        count++;
        m_hFile = open(m_szFileName, nFileMode, nShareMode);
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
  if (m_bOpen) {
    flock(m_hFile, (nShareMode & shareDenyWrite) ? LOCK_EX : LOCK_SH);
  }

  if (m_hFile == -1) {
    std::cout << "error opening file: " << m_szFileName << "; error: " << strerror(errno) << std::endl;
    this->m_errorText = strerror(errno);
  }

  return m_bOpen;
}

void WFile::Close() {
  if (WFile::IsFileHandleValid(m_hFile)) {
    flock(m_hFile, LOCK_UN);
    close(m_hFile);
    m_hFile = WFile::invalid_handle;
    m_bOpen = false;
  }
}

long WFile::GetLength() {
  // _stat/_fstat is the 32 bit version on WIN32
  struct stat fileinfo;

  if (IsOpen()) {
    // File is open, use fstat
    if (fstat(m_hFile, &fileinfo) != 0) {
      return -1;
    }
  } else {
    // stat works on filenames, not filehandles.
    if (stat(m_szFileName, &fileinfo) != 0) {
      return -1;
    }
  }
  return fileinfo.st_size;
}

void WFile::SetLength(long lNewLength) {
  WWIV_ASSERT(WFile::IsFileHandleValid(m_hFile));
  ftruncate(m_hFile, lNewLength);
}

bool WFile::IsDirectory() {
  struct stat statbuf;
  stat(m_szFileName, &statbuf);
  return (statbuf.st_mode & 0x0040000 ? true : false);
}

time_t WFile::GetFileTime() {
  bool bOpenedHere = false;
  if (!this->IsOpen()) {
    bOpenedHere = true;
    Open();
  }
  WWIV_ASSERT(WFile::IsFileHandleValid(m_hFile));

  struct stat buf;
  time_t nFileTime = (fstat(m_hFile, &buf) == -1) ? 0 : buf.st_mtime;

  if (bOpenedHere) {
    Close();
  }
  return nFileTime;
}

/////////////////////////////////////////////////////////////////////////////
// Static functions

bool WFile::Rename(const std::string origFileName, const std::string newFileName) {
  return rename(origFileName.c_str(), newFileName.c_str());
}

bool WFile::Exists(const std::string fileName) {
  struct stat buf;
  return (stat(fileName.c_str(), &buf) ? false : true);
}

bool WFile::CopyFile(const std::string sourceFileName, const std::string destFileName) {
  if (sourceFileName != destFileName && WFile::Exists(sourceFileName) && !WFile::Exists(destFileName)) {
    char *pBuffer = static_cast<char *>(malloc(16400));
    if (pBuffer == NULL) {
      return false;
    }
    int hSourceFile = open(sourceFileName.c_str(), O_RDONLY | O_BINARY);
    if (!hSourceFile) {
      free(pBuffer);
      return false;
    }

    int hDestFile = open(destFileName.c_str(), O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
    if (!hDestFile) {
      free(pBuffer);
      close(hSourceFile);
      return false;
    }

    int i = read(hSourceFile, (void *) pBuffer, 16384);

    while (i > 0) {
      write(hDestFile, (void *) pBuffer, i);
      i = read(hSourceFile, (void *) pBuffer, 16384);
    }

    hSourceFile = close(hSourceFile);
    hDestFile = close(hDestFile);
    free(pBuffer);
  }

  // I'm not sure about the logic here since you would think we should return true
  // in the last block, and false here.  This seems fishy
  return true;
}

bool WFile::MoveFile(const std::string sourceFileName, const std::string destFileName) {
  //TODO: Atani needs to see if Rushfan buggered up this implementation
  if (CopyFile(sourceFileName, destFileName)) {
    return Remove(sourceFileName);
  }
  return false;
}
