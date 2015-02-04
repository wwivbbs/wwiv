/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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

string FilePath(const string& directoryName, const string& fileName) {
  string fullPathName(directoryName);
  char last_char = directoryName.back();
  if (last_char != File::pathSeparatorChar) {
    fullPathName.push_back(File::pathSeparatorChar);
  }
  fullPathName.append(fileName);
  return fullPathName;
}

IniFile::IniFile(const string& fileName, const string& primary, const string& secondary) 
    : file_name_(fileName), open_(false), primary_(primary), secondary_(secondary) {

  TextFile file(file_name_, "rt");
  if (!file.IsOpen()) {
    open_ = false;
    return;
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

      int equals = line.find('=');
      if (equals == string::npos) {
        // not a line of the form key = value [; comment]
        continue;
      }
      string key = line.substr(0, equals);
      string value = line.substr(equals + 1);
      StringTrim(&key);
      StringTrim(&value);

      string real_key = section + "." + key;
      data_[real_key] = value;
    }
  }
  open_ = true;
}

IniFile::~IniFile() { open_ = false; }

/* Close is now a NOP */
void IniFile::Close() {}

const char* IniFile::GetValue(const string& key, const char *default_value)  const {
  const string primary_key = StrCat(primary_, ".", key);
  {
    const auto& it = data_.find(primary_key);
    if (it != data_.end()) {
      return it->second.c_str();
    }
  }
  // Not in primary, try secondary.
  const string secondary_key = secondary_ + "." + key;
  {
    const auto& it = data_.find(secondary_key);
    if (it != data_.end()) {
      return it->second.c_str();
    }
  }
  return default_value;
}

const bool IniFile::GetBooleanValue(const string& key, bool defaultValue)  const {
  const char *s = GetValue(key);
  return (s != nullptr) ? IniFile::StringToBoolean(s) : defaultValue;
}

// static
bool IniFile::StringToBoolean(const char *p) {
  if (!p) {
    return false;
  }
  char ch = wwiv::UpperCase<char>(*p);
  return (ch == 'Y' || ch == 'T' || ch == '1');
}

const long IniFile::GetNumericValueT(const string& key, long default_value) const {
  const char *s = GetValue(key);
  return (s != nullptr) ? atoi(s) : default_value;
}

}  // namespace core
}  // namespace wwiv
