/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#include <initializer_list>
#include <map>
#include <sstream>
#include <string>

#include "core/inifile.h"
#include "core/strings.h"
#include "core/file.h"
#include "core/textfile.h"
#include "core/wwivassert.h"

using namespace wwiv::strings;
using std::map;
using std::string;

namespace wwiv {
namespace core {

namespace {
/**
* Reads a specified value from INI file data (contained in *inidata). The
* name of the value to read is contained in *value_name. If such a name
* doesn't exist in this INI file subsection, then *val is nullptr, else *val
* will be set to the string value of that value name. If *val has been set
* to something, then this function returns 1, else it returns 0.
*/
static bool StringToBoolean(const char *p) {
  if (!p) {
    return false;
  }
  char ch = wwiv::UpperCase<char>(*p);
  return (ch == 'Y' || ch == 'T' || ch == '1');
}
}  // namespace {}

static bool ParseIniFile(const string& filename, std::map<string, string>& data) {
  data.clear();
  TextFile file(filename, "rt");
  if (!file.IsOpen()) {
    return false;
  }

  string section("");
  string line;
  while (file.ReadLine(&line)) {
    StringTrim(&line);
    if (line.size() == 0) {
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

      std::size_t equals = line.find('=');
      if (equals == string::npos) {
        // not a line of the form full_key = value [; comment]
        continue;
      }
      string key = line.substr(0, equals);
      string value = line.substr(equals + 1);
      StringTrim(&key);
      StringTrim(&value);

      string real_key = section + "." + key;
      data[real_key] = value;
    }
  }
  return true;
}

IniFile::IniFile(const std::string& filename, const std::initializer_list<const char*> sections)
  : file_name_(filename), open_(false) {
  // Can't use initializer_list to go from const string -> vector<string>
  // and can't use vector<const string>
  for (const auto& s : sections) {
    sections_.emplace_back(s);
  }
  open_ = ParseIniFile(filename, data_);
}

IniFile::IniFile(const std::string& filename, const std::initializer_list<const std::string> sections)
  : file_name_(filename), open_(false) {
  // Can't use initializer_list to go from const string -> vector<string>
  // and can't use vector<const string>
  for (const auto& s : sections) {
    sections_.emplace_back(s);
  }
  open_ = ParseIniFile(filename, data_);
}

IniFile::~IniFile() { open_ = false; }

/* Close is now a NOP */
void IniFile::Close() {}

const char* IniFile::GetValue(const string& raw_key, const char *default_value)  const {
  for (const auto& section : sections_) {
    const string full_key = StrCat(section, ".", raw_key);
    {
      const auto& it = data_.find(full_key);
      if (it != data_.end()) {
        return it->second.c_str();
      }
    }
  }
  return default_value;
}

const std::string IniFile::GetStringValue(const std::string& key, const std::string& default_value) const {
  const char *s = GetValue(key);
  return (s != nullptr) ? s : default_value;
}

const bool IniFile::GetBooleanValue(const string& key, bool defaultValue)  const {
  const char *s = GetValue(key);
  return (s != nullptr) ? StringToBoolean(s) : defaultValue;
}

const long IniFile::GetNumericValueT(const string& key, long default_value) const {
  const char *s = GetValue(key);
  return (s != nullptr) ? atoi(s) : default_value;
}

template<>
const std::string IniFile::value<std::string>(const std::string& key, const std::string& default_value) const {
  return GetStringValue(key, default_value);
}

template<>
const std::string IniFile::value<std::string>(const std::string& key) const {
  return GetStringValue(key, "");
}

template<>
const bool IniFile::value<bool>(const std::string& key, const bool& default_value) const {
  return GetBooleanValue(key, default_value);
}

template<>
const bool IniFile::value<bool>(const std::string& key) const {
  return GetBooleanValue(key, false);
}


}  // namespace core
}  // namespace wwiv
