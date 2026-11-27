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
#include "sdk/files/file_record.h"

#include "core/datetime.h"
#include "core/file.h"
#include "core/strings.h"
#include "local_io/keycodes.h"
#include "sdk/vardec.h"
#include "sdk/files/files_ext.h"
#include <string>
#include <string_view>

using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::strings;

namespace wwiv::sdk::files {

namespace {
const char* WWIV_FILE_DATE_FORMAT = "%m/%d/%y";
}

// One part of an aligned name (the name or the extension): cut or padded with
// spaces to width. After a '*' every character is '?'; in the name
// (stop_at_space), after a space every character is a space until a '*'.
static std::string align_part(std::string_view part, size_t width, bool stop_at_space) {
  std::string out(part.substr(0, width));
  out.resize(width, ' ');
  auto has_wildcard = false;
  auto has_space = false;
  for (auto& c : out) {
    if (c == '*') {
      has_wildcard = true;
    }
    if (stop_at_space && c == ' ') {
      has_space = true;
    }
    if (has_space) {
      c = ' ';
    }
    if (has_wildcard) {
      c = '?';
    }
  }
  return out;
}

// Names of any length are cut to 8.3: up to 8 characters of name (what's
// before the last '.') and 3 of extension.
std::string align(const std::string& file_name) {
  // Up to the first NUL, as when this took a C string.
  const std::string_view fn(file_name.c_str());
  if (fn.starts_with('.') || fn.find_first_of("\\/:<>|") != std::string_view::npos) {
    return "        .   ";
  }
  auto name = fn;
  std::string_view ext;
  if (const auto dot = fn.rfind('.'); dot != std::string_view::npos) {
    name = fn.substr(0, dot);
    ext = fn.substr(dot + 1);
  }
  auto out = StrCat(align_part(name, 8, true), ".", align_part(ext, 3, false));
  for (auto& c : out) {
    c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
  }
  return out;
}

std::string unalign(const std::string& file_name) {
  auto s{file_name};
  s.erase(remove(s.begin(), s.end(), ' '), s.end());
  if (s.empty()) {
    return {};
  }
  if (s.back() == '.') {
    s.pop_back();
  }
  return ToStringLowerCase(s);
}

FileName::FileName(std::string aligned_filename)
    : aligned_filename_(align(aligned_filename)), unaligned_filename_(unalign(aligned_filename_)) {}

FileName::FileName(const FileName& that)
    : aligned_filename_(that.aligned_filename()), unaligned_filename_(that.unaligned_filename()) {}

const std::string& FileName::aligned_filename() const noexcept {
  return aligned_filename_;
}

const std::string& FileName::unaligned_filename() const noexcept {
  return unaligned_filename_;
}

std::string FileName::basename() const {
  return StringTrim(aligned_filename_.substr(0, 8));
}

std::string FileName::extension() const {
  return StringTrim(aligned_filename_.substr(9));
}

//static
std::optional<FileName> FileName::FromUnaligned(const std::string& unaligned_name) {
  return {FileName(align(unaligned_name))};
}

FileRecord::FileRecord() : FileRecord(uploadsrec{}) {}

FileRecord::FileRecord(const FileRecord& that) : u_(that.u()) {}

FileRecord& FileRecord::operator=(const FileRecord& that) {
  u_ = that.u();
  return *this;
}


bool FileRecord::has_extended_description() const noexcept {
  return mask(mask_extended);
}

void FileRecord::set_extended_description(bool b) {
  set_mask(mask_extended, b);
}

void FileRecord::set_mask(int mask, bool on) {
  if (on) {
    u_.mask |= mask;
  } else {
    u_.mask &= ~mask;
  }
}

bool FileRecord::mask(int mask) const {
  return u_.mask & mask;
}

bool FileRecord::set_filename(const FileName& filename) {
  return set_filename(filename.unaligned_filename());
}

bool FileRecord::set_filename(const std::string& unaligned_filename) {
  if (unaligned_filename.size() > 12) {
    return false;
  }
  to_char_array(u_.filename, align(unaligned_filename));
  return true;
}

uint32_t FileRecord::numbytes() const noexcept {
  return u_.numbytes;
}

void FileRecord::set_numbytes(int size) {
  u_.numbytes = static_cast<daten_t>(size);
}

uint16_t FileRecord::ownerusr() const noexcept {
  return u_.ownerusr;
}

void FileRecord::set_ownerusr(int o) {
  u_.ownerusr = static_cast<uint16_t>(o);
}

uint16_t FileRecord::ownersys() const noexcept {
  return u_.ownersys;
}

void FileRecord::set_ownersys(int o) {
  u_.ownersys = static_cast<uint16_t>(o);
}

bool FileRecord::set_description(const std::string& desc) {
  auto d = desc;
  if (d.size() > 55u) {
    d.resize(55);
  }
  to_char_array(u_.description, d);
  return true;
}

bool FileRecord::set_uploaded_by(const std::string& name) {
  auto d = name;
  if (d.size() > 35u) {
    d.resize(35);
  }
  to_char_array(u_.upby, d);
  return true;
}

std::string FileRecord::uploaded_by() const {
  return u_.upby;
}

std::string FileRecord::description() const {
  return u_.description;
}

wwiv::core::DateTime FileRecord::date() const {
  return DateTime::from_daten(u_.daten);
}

std::string FileRecord::actual_date() const {
  return std::string(u_.actualdate);
}

bool FileRecord::set_date(const wwiv::core::DateTime& dt) {
  const auto d = dt.to_time_t() == 0 ? DateTime::now() : dt;
  to_char_array(u_.date, d.to_string(WWIV_FILE_DATE_FORMAT));
  u_.daten = d.to_daten_t();
  return true;
}

bool FileRecord::set_actual_date(const wwiv::core::DateTime& dt) {
  const auto d = dt.to_time_t() == 0 ? DateTime::now() : dt;
  to_char_array(u_.actualdate, d.to_string(WWIV_FILE_DATE_FORMAT));
  return true;
}

FileName FileRecord::filename() const {
  return FileName(u_.filename);
}

std::string FileRecord::aligned_filename() const {
  return align(u_.filename);
}

std::string FileRecord::unaligned_filename() const {
  return unalign(u_.filename);
}

std::filesystem::path FilePath(const std::filesystem::path& directory_name,
                                   const FileRecord& f) {
  if (directory_name.empty()) {
    return f.unaligned_filename();
  }
  return directory_name / f.unaligned_filename();
}

std::filesystem::path FilePath(const std::filesystem::path& directory_name,
                                   const FileName& f) {
  if (directory_name.empty()) {
    return f.unaligned_filename();
  }
  return directory_name / f.unaligned_filename();
}

std::ostream& operator<<(std::ostream& os, const FileName& f) {
  os << f.unaligned_filename();
  return os;
}

std::ostream& operator<<(std::ostream& os, const FileRecord& f) {
  os << f.unaligned_filename();
  return os;
}

} // namespace wwiv::sdk::files
