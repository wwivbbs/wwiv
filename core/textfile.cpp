/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2005-2021, WWIV Software Services             */
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

#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <cstdarg>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>

#else  // _WIN32
#include <sys/file.h>
#include <unistd.h>
#endif  // _WIN32

using std::chrono::milliseconds;
using wwiv::core::File;
using wwiv::strings::StringReplace;
using namespace wwiv::os;

namespace {
const int WAIT_TIME = 10;
const int TRIES = 100;
}

#ifdef _WIN32
FILE* OpenImpl(const std::string& name, const std::string& mode) {
  auto share = SH_DENYWR;
  auto md = O_RDONLY;
  if (strchr(mode.c_str(), 'w') != nullptr) {
    share = SH_DENYRD;
    md = O_RDWR | O_CREAT | O_TRUNC;
  } else if (strchr(mode.c_str(), 'a') != nullptr) {
    share = SH_DENYRD;
    md = O_RDWR | O_CREAT;
  }

  if (strchr(mode.c_str(), 'b') != nullptr) {
    md |= O_BINARY;
  }
  if (strchr(mode.c_str(), '+') != nullptr) {
    md &= ~O_RDONLY;
    md |= O_RDWR;
    share = SH_DENYRD;
  }

  auto fd = _sopen(name.c_str(), md, share, S_IREAD | S_IWRITE);
  if (fd < 0) {
    auto count = 1;
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

    auto* hFile = _fdopen(fd, mode.c_str());
    if (!hFile) {
      _close(fd);
    }
    return hFile;
  }
  VLOG(1) << "TextFile::OpenImpl; fopen failed on file: '" << name;  
  return nullptr;
}

#else  // _WIN32
FILE* OpenImpl(const std::string& name, const std::string& mode) {
  FILE* f = fopen(name.c_str(), mode.c_str());
  if (f != nullptr) {
    flock(fileno(f), (strpbrk(mode.c_str(), "wa+")) ? LOCK_EX : LOCK_SH);
  } else {
    VLOG(1) << "TextFile::OpenImpl; fopen failed on file: '" << name << "'; errno: " << errno;
  }
  return f;
}
#endif // _WIN32

static std::string fopen_compatible_mode(const std::string& m) noexcept {
  if (m.find('d') == std::string::npos) {
    return m;
  }
  auto s{m};
  return StringReplace(&s, "d", "t");
}

TextFile::TextFile(const std::filesystem::path& file_name, const std::string& file_mode) noexcept
  : file_name_(file_name), file_(OpenImpl(file_name.string(), fopen_compatible_mode(file_mode))),
    open_(file_ != nullptr), dos_mode_(strchr(file_mode.c_str(), 'd') != nullptr) {
}

bool TextFile::Close() noexcept {
  if (file_ == nullptr) {
    return false;
  }
  fclose(file_);
  file_ = nullptr;
  open_ = false;
  return true;
}

// ReSharper disable once CppMemberFunctionMayBeConst
ssize_t TextFile::WriteChar(char ch) noexcept {
  DCHECK(file_);
  return fputc(ch, file_);
}

// ReSharper disable once CppMemberFunctionMayBeConst
ssize_t TextFile::Write(const std::string& text) noexcept {
  DCHECK(file_);
  return static_cast<ssize_t>((fputs(text.c_str(), file_) >= 0) ? text.size() : 0);
}

ssize_t TextFile::WriteLine(const std::string& text) noexcept {
  if (file_ == nullptr) {
    return -1;
  }
  auto num_written = Write(text);
  // fopen in text mode will force \n -> \r\n on win32
#if defined(_WIN32) || defined(__OS2__)
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

static void StripLineEnd(char* str) noexcept {
  auto i = strlen(str);
  while ((i > 0) && ((str[i - 1] == 10) || str[i - 1] == 13)) {
    --i;
  }
  str[i] = '\0';
}

// ReSharper disable once CppMemberFunctionMayBeConst
bool TextFile::ReadLine(std::string* out) noexcept {
  try {
    if (file_ == nullptr) {
      out->clear();
      return false;
    }
    char s[4096];
    auto* p = fgets(s, sizeof(s), file_);
    if (p == nullptr) {
      out->clear();
      return false;
    }
    // Strip off trailing \r\n
    StripLineEnd(p);
    out->assign(p);
    return true;
  } catch (...) {
    return false;
  }
}

File::size_type TextFile::position() const noexcept {
  DCHECK(file_);
  if (!file_) {
    return 0;
  }
  return ftell(file_);
}

const std::filesystem::path& TextFile::path() const noexcept {
  return file_name_;
}

std::string TextFile::full_pathname() const { return file_name_.string(); }

TextFile::~TextFile() noexcept {
  Close();
}

std::string TextFile::ReadFileIntoString() {
  VLOG(3) << "ReadFileIntoString: ";
  if (file_ == nullptr) {
    return {};
  }

  fseek(file_, 0, SEEK_END);
  const auto len = static_cast<unsigned long>(ftell(file_));
  rewind(file_);
  if (len == 0) {
    return {};
  }

  std::string contents;
  contents.resize(len);
  const auto num_read = fread(&contents[0], 1, contents.size(), file_);
  contents.resize(num_read);
  return contents;
}

std::vector<std::string> TextFile::ReadFileIntoVector(int64_t max_lines) {
  if (max_lines <= 0) {
    max_lines = std::numeric_limits<int64_t>::max();
  }
  std::vector<std::string> result;
  std::string line;
  while (ReadLine(&line) && max_lines-- > 0) {
    result.push_back(line);
  }
  return result;
}

std::vector<std::string> TextFile::ReadLastLinesIntoVector(int num_lines) {
  fseek(file_, 0, SEEK_END);
  const auto len = static_cast<unsigned long>(ftell(file_));
  const auto pos = std::max<int>(0, len - (num_lines * 1024));
  // move to new pos
  fseek(file_, pos, SEEK_SET);
  std::string contents;
  contents.resize(len - pos + 1);
  const auto num_read = fread(&contents[0], 1, contents.size(), file_);
  contents.resize(num_read);

  if (contents.back() == '\n') {
    contents.pop_back();
  }

  auto cur_num = 0;
  auto count = 0;
  for (auto it = std::rbegin(contents); it != std::rend(contents); ++it) {
    if (*it == '\n') {
      ++cur_num;
    }
    count++;
    if (cur_num == num_lines) {
      break;
    }
  }
  auto f = wwiv::stl::size_int(contents) - count;
  if (f > 0)  {
    ++f;
  }
  contents = contents.substr(f);
  return wwiv::strings::SplitString(contents, "\n", true);
}

std::ostream& operator<<(std::ostream& os, const TextFile& file) {
  os << file.full_pathname();
  return os;
}
