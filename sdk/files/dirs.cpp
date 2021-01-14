/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include "sdk/files/dirs.h"

// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>

#include "core/datafile.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "core/cereal_utils.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"
#include "sdk/acs/expr.h"
#include "sdk/files/dirs_cereal.h"
#include <string>
#include <utility>
#include <vector>

using cereal::make_nvp;
using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::sdk::files {

std::ostream& operator<<(std::ostream& os, const sdk::files::directory_t& d) {
  os << d.name << " (" << d.filename << ")";
  return os;
}

bool Dirs::LoadFromJSON(const std::filesystem::path& dir, const std::string& filename, std::vector<directory_t>& entries) {
  entries.clear();
  JsonFile f(FilePath(dir, filename), "dirs", entries, 1);
  return f.Load();
}

//static 
bool Dirs::SaveToJSON(const std::filesystem::path& dir, const std::string& filename, const std::vector<directory_t>& entries) {
  JsonFile f(FilePath(dir, filename), "dirs", entries, 1);
  return f.Save();
}

bool Dirs::set_dirs(const std::vector<directory_t>& dirs) {
  dirs_ = dirs;
  return true;
}

vector<directoryrec_422_t> read_dirs(const std::filesystem::path& datadir) {
  DataFile<directoryrec_422_t> file(FilePath(datadir, DIRS_DAT));
  if (!file) {
    // LOG(ERROR) << file.file() << " NOT FOUND.";
    return{};
  }
  std::vector<directoryrec_422_t> dirs;
  if (!file.ReadVector(dirs)) {
    return{};
  }
  return dirs;
}

bool write_dirs(const std::filesystem::path& datadir, const vector<directoryrec_422_t>& dirs) {
  DataFile<directoryrec_422_t> file(FilePath(datadir, DIRS_DAT),
                                        File::modeBinary | File::modeReadWrite |
                                            File::modeCreateFile | File::modeTruncate,
                                        File::shareDenyReadWrite);
  if (!file) {
    return false;
  }
  return file.WriteVector(dirs);
}


// Classes

Dirs::Dirs(std::filesystem::path datadir, int max_backups) : datadir_(std::move(datadir)), max_backups_(max_backups) {};

Dirs::~Dirs() = default;

bool Dirs::Load() {
  if (!LoadFromJSON(datadir_.string(), DIRS_JSON, dirs_)) {
    return LoadLegacy();
  }
  return true;
}

bool Dirs::LoadLegacy() {
  LOG(INFO) << "Reading Legacy Dirs";
  auto old_dirs = read_dirs(datadir_);

  dirs_.clear();
  for (auto i = 0; i < wwiv::stl::ssize(old_dirs); i++) {
    const auto& olds = stl::at(old_dirs, i);
    directory_t dir{};
    dir.name = olds.name;
    dir.filename = olds.filename;
    dir.path = olds.path;
    acs::AcsExpr ae;
    dir.acs = ae.min_dsl(olds.dsl).min_age(olds.age).dar_int(olds.dar).get();
    dir.maxfiles = olds.maxfiles;
    dir.mask = olds.mask;
    dirs_.emplace_back(std::move(dir));
  }
  return true;
}

bool Dirs::Save() {
  std::vector<directoryrec_422_t> dirs;

  for (const auto& s : dirs_) {
    directoryrec_422_t ls{};
    to_char_array(ls.name, s.name);
    to_char_array(ls.filename, s.filename);
    to_char_array(ls.path, s.path);
    //ls.dsl = s.dsl;
    //ls.age = s.age;
    //ls.dar = s.dar;
    ls.maxfiles = s.maxfiles;
    ls.mask = s.mask;
    dirs.emplace_back(ls);
  }

  if (!write_dirs(datadir_, dirs)) {
    LOG(ERROR) << "Error saving dirs";
    return false;
  }
  // Backup dirs.json
  backup_file(FilePath(datadir_, DIRS_JSON), max_backups_);

  // Save dirs.
  return SaveToJSON(datadir_, DIRS_JSON, dirs_);
}

bool Dirs::insert(int n, directory_t r) {
  return insert_at(dirs_, n, r);
}

bool Dirs::erase(int n) {
  return erase_at(dirs_, n);
}

const directory_t& Dirs::dir(const std::string& filename) const {
  for (const auto& n : dirs_) {
    if (iequals(filename, n.filename)) {
      return n;
    }
  }
  throw std::out_of_range(StrCat("Unable to find dir of filename: ", filename));
}

directory_t& Dirs::dir(const std::string& filename) {
  for (auto& n : dirs_) {
    if (iequals(filename, n.filename)) {
      return n;
    }
  }
  throw std::out_of_range(StrCat("Unable to find dir of filename: ", filename));
}

bool Dirs::exists(const std::string& filename) const {
  for (const auto& n : dirs_) {
    if (iequals(filename, n.filename)) {
      return true;
    }
  }

  return false;
}

}
