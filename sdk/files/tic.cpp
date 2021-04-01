/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*           Copyright (C)2020-2021, WWIV Software Services               */
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
#include "sdk/files/dirs.h"
#include "sdk/net/net.h"
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;

namespace wwiv::sdk::files {

Tic::Tic(std::filesystem::path path)
    : path_(std::move(path)), valid_(false) {
}

bool Tic::IsValid() const {
  return valid_ && exists() && crc_valid() && size_valid();
}

bool Tic::crc_valid() const {
  const auto actual_int = wwiv::core::crc32file(fpath());
  const auto actual = fmt::sprintf("%8.8x", actual_int);

  const auto crc_valid = iequals(crc, actual);
  if (!crc_valid) {
    LOG(INFO) << "CRC: actual: " << actual << "; expected: " << crc;
  }
  return crc_valid;
}

bool Tic::size_valid() const {
  const File f(fpath());
  const int actual_size = static_cast<int>(f.length());
  if (actual_size != size()) {
    LOG(INFO) << "Tic FileSize !valid. actual: " << actual_size << "; expected: " << size();
    return false;
  }
  return true;
}

bool Tic::exists() const {
  return File::Exists(fpath());
}

int Tic::size() const {
  if (size_ != 0) {
    return size_;
  }
  const File f(fpath());
  return static_cast<int>(f.length());
}

core::DateTime Tic::date() const {
  if (date_.to_time_t() == 0) {
    const auto t = File::last_write_time(fpath());
    return DateTime::from_time_t(t);
  }
  return date_;
}

std::filesystem::path Tic::fpath() const {
  auto fp = FilePath(path_.parent_path(), file);
  return fp;
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
      if (const auto o = fido::try_parse_fidoaddr(params, fido::fidoaddr_parse_t::lax)) {
        t.origin = o.value();
      } else {
        LOG(WARNING) << "Unable to parse 'origin' from : '" << params << "'";
      }
    } else if (keyword == "from") {
      if (const auto o = fido::try_parse_fidoaddr(params, fido::fidoaddr_parse_t::lax)) {
        t.from = o.value();
      } else {
        LOG(WARNING) << "Unable to parse 'from' from : '" << params << "'";
      }
    } else if (keyword == "to") {
      if (const auto o = fido::try_parse_fidoaddr(params, fido::fidoaddr_parse_t::lax)) {
        t.to = o.value();
      } else {
        LOG(WARNING) << "Unable to parse 'to' from : '" << params << "'";
      }
    } else if (keyword == "file") {
      t.file = params;
    } else if (keyword == "lfile" || keyword == "fullname") {
      t.lfile = params;
    } else if (keyword == "size") {
      t.size_ = to_number<int>(params);
    } else if (keyword == "date") {
      const auto dtt = to_number<time_t>(params);
      t.date_ = DateTime::from_time_t(dtt);
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

std::optional<directory_t> FindFileAreaForTic(const files::Dirs& dirs, const Tic& tic,
                                              const Network& net) {
  const auto area_tag = tic.area;
  for (const auto& d : dirs.dirs()) {
    for (const auto& dt : d.area_tags) {
      VLOG(3) << "Checking area: '" << dt.area_tag << "'";
      if (iequals(area_tag, dt.area_tag)) {
        VLOG(3) << "Found matching area, checking UUID for area: '" << dt.net_uuid << "'";
        if (dt.net_uuid == net.uuid) {
          return {d};
        }
        VLOG(1) << "UUID didn't match: [dir] " << dt.net_uuid << " != [net]:  " << net.uuid;
      }
    }
  }
  return std::nullopt;
}

} // namespace wwiv::sdk::files
