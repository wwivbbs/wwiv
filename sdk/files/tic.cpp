/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*                Copyright (C)2020, WWIV Software Services               */
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
#include "sdk/files/tic.h"

#include "core/crc32.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::sdk::files {

Tic::Tic(std::filesystem::path path) : path_(std::move(path)), valid_(false) {}
Tic::~Tic() = default;

bool Tic::IsValid() const {
  return valid_ && crc_valid() && exists();
}

bool Tic::crc_valid() const {
  const auto dir = path_.parent_path();
  const auto actual_int = wwiv::core::crc32file(FilePath(dir, file));
  const auto actual = fmt::sprintf("%8.8x", actual_int);

  const auto crc_valid = iequals(crc, actual);
  if (!crc_valid) {
    LOG(INFO) << "CRC: actual: " << actual << "; expected: " << crc;
  }
  return crc_valid;
}

bool Tic::exists() const {
  return File::Exists(path_);
}

TicParser::TicParser(std::filesystem::path dir) : dir_(std::move(dir)) {}

static std::tuple<std::string, std::string, bool> split_tic_line(const std::string& line) {
  const auto idx = line.find_first_of(" \t");
  if (idx == std::string::npos) {
    return std::make_tuple("", "", true);
  }
  auto keyword = ToStringLowerCase(line.substr(0, idx));
  StringTrimEnd(&keyword);
  const auto params = StringTrim(line.substr(idx + 1));
  return std::make_tuple(keyword, params, false);
}

std::optional<Tic> TicParser::parse(const std::string& filename) const {
  TextFile f(FilePath(dir_, filename), "rt");
  if (!f) {
    return std::nullopt;
  }
  const auto lines = f.ReadFileIntoVector();
  return parse(filename, lines);
}

std::optional<Tic> TicParser::parse(const std::string& filename, const std::vector<std::string>& lines) const {
  const auto p = FilePath(dir_, filename);
  Tic t(p);

  for (const auto& l : lines) {
    auto [keyword, params, error] = split_tic_line(l);
    if (error) {
      continue;
    }
    if (keyword == "area") {
      t.area = params;
    } else if (keyword == "areadesc") {
      t.area_description = params;
    } else if (keyword == "origin") {
      t.origin = fido::FidoAddress(params);
    } else if (keyword == "from") {
      t.from = fido::FidoAddress(params);
    } else if (keyword == "to") {
      t.to = fido::FidoAddress(params);
    } else if (keyword == "file") {
      t.file = params;
    } else if (keyword == "lfile") {
      t.lfile = params;
    } else if (keyword == "fullname") {
      t.lfile = params;
    } else if (keyword == "size") {
      t.size = to_number<int>(params);
    } else if (keyword == "date") {
      const auto dtt = to_number<time_t>(params);
      t.date = DateTime::from_time_t(dtt);
    } else if (keyword == "desc") {
      t.desc = params;
    } else if (keyword == "ldesc") {
      t.ldesc.push_back(params);
    } else if (keyword == "created") {
      t.created = params;
    } else if (keyword == "magic") {
      t.magic = params;
    } else if (keyword == "replaces") {
      t.replaces = params;
    } else if (keyword == "crc") {
      t.crc = params;
    } else if (keyword == "path") {
      // TODO(rushfan): Path and seenby are blocks like key path, numerous seen-by items for/
      // that path.
      t.ftn_path = params;
    } else if (keyword == "seenby") {
      t.seen_by = params;
    } else if (keyword == "pw") {
      t.pw = params;
    }
  }
  t.valid_ = true;
  return {t};
}
} // namespace wwiv::sdk::files
