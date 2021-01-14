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
#include "sdk/gfiles.h"

// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/gfiles_cereal.h"

#include "core/cereal_utils.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/acs/expr.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"
#include <string>
#include <utility>
#include <vector>

using cereal::make_nvp;
using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

#pragma pack(push, 1)

struct gfiledirrec {
  char name[41],   // g-file section name
      filename[9]; // g-file database filename

  uint8_t sl, // sl required to read
      age;    // minimum age for section

  uint16_t maxfiles, // max # of files
      ar;            // AR for g-file section
};

struct gfilerec {
  char description[81], // description of file
      filename[13];     // filename of file

  daten_t daten; // date added
};

static_assert(sizeof(gfilerec) == 98, "gfilerec == 98");
static_assert(sizeof(gfiledirrec) == 56, "gfiledirrec == 56");

#pragma pack(pop)

namespace wwiv::sdk {

bool GFiles::LoadFromJSON(const std::filesystem::path& dir, const std::string& filename,
                          std::vector<gfile_dir_t>& entries) {
  entries.clear();
  JsonFile f(FilePath(dir, filename), "gfiles", entries, 1);
  return f.Load();
}

bool GFiles::SaveToJSON(const std::filesystem::path& dir, const std::string& filename,
                        const std::vector<gfile_dir_t>& entries) {
  const auto path = FilePath(dir, filename);
  backup_file(path, max_backups_);
  JsonFile f(path, "gfiles", entries, 1);
  return f.Save();
}

bool GFiles::set_dirs(const std::vector<gfile_dir_t>& dirs) {
  dirs_ = dirs;
  return true;
}

GFiles::GFiles(std::filesystem::path datadir, int max_backups)
    : datadir_(std::move(datadir)), max_backups_(max_backups){};

GFiles::~GFiles() = default;

bool GFiles::LoadLegacy() {
  std::vector<gfiledirrec> gfilesec;
  if (DataFile<gfiledirrec> file(FilePath(datadir_, GFILE_DAT)); file) {
    file.ReadVector(gfilesec);
  }
  for (const auto& o : gfilesec) {
    gfile_dir_t gf{};
    gf.name = o.name;
    gf.filename = o.filename;
    gf.maxfiles = o.maxfiles;
    acs::AcsExpr ae;
    gf.acs = ae.min_sl(o.sl).min_age(o.age).ar_int(o.ar).get();

    const auto gfl_file_name = StrCat(gf.filename, ".gfl");
    const auto gfl_path = FilePath(datadir_, gfl_file_name);
    if (DataFile<gfilerec> gfile(gfl_path); gfile) {
      if (std::vector<gfilerec> files; gfile.ReadVector(files)) {
        for (const auto& of : files) {
          gfile_t f{};
          f.filename = of.filename;
          f.daten = of.daten;
          f.description = of.description;
          gf.files.emplace_back(f);
        }
      }
    }
    dirs_.emplace_back(gf);
  }
  return true;
}

bool GFiles::Load() {
  const auto json_path = FilePath(datadir_, "gfiles.json");
  const auto legacy_path = FilePath(datadir_, GFILE_DAT);
  if (!File::Exists(json_path) && File::Exists(legacy_path)) {
    const auto loaded = LoadLegacy();
    if (loaded) {
      return Save();
    }
    return false;
  }
  return LoadFromJSON(datadir_, "gfiles.json", dirs_);
}

bool GFiles::Save() { return SaveToJSON(datadir_, "gfiles.json", dirs_); }

bool GFiles::insert(int n, gfile_dir_t r) { return insert_at(dirs_, n, r); }

bool GFiles::erase(int n) { return erase_at(dirs_, n); }

const gfile_dir_t& GFiles::dir(const std::string& filename) const {
  for (const auto& n : dirs_) {
    if (iequals(filename, n.filename)) {
      return n;
    }
  }
  throw std::out_of_range(StrCat("Unable to find dir of filename: ", filename));
}

gfile_dir_t& GFiles::dir(const std::string& filename) {
  for (auto& n : dirs_) {
    if (iequals(filename, n.filename)) {
      return n;
    }
  }
  throw std::out_of_range(StrCat("Unable to find dir of filename: ", filename));
}

bool GFiles::exists(const std::string& filename) const {
  for (const auto& n : dirs_) {
    if (iequals(filename, n.filename)) {
      return true;
    }
  }

  return false;
}

} // namespace wwiv::sdk
