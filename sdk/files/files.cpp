/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
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
#include "sdk/files/files.h"

#include "dirs.h"
#include "core/datafile.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/vardec.h"
#include "sdk/files/files_ext.h"
#include <algorithm>
#include <string>
#include <utility>

using std::endl;
using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::sdk::files {

FileApi::FileApi(std::string data_directory)
: data_directory_(std::move(data_directory)) {
  clock_ = std::make_unique<SystemClock>();
};

bool FileApi::Exist(const std::string& filename) const {
  return File::Exists(::FilePath(data_directory_, StrCat(filename, ".dir")));
}

bool FileApi::Exist(const directory_t& dir) const {
  return Exist(dir.filename);
}

bool FileApi::Create(const std::string& filename) {
  if (Exist(filename)) {
    return false;
  }

  FileArea area(this, data_directory_, filename);
  // Close should save and write header if needed.
  return area.Close();
}

bool FileApi::Create(const directory_t& dir) {
  return Create(dir.filename);  
}


bool FileApi::Remove(const std::string&) {
  // TODO(rushfan): Implement me.
  return false;
}

std::unique_ptr<FileArea> FileApi::Open(const std::string& filename) {
  if (!Exist(filename)) {
    return {};
  }
  return std::make_unique<FileArea>(this, data_directory_, filename);
}

std::unique_ptr<FileArea> FileApi::Open(const directory_t& dir) {
  return Open(dir.filename);
}

std::unique_ptr<FileArea> FileApi::CreateOrOpen(const directory_t& dir) {
  auto o = Open(dir);
  if (o) {
    return o;
  }
  if (Create(dir)) {
    return Open(dir);
  }
  return {};
}

const Clock* FileApi::clock() const noexcept {
  return clock_.get();  
}

void FileApi::set_clock(std::unique_ptr<Clock> clock) {
  clock_ = std::move(clock);
}

FileAreaHeader::FileAreaHeader(const uploadsrec& u) : u_(u) {}

bool FileAreaHeader::FixHeader(const Clock& clock, uint32_t num_files) {
  // Always overwrite marker.
  to_char_array(u_.filename, "|MARKER|");
  if (u_.daten == 0) {
    const auto now = clock.Now();
    u_.daten = now.to_daten_t();
  }
  u_.numbytes = num_files;
  return true;
}

void FileAreaHeader::set_daten(daten_t d) {
  u_.daten = d;
}

daten_t FileAreaHeader::daten() const {
  return u_.daten;
}

// Delegates to other constructor
FileArea::FileArea(FileApi* api, std::string data_directory, const directory_t& dir)
    : FileArea(api, std::move(data_directory), dir.filename) {
  dir_ = dir; 
}

// Real constructor that does all of the work.
FileArea::FileArea(FileApi* api, std::string data_directory, const std::string& filename)
    : api_(api), data_directory_(std::move(data_directory)),
      base_filename_(filename), filename_(StrCat(filename, ".dir")) {
  open_ = Load();
  // Load up extended descriptions
  ext_desc();
}


// ReSharper disable once CppMemberFunctionMayBeConst
bool FileArea::FixFileHeader() {
  return header_->FixHeader(*api_->clock(), files_.empty() ? 0 : stl::size_uint32(files_) - 1);
}

FileAreaHeader& FileArea::header() const {
  return *header_;
}

bool FileArea::Load() {
  dirty_ = false;
  DataFile<uploadsrec> file(path(), File::modeReadOnly | File::modeBinary);
  if (file) {
    if (file.ReadVector(files_)) {
      open_ = true;
    }
  }
  if (files_.empty()) {
    files_.emplace_back();
    open_ = dirty_ = true;
  }
  header_ = std::make_unique<FileAreaHeader>(files_.front());
  header_->FixHeader(*api_->clock(), files_.empty() ? 0 : stl::size_uint32(files_) - 1);
  return open_;
}

bool FileArea::Close() {
  if (open_ && dirty_) {
    const auto r = Save();
    ext_desc_.reset();
    open_ = false;
    return r;
  }
  return true;
}

bool FileArea::Lock() { return false; }
bool FileArea::Unlock() { return false; }

int FileArea::number_of_files() const {
  if (!open_) {
    return 0;
  }

  if (files_.empty()) {
    return 0;
  }

  return wwiv::stl::size_int32(files_) - 1;
}

typedef std::function<bool(const uploadsrec& l, const uploadsrec& r)> UlrCompare;

std::map<FileAreaSortType, UlrCompare> CreateSortFunctions() {
  std::map<FileAreaSortType, UlrCompare> m;
  m.emplace(FileAreaSortType::DATE_ASC, [](const uploadsrec& l, const uploadsrec& r){
    return l.daten < r.daten;
  });
  m.emplace(FileAreaSortType::DATE_DESC, [](const uploadsrec& l, const uploadsrec& r){
    return r.daten < l.daten;
  });
  m.emplace(FileAreaSortType::FILENAME_ASC, [](const uploadsrec& l, const uploadsrec& r){
    return StringCompare(l.filename, r.filename) == -1;
  });
  m.emplace(FileAreaSortType::FILENAME_DESC, [](const uploadsrec& l, const uploadsrec& r){
    return StringCompare(r.filename, l.filename) == -1;
  });
  return m;
}

bool FileArea::Sort(FileAreaSortType type) {
  if (files_.size() <= 1) {
    return false;
  }

  const auto compare_funcs = CreateSortFunctions();
  // stl::at didn't work
  const auto& f = compare_funcs.at(type);
  std::sort(std::begin(files_) + 1, std::end(files_), f);
  dirty_ = true;
  return true;
}

// File specific
FileRecord FileArea::ReadFile(int num) {
  return FileRecord(wwiv::stl::at(files_, num));
}

bool FileArea::AddFile(const FileRecord& f) {
  if (files_.size() <= 1) {
    files_.push_back(f.u());
  } else {
    files_.insert(std::begin(files_) + 1, f.u());
  }
  header_->set_num_files(stl::size_uint32(files_) - 1);
  header_->set_daten(std::max(header_->daten(), f.u().daten));
  dirty_ = true;
  return true;
}

bool FileArea::AddFile(FileRecord& f, const std::string& ext_desc) {
  f.set_extended_description(!ext_desc.empty());
  if (!AddFile(f)) {
    return false;
  }
  if (ext_desc.empty()) {
    return true;
  }
  return AddExtendedDescription(f, ext_desc);
}

bool FileArea::UpdateFile(FileRecord& f, int num) {
  files_.at(num) = f.u();
  header_->set_daten(std::max(header_->daten(), f.u().daten));
  dirty_ = true;
  return true;
}

bool FileArea::UpdateFile(FileRecord& f, int num, const std::string& ext_desc) {
  f.set_extended_description(!ext_desc.empty());
  if (!UpdateFile(f, num)) {
    return false;
  }
  if (ext_desc.empty()) {
    return true;
  }
  // Delete any old description before adding new ones.
  DeleteExtendedDescription(f, num);
  return AddExtendedDescription(f, num, ext_desc);
}

bool FileArea::DeleteFile(const FileRecord& f, int file_number) {
  if (!ValidateFileNum(f, file_number)) {
    return false;
  }
  return DeleteFile(file_number);
}

bool FileArea::DeleteFile(int file_number) {
  const auto old = stl::at(files_, file_number);
  if (!stl::erase_at(files_, file_number)) {
    return false;
  }
  // Attempt to delete the extended descriptions if they existed.
  if (old.mask & mask_extended) {
    DeleteExtendedDescription(old.filename);
  }
  dirty_ = true;
  header_->set_num_files(files_.empty() ? 0 : stl::size_uint32(files_) - 1);

  return true;
}

std::optional<FileAreaExtendedDesc*> FileArea::ext_desc(bool reload) {
  if (!open_) {
    return std::nullopt;
  }
  if (!ext_desc_ || reload) {
    ext_desc_ = std::make_unique<FileAreaExtendedDesc>(
      api_, data_directory_, base_filename_, number_of_files());
  }
  return {ext_desc_.get()};
}

bool FileArea::AddExtendedDescription(FileRecord& f, int num, const std::string& text) {
  if (!ValidateFileNum(f, num)) {
    return false;
  }
  const auto r = AddExtendedDescription(f, text);
  const auto need_ext_desc_set = !f.has_extended_description();
  f.set_extended_description(true);
  if (need_ext_desc_set && num > 0) {
    UpdateFile(f, num);
  }
  return r;
}

bool FileArea::AddExtendedDescription(const std::string& file_name, const std::string& text) {
  auto o = ext_desc();
  if (!o) {
    return false;
  }
  return o.value()->AddExtended(file_name, text);
}

bool FileArea::AddExtendedDescription(const FileRecord& f, const std::string& text) {
  return AddExtendedDescription(f.aligned_filename(), text);
}

bool FileArea::AddExtendedDescription(const FileName& f, const std::string& text) {
  return AddExtendedDescription(f.aligned_filename(), text);
}

bool FileArea::DeleteExtendedDescription(FileRecord& f, int num) {
  if (!ValidateFileNum(f, num)) {
    return false;
  }
  const auto result = DeleteExtendedDescription(f.aligned_filename());
  f.set_extended_description(false);
  if (num > 0) {
    UpdateFile(f, num);
  }
  return result;
}

bool FileArea::DeleteExtendedDescription(const std::string& file_name) {
  auto o = ext_desc();
  if (!o) {
    return false;
  }
  return o.value()->DeleteExtended(file_name);
}

bool FileArea::DeleteExtendedDescription(const FileName& f) {
  return DeleteExtendedDescription(f.aligned_filename());
}

std::optional<std::string> FileArea::ReadExtendedDescriptionAsString(FileName& f) {
  return ReadExtendedDescriptionAsString(f.aligned_filename());
}

std::optional<std::string> FileArea::ReadExtendedDescriptionAsString(FileRecord& f) {
  return ReadExtendedDescriptionAsString(f.aligned_filename());
}

std::optional<std::string> FileArea::ReadExtendedDescriptionAsString(
    const std::string& aligned_name) {
  auto o = ext_desc();
  if (!o) {
    return std::nullopt;
  }
  return o.value()->ReadExtended(aligned_name);
}

const std::vector<uploadsrec>& FileArea::raw_files() const {
  return files_;
}

bool FileArea::set_raw_files(std::vector<uploadsrec> nf) {
  files_ = std::move(nf);
  return true;
}

std::optional<int> FileArea::FindFile(const FileRecord& f) {
  return FindFile(f.aligned_filename());
}

std::optional<int> FileArea::FindFile(const FileName& f) {
  return FindFile(f.aligned_filename());
}

std::optional<int> FileArea::FindFile(const std::string& file_name) {
  for (auto i = 0; i < stl::ssize(files_); i++) {
    const auto& c = stl::at(files_, i);
    if (file_name == c.filename) {
      return {i};
    }
  }
  return std::nullopt;
}

bool aligned_wildcard_match(const std::string& l, const std::string& r) {
  if (l.size() != 12 || r.size() != 12) {
    LOG(ERROR) << "Invalid params to aligned_wildcard_match: l:'" << l << "'; r:'" << r << "'";
    return false;
  }
  for (auto i = 0; i < 12; i++) {
    if (l[i] != r[i] && l[i] != '?' && r[i] != '?') {
      return false;
    }
  }
  return true;
}

std::optional<int> FileArea::SearchFile(const std::string& filemask, int start_num) {
  const auto numf = number_of_files();
  if (numf < 1 || start_num > numf) { // was >=
    return std::nullopt;
  }

  for (auto i = start_num; i < stl::ssize(files_); i++) {
    const auto& c = stl::at(files_, i);
    if (aligned_wildcard_match(filemask, c.filename)) {
      return {i};
    }
  }
  return std::nullopt;
}


bool FileArea::Save() {
  DataFile<uploadsrec> file(path(), File::modeReadWrite | File::modeCreateFile | File::modeBinary);

  if (!file) {
    return false;
  }

  // Update Header
  FixFileHeader();
  files_.at(0) = header_->u();

  const auto result = file.WriteVectorAndTruncate(files_);
  if (result) {
    dirty_ = false;
  }
  return result;
}

std::filesystem::path FileArea::path() const noexcept {
  return ::FilePath(data_directory_, filename_);
}

std::filesystem::path FileArea::ext_path() {
  const auto e = ext_desc(false);
  if (!e) {
    auto p = path();
    return p.replace_extension(".ext");
  }
  return e.value()->path();
}

bool FileArea::ValidateFileNum(const FileRecord& f, int num) {
  const auto& o = stl::at(files_, num);
  if (f.aligned_filename() != o.filename) {
    LOG(ERROR) << "Mismatched File call for " << f.aligned_filename() << " vs: " << o.filename
               << " at pos: " << num;
    return false;
  }
  return true;
}


} // namespace wwiv::sdk::files
