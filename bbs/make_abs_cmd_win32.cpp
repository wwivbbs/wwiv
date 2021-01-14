/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services            */
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
#include "bbs/make_abs_cmd.h"

#include "core/file.h"
#include "core/strings.h"
#include "fmt/format.h"
#include <direct.h>
#include <filesystem>
#include <string>
#include <vector>

using std::string;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::strings;

void make_abs_cmd(const std::filesystem::path& root, std::string* out) {
  
  static const vector<string> exts{
    "",
    ".com",
    ".exe",
    ".bat",
    ".btm",
    ".cmd",
  };

  auto s1{*out};
  const auto is_abs_with_drive = s1.size() > 2 && s1.at(1) == ':';

  std::string s2;
  if (is_abs_with_drive) {
    if (s1.at(2) != '\\') {
      auto* curdir = _getdcwd(to_upper_case_char(s1[0]) - 'A' + 1, nullptr, 0);
      if (curdir && *curdir) {
        s1 = fmt::format("{}:\\{}\\{}", s1.front(), curdir, s1.substr(2));
      } else {
        s1 = fmt::format("{}:\\{}", s1.front(), s1.substr(2));
      }
      free(curdir);
    }
  } else if (s1.front() == '\\') {
    auto drive = root.string().front();
    s1 = fmt::format("{}:{}", drive, s1);
  } else {
    s2 = s1;
    const auto wsidx = s2.find_first_of(" \t");
    if (wsidx != std::string::npos) {
      if (s2.find(File::pathSeparatorChar) != std::string::npos) {
        s1 = FilePath(root, s1).string();
      }
    }
  }

  const auto idx = s1.find(' ');
  if (idx != std::string::npos) {
    s2 = fmt::format(" {}", s1.substr(idx + 1));
    s1 = s1.substr(0, idx);
  } else {
    s2.clear();
  }
  for (const auto& ext : exts) {
    if (ext.empty()) {
      const auto last_slash = s1.find_last_of(File::pathSeparatorChar);
      auto t = last_slash == std::string::npos ? s1 : s1.substr(last_slash + 1);
      if (strchr(t.c_str(), '.') == nullptr) {
        continue;
      }
    }
    auto s = StrCat(s1, ext);
    if (s1[1] == ':') {
      if (File::Exists(s)) {
        if (File::is_directory(s)) {
          *out = FilePath(s, s2).string();
        } else {
          *out = StrCat(s, s2);
        }
        return;
      }
    } else {
      if (File::Exists(s)) {
        std::error_code ec;
        if (is_directory(root, ec) && !is_directory(FilePath(root, s), ec)) {
          *out = StrCat(FilePath(root, s).string(), s2);
        } else {
          *out = FilePath(root, StrCat(s, s2)).string();
        }
        return;
      }
      char szFoundPath[4096];
      const auto err = _searchenv_s(s.c_str(), "PATH", szFoundPath);
      if (err == 0 && strlen(szFoundPath) > 0) {
        *out = StrCat(szFoundPath, s2);
        return;
      }
    }
  }

  const auto maybe_dir = FilePath(root, s1);
  std::error_code ec;
  if (File::Exists(maybe_dir) && is_directory(maybe_dir, ec)) {
    *out = StrCat(maybe_dir.string(), s2);
  } else {
    *out = FilePath(root, StrCat(s1, s2)).string();
  }
}

