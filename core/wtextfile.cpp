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

#include "core/strings.h"
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

WTextFile::WTextFile(const std::string& fileName, const std::string& fileMode) {
  Open(fileName, fileMode);
}

WTextFile::WTextFile(const std::string& directoryName, const std::string& fileName, const std::string& fileMode) {
  std::string fullPathName(directoryName);
  char last_char = directoryName.back();
  if (last_char != WFile::pathSeparatorChar) {
    fullPathName.push_back(WFile::pathSeparatorChar);
  }
  fullPathName.append(fileName);

  Open(fullPathName, fileMode);
}

bool WTextFile::Open(const std::string& file_name, const std::string& file_mode) {
  file_name_ = file_name;
  file_mode_ = file_mode;
  file_ = WTextFile::OpenImpl();
  return file_ != nullptr;
}

#ifdef _WIN32
FILE* WTextFile::OpenImpl() {
  int share = SH_DENYWR;
  int md = 0;
  if (strchr(file_mode_.c_str(), 'w') != nullptr) {
    share = SH_DENYRD;
    md = O_RDWR | O_CREAT | O_TRUNC;
  } else if (strchr(file_mode_.c_str(), 'a') != nullptr) {
    share = SH_DENYRD;
    md = O_RDWR | O_CREAT;
  } else {
    md = O_RDONLY;
  }
  if (strchr(file_mode_.c_str(), 'b') != nullptr) {
    md |= O_BINARY;
  }
  if (strchr(file_mode_.c_str(), '+') != nullptr) {
    md &= ~O_RDONLY;
    md |= O_RDWR;
    share = SH_DENYRD;
  }

  int fd = _sopen(file_name_.c_str(), md, share, S_IREAD | S_IWRITE);
  if (fd < 0) {
    int count = 1;
    if (WFile::Exists(file_name_)) {
      ::Sleep(WAIT_TIME);
      fd = _sopen(file_name_.c_str(), md, share, S_IREAD | S_IWRITE);
      while ((fd < 0 && errno == EACCES) && count < TRIES) {
        ::Sleep(WAIT_TIME);
        count++;
        fd = _sopen(file_name_.c_str(), md, share, S_IREAD | S_IWRITE);
      }
    }
  }

  if (fd > 0) {
    if (strchr(file_mode_.c_str(), 'a')) {
      _lseek(fd, 0L, SEEK_END);
    }

    FILE *hFile = _fdopen(fd, file_mode_.c_str());
    if (!hFile) {
      _close(fd);
    }
    return hFile;
  }
  return nullptr;
}

#else  // _WIN32
FILE* WTextFile::OpenImpl() {
  FILE *f = fopen(file_name_.c_str(), file_mode_.c_str());
  if (f != nullptr`) {
    flock(fileno(f), (strpbrk(file_mode_.c_str(), "wa+")) ? LOCK_EX : LOCK_SH);
  }
  return f;
}
#endif  // _WIN32

bool WTextFile::Close() {
  if (file_ == nullptr) {
    return false;
  }
  fclose(file_);
  file_ = nullptr;
  return true;
}

int WTextFile::WriteLine(const std::string& text) { 
  int num_written = (fputs(text.c_str(), file_) >= 0) ? text.size() : 0;
  // fopen in text mode will force \n -> \r\n on win32
  fputs("\n", file_);
  num_written += 2;
  return num_written;
}

int WTextFile::WriteFormatted(const char *pszFormatText, ...) {
  va_list ap;
  char szBuffer[4096];

  va_start(ap, pszFormatText);
  vsnprintf(szBuffer, sizeof(szBuffer), pszFormatText, ap);
  va_end(ap);
  return Write(szBuffer);
}

static void StripLineEnd(char *pszString) {
  size_t i = strlen(pszString);
  while ((i > 0) && ((pszString[i - 1] == 10) || pszString[i-1] == 13)) {
    --i;
  }
  pszString[i] = '\0';
}

bool WTextFile::ReadLine(std::string *buffer) {
  char szBuffer[4096];
  char *p = fgets(szBuffer, sizeof(szBuffer), file_);
  if (p == nullptr) {
    return false;
  }
  // Strip off trailing \r\n
  StripLineEnd(p);
  buffer->assign(p);
  return true;
}

WTextFile::~WTextFile() {
  Close();
}
