/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2021, WWIV Software Services               */
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
#include "sdk/fido/flo_file.h"

#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include <string>
#include <utility>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::sdk::fido {

  
static std::vector<std::pair<std::string, flo_directive>>
ParseFloFile(const std::filesystem::path& path) {
  TextFile file(path, "r");
  if (!file.IsOpen()) {
    return {};
  }

  std::vector<std::pair<std::string, flo_directive>> result;
  std::string line;
  while (file.ReadLine(&line)) {
    StringTrim(&line);
    if (line.empty()) {
      continue;
    }
    if (auto st = line.front(); st == '^' || st == '#' || st == '~') {
      const auto fn = line.substr(1);
      result.emplace_back(fn, static_cast<flo_directive>(st));
    }
  }
  return result;
}

FloFile::FloFile(const Network& net, std::filesystem::path p)
    : net_(net), path_(std::move(p)) {
  std::filesystem::path fn;
  if (!path_.has_filename()) {
    // This is a malformed flo file
    LOG(ERROR) << "Malformed FLO file found, bad filename: " << path_;
    return;
  }
  if (!path_.has_extension()) {
    // This is a malformed flo file
    LOG(ERROR) << "Malformed FLO file found, no extension: " << path_;
    return;
  }

  auto basename = ToStringLowerCase(path_.stem().string());
  auto ext = ToStringLowerCase(path_.extension().string());

  // Check for 4 so that it's dot + 3 letter extension
  if (ext.length() != 4) {
    // malformed flo file
    LOG(ERROR) << "Malformed FLO file found, bad extension: " << path_;
    return;
  }
  // extract the dot
  ext = ext.substr(1);

  if (basename.length() != 8) {
    // malformed flo file
    LOG(ERROR) << "Malformed FLO file found, bad stem: " << path_;
    return;
  }
  if (!ends_with(ext, "lo")) {
    // malformed flo file
    LOG(ERROR) << "Malformed FLO file found, bad extension: " << path_;
    return;
  }
  status_ = static_cast<fido_bundle_status_t>(ext.front());

  auto netstr = basename.substr(0, 4);
  auto net_num = to_number<int16_t>(netstr, 16);
  auto nodestr = basename.substr(4);
  auto node_num = to_number<int16_t>(nodestr, 16);

  FidoAddress source(net_.fido.fido_address);
  dest_.reset(new FidoAddress(source.zone(), net_num, node_num, 0, source.domain()));

  Load();
}

FloFile::~FloFile() = default;

bool FloFile::insert(const std::string& file, flo_directive directive) {
  entries_.emplace_back(file, directive);
  return true;
}

bool FloFile::clear() noexcept {
  entries_.clear();
  poll_ = false;
  return true;
}

bool FloFile::erase(const std::string& file) {
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if ((*it).first == file) {
      entries_.erase(it);
      return true;
    }
  }
  return false;
}

bool FloFile::Load() {
  exists_ = File::Exists(path_);
  if (!exists_) {
    return false;
  }

  const File f(path_);
  poll_ = f.length() == 0;
  entries_ = ParseFloFile(path_);
  return true;
}

bool FloFile::Save() {
  if (poll_ || !entries_.empty()) {
    File f(path_);
    if (!f.Open(File::modeCreateFile | File::modeReadWrite | File::modeText | File::modeTruncate,
                File::shareDenyReadWrite)) {
      return false;
    }
    for (const auto& e : entries_) {
      auto dr = static_cast<char>(e.second);
      const auto& name = e.first;
      f.Writeln(StrCat(dr, name));
    }
    return true;
  }
  if (File::Exists(path_)) {
    return File::Remove(path_);
  }
  return true;
}

FidoAddress FloFile::destination_address() const { return *dest_; }

void FloFile::set_poll(bool p) { poll_ = p; }

const std::vector<std::pair<std::string, flo_directive>>& FloFile::flo_entries() const {
  return entries_;
}


}
