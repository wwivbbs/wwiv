/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
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
#include "sdk/files/files_ext.h"

#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/files/file_record.h"
#include "sdk/files/files.h"
#include "sdk/vardec.h"
#include <algorithm>
#include <string>
#include <utility>

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::sdk::files {

FileAreaExtendedDesc::FileAreaExtendedDesc(FileApi* api,
                                           const std::filesystem::path& data_directory,
                                           const directory_t& dir, int num_files)
    : FileAreaExtendedDesc(api, data_directory, dir.filename, num_files) {
  dir_ = dir;
}

FileAreaExtendedDesc::FileAreaExtendedDesc(FileApi* api,
                                           const std::filesystem::path& data_directory,
                                           const std::string& filename, int num_files)
    : api_(api), data_directory_(data_directory), filename_(StrCat(filename, ".ext")),
      num_files_(std::max<int>(std::numeric_limits<int16_t>::max(), num_files)) {}

bool FileAreaExtendedDesc::Load() {
  Close();

  File f(path());
  if (!f.Open(File::modeReadOnly | File::modeBinary)) {
    return false;
  }
  const auto file_size = f.length();
  if (!file_size) {
    open_ = true;
    return true;
  }
  for (long file_pos = 0, count = 0; file_pos < file_size && count <= num_files_; count++) {
    f.Seek(file_pos, File::Whence::begin);
    ext_desc_type ed{};
    if (const auto num_read = f.Read(&ed, sizeof(ext_desc_type)); num_read != sizeof(ext_desc_type)) {
      // LOG ERROR? Return false here?
      break;
    }
    ext_desc_rec e{};
    strcpy(e.name, ed.name);
    e.offset = file_pos;
    ext_.emplace_back(e);

    file_pos += ed.len + static_cast<int>(sizeof(ext_desc_type));
  }
  open_ = true;
  return true;
}

bool FileAreaExtendedDesc::Save() {
  return false;
}

bool FileAreaExtendedDesc::Close() {
  ext_.clear();
  open_ = false;
  return true;
}

int FileAreaExtendedDesc::number_of_ext_descriptions() {
  if (!open_) {
    Load();
  }
  return wwiv::stl::size_int(ext_);
}

bool FileAreaExtendedDesc::AddExtended(const FileRecord& f, const std::string& text) {
  return AddExtended(f.aligned_filename(), text);
}

bool FileAreaExtendedDesc::AddExtended(const std::string& file_name, const std::string& text) {
  Close();

  ext_desc_type ed{};
  to_char_array(ed.name, file_name);
  ed.len = static_cast<int16_t>(text.size());

  {
    File file(path());
    if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      return false;
    }
    file.Seek(0L, File::Whence::end);
    file.Write(&ed, sizeof(ext_desc_type));
    file.Write(text.c_str(), ed.len);
    file.Close();
  }
  return Close();
}

bool FileAreaExtendedDesc::DeleteExtended(const FileRecord& f) {
  const auto file_name = f.aligned_filename();
  return DeleteExtended(file_name);
}

bool FileAreaExtendedDesc::DeleteExtended(const std::string& file_name) {

  ext_desc_type ed{};

  File file(path());
  if (!file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
    return false;
  }
  const auto file_size = file.length();
  File::size_type r = 0, w = 0;
  while (r < file_size) {
    file.Seek(r, File::Whence::begin);
    file.Read(&ed, sizeof(ext_desc_type));
    if (ed.len < 10000) {
      std::string ss;
      ss.resize(ed.len);
      file.Read(&ss[0], ed.len);
      if (file_name != ed.name) {
        if (r != w) {
          file.Seek(w, File::Whence::begin);
          file.Write(&ed, sizeof(ext_desc_type));
          file.Write(ss.data(), ed.len);
        }
        w += sizeof(ext_desc_type) + ed.len;
      }
    }
    r += sizeof(ext_desc_type) + ed.len;
  }
  file.Close();
  file.set_length(w);

  return Close();
}

std::optional<std::string> FileAreaExtendedDesc::ReadExtended(const FileRecord& f) {
  return ReadExtended(f.aligned_filename());
}

std::optional<std::string> FileAreaExtendedDesc::ReadExtended(const std::string& file_name) {
    if (!open_) {
    if (!Load()) {
      return std::nullopt;
    }
  }
  for (const auto& ext : ext_) {
    if (file_name != ext.name) {
      continue;
    }
    File file(path());
    if (!file.Open(File::modeBinary | File::modeReadOnly)) {
      return std::nullopt;
    }
    file.Seek(ext.offset, File::Whence::begin);
    ext_desc_type ed{};
    if (const auto num_read = file.Read(&ed, sizeof(ext_desc_type));
        num_read != sizeof(ext_desc_type) || file_name != ed.name) {
      Close();
      return std::nullopt;
    }
    std::string ss;
    ss.resize(ed.len);
    file.Read(&ss[0], ed.len);
    file.Close();

    return StringTrimEnd(ss);
  }
  return std::nullopt;
}

std::optional<std::vector<std::string>> FileAreaExtendedDesc::ReadExtendedAsLines(
    const FileRecord& f) {
  return ReadExtendedAsLines(f.aligned_filename());
}

std::optional<std::vector<std::string>> FileAreaExtendedDesc::ReadExtendedAsLines(
    const std::string& file_name) {
  auto o = ReadExtended(file_name);
  if (!o) {
    return std::nullopt;
  }
  std::vector<std::string> out;
  auto lines = SplitString(o.value(), "\r", false);
  for (auto l : lines) {
    StringTrimCRLF(&l);
    if (!l.empty()) {
      out.push_back(l);
    }
  }
  return {out};
}

std::filesystem::path FileAreaExtendedDesc::path() const noexcept {
  return ::FilePath(data_directory_, filename_);
}

} // namespace wwiv::sdk::files
