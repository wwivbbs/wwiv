/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2005-2019, WWIV Software Services             */
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
#include "core/textfile.h"
// Always declare wwiv_windows.h first to avoid collisions on defines.
#include "core/wwiv_windows.h"

#include <iostream>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <cstdarg>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>

#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/wwivport.h"

#ifdef _WIN32
#include <io.h>
#include <share.h>

#else  // _WIN32
#include <sys/file.h>
#include <unistd.h>
#endif  // _WIN32

using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;
using std::chrono::milliseconds;

using namespace wwiv::os;

namespace {
const int WAIT_TIME = 10;
const int TRIES = 100;
}

#ifdef _WIN32
FILE* OpenImpl(const std::string& name, const std::string& mode) {
  int share = SH_DENYWR;
  int md = 0;
  if (strchr(mode.c_str(), 'w') != nullptr) {
    share = SH_DENYRD;
    md = O_RDWR | O_CREAT | O_TRUNC;
  } else if (strchr(mode.c_str(), 'a') != nullptr) {
    share = SH_DENYRD;
    md = O_RDWR | O_CREAT;
  } else {
    md = O_RDONLY;
  }
  if (strchr(mode.c_str(), 'b') != nullptr) {
    md |= O_BINARY;
  }
  if (strchr(mode.c_str(), '+') != nullptr) {
    md &= ~O_RDONLY;
    md |= O_RDWR;
    share = SH_DENYRD;
  }

  int fd = _sopen(name.c_str(), md, share, S_IREAD | S_IWRITE);
  if (fd < 0) {
    int count = 1;
    if (File::Exists(name)) {
      sleep_for(milliseconds(WAIT_TIME));
      fd = _sopen(name.c_str(), md, share, S_IREAD | S_IWRITE);
      while ((fd < 0 && errno == EACCES) && count < TRIES) {
        sleep_for(milliseconds(WAIT_TIME));
        count++;
        fd = _sopen(name.c_str(), md, share, S_IREAD | S_IWRITE);
      }
    }
  }

  if (fd > 0) {
    if (strchr(mode.c_str(), 'a')) {
      _lseek(fd, 0L, SEEK_END);
    }

    FILE* hFile = _fdopen(fd, mode.c_str());
    if (!hFile) {
      _close(fd);
    }
    return hFile;
  }
  return nullptr;
}

#else  // _WIN32
FILE* OpenImpl(const std::string& name, const std::string& mode) {
  FILE* f = fopen(name.c_str(), mode.c_str());
  if (f != nullptr) {
    flock(fileno(f), (strpbrk(mode.c_str(), "wa+")) ? LOCK_EX : LOCK_SH);
  }
  return f;
}
#endif // _WIN32

static std::string fopen_compatible_mode(const std::string& m) { 
  if (m.find('d') == std::string::npos) {
    return m;
  }
  std::string s{m};
  return StringReplace(&s, "d", "t");
}

TextFile::TextFile(const string& file_name, const string& file_mode)
    : file_name_(file_name), file_(OpenImpl(file_name, fopen_compatible_mode(file_mode))),
      dos_mode_(strchr(file_mode.c_str(), 'd') != nullptr) {}

bool TextFile::Close() {
  if (file_ == nullptr) {
    return false;
  }
  fclose(file_);
  file_ = nullptr;
  return true;
}

ssize_t TextFile::WriteChar(char ch) { return fputc(ch, file_); }

ssize_t TextFile::Write(const std::string& text) {
  return static_cast<ssize_t>((fputs(text.c_str(), file_) >= 0) ? text.size() : 0);
}

ssize_t TextFile::WriteLine(const string& text) { 
  if (file_ == nullptr) {
    return -1;
  }
  auto num_written = Write(text);
  // fopen in text mode will force \n -> \r\n on win32
#ifdef _WIN32
  fputs("\n", file_);
#else
  if (dos_mode_) {
    fputs("\r\n", file_);
  } else {
    fputs("\n", file_);
  }
#endif
  // TODO(rushfan): Should we just +=1 on non-win32?
  num_written += 2;
  return num_written;
}

ssize_t TextFile::WriteFormatted(const char* formatText, ...) {
  va_list ap;
  char buffer[4096];

  va_start(ap, formatText);
  vsnprintf(buffer, sizeof(buffer), formatText, ap);
  va_end(ap);
  return Write(buffer);
}

static void StripLineEnd(char *str) {
  size_t i = strlen(str);
  while ((i > 0) && ((str[i - 1] == 10) || str[i-1] == 13)) {
    --i;
  }
  str[i] = '\0';
}

bool TextFile::ReadLine(string *out) {
  if (file_ == nullptr) {
    return false;
  }
  char s[4096];
  char *p = fgets(s, sizeof(s), file_);
  if (p == nullptr) {
    return false;
  }
  // Strip off trailing \r\n
  StripLineEnd(p);
  out->assign(p);
  return true;
}

TextFile::~TextFile() {
  Close();
}

std::string TextFile::ReadFileIntoString() {
  VLOG(3) << "ReadFileIntoString: ";
  if (file_ == nullptr) {
    return "";
  }
  string contents;
  fseek(file_, 0, SEEK_END);
  contents.resize(static_cast<unsigned long>(ftell(file_)));
  rewind(file_);
  auto num_read = fread(&contents[0], 1, contents.size(), file_);
  contents.resize(num_read); 
  return contents;
}

std::vector<std::string> TextFile::ReadFileIntoVector() {
  std::vector<std::string> result;
  string line;
  while (ReadLine(&line)) {
    result.push_back(line);
  }
  return result;
}

std::ostream& operator<<(std::ostream& os, const TextFile& file) {
  os << file.full_pathname();
  return os;
}
