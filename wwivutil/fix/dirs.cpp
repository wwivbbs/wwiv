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
#include "wwivutil/fix/dirs.h"

#include "core/command_line.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/vardec.h"
#include "sdk/files/files.h"
#include <iostream>
#include <set>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using std::string;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv::wwivutil {

namespace {
const char* WWIV_FILE_DATE_FORMAT = "%m/%d/%y";
}

static bool ensure_path_exists(const wwiv::sdk::files::directory_t& d, bool dry_run) {
  if (File::Exists(d.path)) {
    return true;
  }

  LOG(INFO) << "Missing directory for file area: " << d;
  LOG(INFO) << "Path: " << d.path;
  if (dry_run) {
    LOG(INFO) << "Please create it.";
    return false;
  }
  LOG(INFO) << "Creating directory: " << d.path;
  return File::mkdirs(d.path);
}

std::unordered_set<std::string> CheckDirForDuplicates(sdk::files::FileArea& area, bool dry_run) {
  std::unordered_set<std::string> files;
  std::vector<uploadsrec> data;
  const auto& raw_files = area.raw_files();
  for (const auto& r : raw_files) {
    auto [it, added] = files.insert(r.filename);
    if (added) {
      data.push_back(r);
    } else {
      LOG(INFO) << "Found duplicate file: " << r.filename;
    }
  }
  if (data.size() != raw_files.size()) {
    // We deleted some, time to save.
    LOG(INFO) << "Removed " << std::abs(ssize(data) - ssize(raw_files)) << " duplicate files";
    if (area.set_raw_files(data)) {
      if (!dry_run) {
        area.Save();
      }
    }
  }
  return files;
}

static std::optional<std::unordered_set<std::string>>
RewriteExtendedDescriptions(const wwiv::sdk::Config& config, const sdk::files::directory_t& dir, sdk::files::FileArea& area,
                            std::unordered_set<std::string> actual_files, bool dry_run) {
  VLOG(1) << "Rewriting extended descriptions for: " << dir;
  auto ext_path = area.ext_path();

  if (!File::Exists(ext_path)) {
    VLOG(1) << "No extended descriptions for: " << dir;
    return std::nullopt;
  }
  auto tmp_path{ext_path};
  tmp_path.replace_extension(".tmp");

  std::unordered_set<std::string> files_with_ext_desc;
  if (!backup_file(ext_path, config.max_backups())) {
    LOG(ERROR) << "Error creating backup of Extended Description file: " << dir;
  }

  // move to .TMP
  if (dry_run) {
    if (!File::Copy(ext_path, tmp_path)) {
      LOG(ERROR) << "Error Copying Extended Description file: " << dir;
      return std::nullopt;
    }
  } else {
    if (!File::Move(ext_path, tmp_path)) {
      LOG(ERROR) << "Error Moving Extended Description file: " << dir;
      return std::nullopt;
    }
  }

  File in(tmp_path);
  if (!in.Open(File::modeReadOnly | File::modeBinary)) {
    LOG(ERROR) << "Error Reading Extended Description file: " << dir;
    return std::nullopt;
  }
  const auto file_size = in.length();
  if (!file_size) {
    VLOG(1) << "Extended description file is empty.";
    return std::nullopt;
  }

  File out(ext_path);
  if (!dry_run) {
    if (!out.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate)) {
      LOG(ERROR) << "Error Opening Extended Description file for Write: " << dir;
      return std::nullopt;
    }
  }

  for (long file_pos = 0; file_pos < file_size; ) {
    in.Seek(file_pos, File::Whence::begin);
    ext_desc_type ed{};
    const auto num_read = in.Read(&ed, sizeof(ext_desc_type));
    if (num_read != sizeof(ext_desc_type)) {
      // LOG ERROR? Return false here?
      break;
    }
    if (contains(actual_files, ed.name)) {
      // The file exists in the list, so let's write it and then remove
      // it, so we don't write dupes.
      actual_files.erase(ed.name);
      files_with_ext_desc.insert(ed.name);
      if (!dry_run) {
        out.Write(&ed, sizeof(ext_desc_type));
      }

      string ss;
      ss.resize(ed.len);
      in.Read(&ss[0], ed.len);
      if (!dry_run) {
        out.Write(&ss[0], ed.len);
      }
    } else {
      LOG(INFO) << "Removing file from ext description [does not exist] : " << ed.name;
    }
    file_pos += ed.len + static_cast<int>(sizeof(ext_desc_type));
  }
  return {files_with_ext_desc};
}

static uint32_t file_size(const std::filesystem::path& path) {
  const File f(path);
  return static_cast<uint32_t>(f.length());
}

static bool CheckAttributes(const wwiv::sdk::files::directory_t& dir, sdk::files::FileArea& area,
  const std::unordered_set<std::string>& files_with_ext_desc, bool dry_run) {
  const auto nf = area.number_of_files();
  auto area_dirty{false};
  for (auto i = 1; i < nf; i++) {
    auto dirty{false};
    auto f = area.ReadFile(i);
    const auto actual_extended = contains(files_with_ext_desc, f.aligned_filename());
    if (actual_extended != f.has_extended_description()) {
      dirty = true;
      LOG(INFO) << "Fixing ext desc mask on: " << f.filename();
      f.set_extended_description(actual_extended);
    }
    const auto path = FilePath(dir.path, f);
    const auto actual_size = wwiv::wwivutil::file_size(path);
    if (actual_size != f.numbytes()) {
      dirty = true;
      LOG(INFO) << "Fixing file size for: " << f.filename();
      f.set_numbytes(actual_size);
    }
    auto actual_dt = DateTime::from_time_t(File::last_write_time(path));
    const auto actual_dts = actual_dt.to_string(WWIV_FILE_DATE_FORMAT);
    if (!iequals(actual_dts, f.actual_date())) {
      dirty = true;
      LOG(INFO) << "Fixing file actual date for: " << f.filename() << "; date: " << actual_dts;
      f.set_actual_date(actual_dt);
    }

    if (dirty && !dry_run) {
      area_dirty = true;
      if (!area.UpdateFile(f, i)) {
        LOG(ERROR) << "Error Updating file: " << f.filename();
      }
    }
  }

  if (area_dirty && !dry_run) {
    return area.Save();
  }
  return true;
}

bool CheckExtendedDirAndAttributes(const wwiv::sdk::Config& config, const sdk::files::directory_t& dir, sdk::files::FileArea& area, bool dry_run) {
  const auto actual_files = CheckDirForDuplicates(area, dry_run);
  std::unordered_set<std::string> empty_set;
  const auto files_with_ext_desc =
      RewriteExtendedDescriptions(config, dir, area, actual_files, dry_run).value_or(empty_set);
  // Reload areas since duplicates could have been removed.
  area.Close();
  area.Load();
  return CheckAttributes(dir, area, files_with_ext_desc, dry_run);
}

static void checkFileAreas(const wwiv::sdk::Config& config, bool verbose, bool dry_run) {
  const auto datadir = config.datadir();
  sdk::files::Dirs dirs(datadir, config.max_backups());

  if (!dirs.Load()) {
    LOG(ERROR) << "Unble to load dirs.dat";
    return;
  }
  sdk::files::FileApi api(datadir);
  const auto& directories = dirs.dirs();

  LOG(INFO) << "Checking " << directories.size() << " directories.";
  for (const auto& d : directories) {
    if (d.mask & mask_cdrom) {
      LOG(INFO) << "Skipping directory '" << d.name << "' [CD-ROM]";
      continue;
    }
    if (d.mask & mask_offline) {
      LOG(INFO) << "Skipping directory '" << d.name << "' [OFFLINE]";
      continue;
    }
    if (!ensure_path_exists(d, dry_run)) {
      continue;
    }
    auto area = api.Open(d);
    if (!area) {
      LOG(ERROR) << "Unable to open file area: " << d;
      continue;
    }
    if (!area->FixFileHeader()) {
      LOG(ERROR) << "Error fixing file header";
    }
    if (!CheckExtendedDirAndAttributes(config, d, *area, dry_run)) {
      LOG(ERROR) <<"Failed to fix directory: " << d;
    }
  }
}

std::string FixDirectoriesCommand::GetUsage() const {
  std::ostringstream ss;
  ss<< "Usage:   fix dirs" << endl;
  ss << "Example: WWIVUTIL fix dirs" << endl;
  return ss.str();
}

bool FixDirectoriesCommand::AddSubCommands() {
  add_argument(BooleanCommandLineArgument("verbose", 'v', "Enable verbose output.", false));
  add_argument(BooleanCommandLineArgument("dry_run", 'x', "Enable dry run mode (report errors, do not fix).", false));

  return true;
}

int FixDirectoriesCommand::Execute() {
  cout << "Runnning FixDirectoriesCommand::Execute" << endl;

  CHECK(config()->config());
  checkFileAreas(*config()->config(), verbose(), dry_run());
  return 0;
}

bool FixDirectoriesCommand::verbose() const {
  return arg("verbose").as_bool();
}

bool FixDirectoriesCommand::dry_run() const {
  return arg("dry_run").as_bool();
}

} // namespace wwiv

