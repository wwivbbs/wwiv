/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/json.hpp>

#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

#include <sstream>
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

std::vector<directoryrec_422_t> read_dirs(const std::string &datadir);
bool write_dirs(const std::filesystem::path& datadir, const std::vector<directoryrec_422_t>& dirs);

template <class Archive>
void serialize(Archive & ar, directory_t& s) {
  ar(make_nvp("name", s.name));
  ar(make_nvp("filename", s.filename));
  ar(make_nvp("dsl", s.dsl));
  ar(make_nvp("age", s.age));
  ar(make_nvp("dar", s.dar));
  ar(make_nvp("maxfiles", s.maxfiles));
  ar(make_nvp("mask", s.mask));
  ar(make_nvp("area_tag", s.area_tag));
}

template <class Archive>
void serialize(Archive & ar, dirs_t& d) {
  ar(cereal::make_nvp("dirs", d.dirs));
}

bool Dirs::LoadFromJSON(const std::filesystem::path& dir, const std::string& filename, dirs_t& s) {
  s.dirs.clear();
  TextFile file(FilePath(dir, filename), "r");
  if (!file.IsOpen()) {
    return false;
  }
  const auto text = file.ReadFileIntoString();
  std::stringstream ss;
  ss << text;
  cereal::JSONInputArchive load(ss);
  load(cereal::make_nvp("dirs", s.dirs));

  return true;
}

//static 
bool Dirs::SaveToJSON(const std::filesystem::path& dir, const std::string& filename, const dirs_t& s) {
  std::ostringstream ss;
  {
    cereal::JSONOutputArchive save(ss);
    save(cereal::make_nvp("dirs", s.dirs));
  }

  TextFile file(FilePath(dir, filename), "w");
  if (!file.IsOpen()) {
    // rapidjson will assert if the file does not exist, so we need to 
    // verify that the file exists first.
    return false;
  }

  file.Write(ss.str());
  return true;
}

bool Dirs::set_dirs(const std::vector<directory_t>& dirs) {
  dirs_ = dirs;
  return true;
}

vector<directoryrec_422_t> read_dirs(const std::filesystem::path& datadir) {
  DataFile<directoryrec_422_t> file(FilePath(datadir, DIRS_DAT));
  if (!file) {
    // TODO(rushfan): Figure out why this caused link errors. What's missing?
    //LOG(ERROR) << file.file() << " NOT FOUND.";
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

Dirs::Dirs(std::filesystem::path datadir) : datadir_(std::move(datadir)){};

Dirs::~Dirs() = default;

bool Dirs::Load() {
  dirs_t s;
  if (!LoadFromJSON(datadir_.string(), DIRS_JSON, s)) {
    return LoadLegacy();
  }
  // Assign the dirs.
  dirs_ = s.dirs;
  return true;
}

bool Dirs::LoadLegacy() {
  auto old_dirs = read_dirs(datadir_);

  dirs_.clear();
  for (auto i = 0; i < wwiv::stl::ssize(old_dirs); i++) {
    const auto& olds = old_dirs.at(i);
    directory_t dir{};
    dir.name = olds.name;
    dir.filename = olds.filename;
    dir.path = olds.path;
    dir.dsl = olds.dsl;
    dir.age = olds.age;
    dir.dar = olds.dar;
    dir.age = olds.age;
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
    ls.dsl = s.dsl;
    ls.age = s.age;
    ls.dar = s.dar;
    ls.maxfiles = s.maxfiles;
    ls.mask = s.mask;
    dirs.emplace_back(ls);
  }

  if (!write_dirs(datadir_, dirs)) {
    LOG(ERROR) << "Error saving dirs";
    return false;
  }
  // Backup dirs.json
  backup_file(FilePath(datadir_, DIRS_JSON));

  // Save dirs.
  dirs_t t{};
  t.dirs = dirs_;
  return SaveToJSON(datadir_, DIRS_JSON, t);
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
