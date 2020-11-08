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
#include "core/inifile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include <initializer_list>
#include <map>
#include <string>
#include <utility>

using namespace wwiv::strings;
using std::map;
using std::string;

namespace wwiv::core {

namespace {
/**
 * Reads a specified value from INI file data. The
 * name of the value to read is contained in *value_name. If such a name
 * does not exist in this INI file subsection, then *val is nullptr, else *val
 * will be set to the string value of that value name. If *val has been set
 * to something, then this function returns 1, else it returns 0.
 */
bool StringToBoolean(const char* p) {
  if (!p) {
    return false;
  }
  const auto ch = to_upper_case<char>(*p);
  return ch == 'Y' || ch == 'T' || ch == '1';
}

} // namespace {}

static bool ParseIniFile(const std::filesystem::path& filename, std::map<string, string>& data) {
  if (!File::Exists(filename)) {
    // No need to try to open a file that does not exist.
    return false;
  }

  data.clear();
  TextFile file(filename, "rt");
  if (!file.IsOpen()) {
    return false;
  }

  string section;
  string line;
  while (file.ReadLine(&line)) {
    StringTrim(&line);
    if (line.empty()) {
      continue;
    }
    if (line.front() == '[' && line.back() == ']') {
      // Section header.
      section = line.substr(1, line.size() - 2);
    } else {
      // Not a section.
      if (line.find(';') != string::npos) {
        // we have a comment, remove it.
        line.erase(line.find(';'));
      }

      const auto equals = line.find('=');
      if (equals == string::npos) {
        // not a line of the form full_key = value [; comment]
        continue;
      }
      const auto key = StringTrim(line.substr(0, equals));
      const auto value = StringTrim(line.substr(equals + 1));

      const auto real_key = StrCat(section, ".", key);
      data[real_key] = value;
    }
  }
  return true;
}

IniFile::IniFile(std::filesystem::path filename,
                 const std::initializer_list<const char*> sections)
  : path_(std::move(filename)) {
  // Can't use initializer_list to go from const string -> vector<string>
  // and can't use vector<const string>
  for (const auto& s : sections) {
    sections_.emplace_back(s);
  }
  open_ = ParseIniFile(path_, data_);
}

IniFile::IniFile(std::filesystem::path path,
                 const std::initializer_list<const std::string> sections)
  : path_(std::move(path)) {
  // Can't use initializer_list to go from const string -> vector<string>
  // and can't use vector<const string>
  for (const auto& s : sections) {
    sections_.emplace_back(s);
  }
  open_ = ParseIniFile(path_, data_);
}

IniFile::~IniFile() {
  open_ = false;
}

/* Close is now a NOP */
// ReSharper disable once CppMemberFunctionMayBeStatic
void IniFile::Close() noexcept {
}

const char* IniFile::GetValue(const string& raw_key, const char* default_value) const {
  for (const auto& section : sections_) {
    const auto full_key = StrCat(section, ".", raw_key);
    const auto& it = data_.find(full_key);
    if (it != data_.end()) {
      return it->second.c_str();
    }
  }
  return default_value;
}

std::string IniFile::
GetStringValue(const std::string& key, const std::string& default_value) const {
  const auto* s = GetValue(key);
  return s != nullptr ? s : default_value;
}

bool IniFile::GetBooleanValue(const string& key, bool default_value) const {
  const auto* s = GetValue(key);
  return s != nullptr ? StringToBoolean(s) : default_value;
}

long IniFile::GetNumericValueT(const string& key, long default_value) const {
  const auto* s = GetValue(key);
  return s != nullptr ? wwiv::strings::to_number<long>(s) : default_value;
}

std::string IniFile::full_pathname() const noexcept {
  try {
    return path_.string();
  } catch (const std::exception& e) {
    DLOG(FATAL) << "IniFile::full_pathname() threw exception: " << e.what();
  } catch (...) {
    DLOG(FATAL) << "IniFile::full_pathname() threw exception";
  }
  return {};
}

[[nodiscard]] std::vector<int> IniFile::GetIntList(const std::string& key) const {
  const auto s = GetStringValue(key, "");
  if (s.empty()) {
    return {};
  }
  auto list = SplitString(s, ",");
  std::vector<int> out;
  for (const auto& i : list) {
    out.push_back(wwiv::strings::to_number<long>(i));
  }
  return out;
} 


template <>
std::string IniFile::value<std::string>(const std::string& key,
                                        const std::string& default_value) const {
  return GetStringValue(key, default_value);
}

template <>
std::string IniFile::value<std::string>(const std::string& key) const {
  return GetStringValue(key, "");
}

template <>
bool IniFile::value<bool>(const std::string& key, const bool& default_value) const {
  return GetBooleanValue(key, default_value);
}

template <>
bool IniFile::value<bool>(const std::string& key) const {
  return GetBooleanValue(key, false);
}


} // namespace wwiv
