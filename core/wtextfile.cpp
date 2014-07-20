/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)2005-2014, WWIV Software Services             */
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
#include "core/wtextfile.h"

#include <iostream>
#include <cerrno>
#include <cstring>
#include <cstdarg>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>

#include "core/wfile.h"
#include "core/wwivport.h"

#ifdef _WIN32
#include <io.h>
#include <share.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#else  // _WIN32
#include <sys/file.h>
#include <unistd.h>
#endif  // _WIN32

const int WTextFile::WAIT_TIME = 10;
const int WTextFile::TRIES = 100;


WTextFile::WTextFile(const std::string fileName, const std::string fileMode) {
  Open(fileName, fileMode);
}

WTextFile::WTextFile(const std::string directoryName, const std::string fileName, const std::string fileMode) {
  std::string fullPathName(directoryName);
  fullPathName.append(fileName);
  Open(fullPathName, fileMode);
}

bool WTextFile::Open(const std::string fileName, const std::string fileMode) {
  m_fileName = fileName;
  m_fileMode = fileMode;
  m_hFile = WTextFile::OpenImpl(m_fileName.c_str(), m_fileMode.c_str());
  return m_hFile != nullptr;
}

#ifdef _WIN32

FILE* WTextFile::OpenImpl(const char* pszFileName, const char* pszFileMode) {
  int share = SH_DENYWR;
  int md = 0;
  if (strchr(pszFileMode, 'w') != NULL) {
    share = SH_DENYRD;
    md = O_RDWR | O_CREAT | O_TRUNC;
  } else if (strchr(pszFileMode, 'a') != NULL) {
    share = SH_DENYRD;
    md = O_RDWR | O_CREAT;
  } else {
    md = O_RDONLY;
  }
  if (strchr(pszFileMode, 'b') != NULL) {
    md |= O_BINARY;
  }
  if (strchr(pszFileMode, '+') != NULL) {
    md &= ~O_RDONLY;
    md |= O_RDWR;
    share = SH_DENYRD;
  }

  int fd = _sopen(pszFileName, md, share, S_IREAD | S_IWRITE);
  if (fd < 0) {
    int count = 1;
    if (WFile::Exists(pszFileName)) {
      ::Sleep(WAIT_TIME);
      fd = _sopen(pszFileName, md, share, S_IREAD | S_IWRITE);
      while ((fd < 0 && errno == EACCES) && count < TRIES) {
        ::Sleep(WAIT_TIME);
        count++;
        fd = _sopen(pszFileName, md, share, S_IREAD | S_IWRITE);
      }
    }
  }

  if (fd > 0) {
    if (strchr(pszFileMode, 'a')) {
      _lseek(fd, 0L, SEEK_END);
    }

    FILE *hFile = _fdopen(fd, pszFileMode);
    if (!hFile) {
      _close(fd);
    }
    return hFile;
  }
  return nullptr;
}

#else  // _WIN32
FILE* WTextFile::OpenImpl(const char* pszFileName, const char* pszFileMode) {
  FILE *f = fopen(pszFileName, pszFileMode);
  if (f != NULL) {
    flock(fileno(f), (strpbrk(pszFileMode, "wa+")) ? LOCK_EX : LOCK_SH);
  }
  return f;
}

#endif  // _WIN32

bool WTextFile::Close() {
  if (m_hFile != nullptr) {
    fclose(m_hFile);
    m_hFile = nullptr;
    return true;
  }
  return false;
}

int WTextFile::WriteFormatted(const char *pszFormatText, ...) {
  va_list ap;
  char szBuffer[ 4096 ];

  va_start(ap, pszFormatText);
  WWIV_VSNPRINTF(szBuffer, sizeof(szBuffer), pszFormatText, ap);
  va_end(ap);
  return Write(szBuffer);
}

bool WTextFile::ReadLine(std::string *buffer) {
  char szBuffer[4096];
  char *pszBuffer = fgets(szBuffer, sizeof(szBuffer), m_hFile);
  *buffer = szBuffer;
  return (pszBuffer != nullptr);
}

WTextFile::~WTextFile() {
  Close();
}
