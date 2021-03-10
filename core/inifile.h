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
#ifndef INCLUDED_CORE_INIFILE_H
#define INCLUDED_CORE_INIFILE_H

#include <filesystem>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace wwiv::core {

struct ini_flags_type {
  const std::string strnum;
  uint32_t value;
};


class IniFile final {
public:
  IniFile(std::filesystem::path filename, std::initializer_list<const char*> sections);
  IniFile(std::filesystem::path path,
          std::initializer_list<const std::string> sections);
  // Constructor/Destructor
  ~IniFile();

  // Member functions
  void Close() noexcept;
  [[nodiscard]] bool IsOpen() const noexcept { return open_; }

  template <typename T>
  [[nodiscard]] T value(const std::string& key, const T& default_value) const {
    return static_cast<T>(GetNumericValueT(key, default_value));
  }

  template <typename T>
  [[nodiscard]] T value(const std::string& key) const {
    return static_cast<T>(GetNumericValueT(key, T()));
  }

  [[nodiscard]] std::string full_pathname() const noexcept;
  [[nodiscard]] std::filesystem::path path() const noexcept { return path_; }

template <typename T>
  [[nodiscard]] T GetFlags(const std::vector<ini_flags_type>& flag_definitions, T flags) {
  for (const auto& [key, v] : flag_definitions) {
    if (key.empty()) {
      continue;
    }
    if (const auto val = value<std::string>(key); val.empty()) {
      continue;
    }
    if (value<bool>(key)) {
      flags |= v;
    } else {
      flags &= ~v;
    }
  }
  return flags;
}

/**
 * Gets a std::vector of int from key. In the INI file the list should be
 * a comma separated list.
 */
[[nodiscard]] std::vector<int> GetIntList(const std::string& key) const;

// This class should not be assignable via '=' so remove the implicit operator=
// and Copy constructor.
IniFile(const IniFile& other) = delete;
IniFile& operator=(const IniFile& other) = delete;

private:
  [[nodiscard]] std::optional<std::string> GetValue(const std::string& key) const;

  [[nodiscard]] std::string GetStringValue(const std::string& key, const std::string& default_value) const;
  [[nodiscard]] long GetNumericValueT(const std::string& key, long default_value = 0) const;
  [[nodiscard]] bool GetBooleanValue(const std::string& key, bool default_value = false) const;


  const std::filesystem::path path_;
  bool open_{false};
  std::vector<std::string> sections_;
  std::map<std::string, std::string> data_;
};

template <>
[[nodiscard]] std::string IniFile::value<std::string>(const std::string& key,
                                                      const std::string& default_value) const;

template <>
[[nodiscard]] std::string IniFile::value<std::string>(const std::string& key) const;

template <>
[[nodiscard]] bool IniFile::value<bool>(const std::string& key, const bool& default_value) const;
template <>
[[nodiscard]] bool IniFile::value<bool>(const std::string& key) const;


} // namespace

#endif
