/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*           Copyright (C)2020-2022, WWIV Software Services               */
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
/**************************************************************************/
#include "sdk/files/diz.h"
#include <algorithm>
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include <utility>

// CP437 box drawing and block characters, written as escapes so the file
// stays ASCII (MSVC compiles with /utf-8, required by fmt 11+).
static const char* invalid_chars = "\xda\xbf\xc0\xd9\xc4\xb3\xc3\xb4\xc1\xc2\xc9\xbb\xc8\xbc\xcd\xba\xcc\xb9\xca\xcb\xd5\xb8\xd4\xbe\xcd\xb3\xc6\xb5\xcf\xd1\xd6\xb7\xd3\xbd\xc4\xba\xc7\xb6\xd0\xd2\xc5\xce\xd8\xd7\xb0\xb1\xb2\xdb\xdf\xdc\xdd\xde";

wwiv::sdk::files::Diz::Diz(std::string description, std::string extended_description)
    : description_(std::move(description)), extended_description_(std::move(extended_description)) {}

std::string wwiv::sdk::files::Diz::description() const noexcept { return description_; }

std::string wwiv::sdk::files::Diz::extended_description() const noexcept {
  return extended_description_;
}

wwiv::sdk::files::DizParser::DizParser(bool firstline_as_desc)
    : firstline_as_desc_(firstline_as_desc) {}


static bool valid_desc(const std::string& description) {
  // I don't think this function is really doing what it should
  // be doing, but am not sure what it should be doing instead.
  for (const auto& c : description) {
    if (c > '@' && c < '{') {
      return true;
    }
  }
  return false;
}

static std::string fixup_diz_string(const std::string& orig) {
  auto s{orig};
  std::replace(s.begin(), s.end(), '\x0c', ' ');
  std::replace(s.begin(), s.end(), '\x1a', ' ');
  return s;
}

std::optional<wwiv::sdk::files::Diz> wwiv::sdk::files::DizParser::parse(
    const std::filesystem::path& path) const {
  if (!core::File::Exists(path)) {
    VLOG(1) << ":DizParser::parse: path not exist: " << path;
    return std::nullopt;
  }

  std::string description;

  TextFile file(path, "rt");
  auto lines = file.ReadFileIntoVector();
  auto iter = std::begin(lines);
  const auto end = std::end(lines);
  while (iter != end && iter->empty()) {
    // skip over any empty lines at the start.
    ++iter;
  }
  // Save off the first non empty line.
  const auto first_real_line = iter;
  // iter == end covers an empty file and a file of only blank lines.
  if (iter == end) {
    VLOG(1) << "DizParser::parse: lines empty";
    return std::nullopt;
  }

  if (firstline_as_desc_) {
    auto ss = *iter;
    if (!ss.empty()) {
      for (auto& s : ss) {
        if (strchr(invalid_chars, s) != nullptr && s != 26) {
          s = ' ';
        }
      }
      if (!valid_desc(ss)) {
        // The old do/while incremented, then dereferenced *iter, then
        // checked iter != end, so a DIZ with no letter-bearing line
        // (only punctuation, digits, or box-drawing) read past the end.
        for (++iter; iter != end; ++iter) {
          ss = *iter;
          if (valid_desc(ss)) {
            break;
          }
        }
        if (iter == end) {
          VLOG(1) << "Description is empty, failed to parse.";
          return std::nullopt;
        }
      }
    }
    if (!ss.empty() && ss.back() == '\r') {
      ss.pop_back();
    }

    description = strings::StringTrim(ss);
    // Only bail here if we have nothing.
    if (description.empty()) {
      VLOG(1) << "Description is empty, failed to parse.";
      return std::nullopt;
    }

    ++iter;
  } else {
    iter = first_real_line;
  }
  std::string ext;
  for (; iter != end; ++iter) {
    ext.append(fixup_diz_string(*iter));
    ext.append("\n");
  }

  if (description.empty() && ext.empty()) {
    return std::nullopt;
  }

  return {Diz(description, ext)};
}

static constexpr const char* kFileIdDiz = "FILE_ID.DIZ";
static constexpr const char* kDescSdi = "DESC.SDI";

bool wwiv::sdk::files::is_diz_filename(const std::string& filename) {
  return strings::iequals(filename, kFileIdDiz) || strings::iequals(filename, kDescSdi);
}

// Returns all of the description files in dir, whatever their case.
static std::vector<std::filesystem::path> diz_files_in(const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> result;
  std::error_code ec;
  for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code ec2;
    if (it->is_regular_file(ec2) &&
        wwiv::sdk::files::is_diz_filename(it->path().filename().string())) {
      result.push_back(it->path());
    }
  }
  return result;
}

int wwiv::sdk::files::remove_diz_files(const std::filesystem::path& dir) {
  auto count = 0;
  for (const auto& p : diz_files_in(dir)) {
    std::error_code ec;
    if (std::filesystem::remove(p, ec)) {
      ++count;
    } else {
      LOG(WARNING) << "Unable to remove old description file: " << p << ": " << ec.message();
    }
  }
  return count;
}

std::optional<std::filesystem::path>
wwiv::sdk::files::find_diz_file(const std::filesystem::path& dir) {
  std::optional<std::filesystem::path> sdi;
  for (const auto& p : diz_files_in(dir)) {
    if (strings::iequals(p.filename().string(), kFileIdDiz)) {
      return p;
    }
    if (!sdi) {
      sdi = p;
    }
  }
  return sdi;
}
