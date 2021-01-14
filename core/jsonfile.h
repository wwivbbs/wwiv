/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2016-2021, WWIV Software Services            */
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
#ifndef INCLUDED_CORE_JSONFILE_H
#define INCLUDED_CORE_JSONFILE_H

#include "core/cereal_utils.h"
#include "core/log.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/access.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/cereal.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/archives/json.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/types/map.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/types/memory.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/types/set.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/types/vector.hpp>


namespace wwiv::core {

std::optional<std::string> read_json_file(const std::filesystem::path& p);
int json_file_version(const std::filesystem::path& p);

struct json_version_error : public std::runtime_error {
  json_version_error(const std::string& filename, int min_ver, int actual_ver)
      : std::runtime_error(fmt::format("Invalid version for file '{}', expected: {}, actual: {}. "
                                       "Please run WWIVconfig to update it.",
                                       filename, min_ver, actual_ver)) {}
};


template <typename T>
class JsonFile final {
public:
  JsonFile(std::filesystem::path file_name, std::string key, T& t, int version = 0)
    : file_name_(std::move(file_name)), key_(std::move(key)), t_(t), version_(version) {
  }
  JsonFile(const JsonFile&) = delete;
  JsonFile(JsonFile&&) = delete;
  JsonFile& operator=(const JsonFile&) = delete;
  JsonFile& operator=(JsonFile&&) = delete;

  ~JsonFile() = default;

  bool Load() {
    try {
      if (!File::Exists(file_name_)) {
        VLOG(3) << "JSON File does not exist: " << file_name_.string();
        return false;
      }
      if (const auto o = read_json_file(file_name_)) {
        std::stringstream ss(o.value());
        cereal::JSONInputArchive ar(ss);
        SERIALIZE_NVP("version", loaded_version_);
        if (version_ > 0 && loaded_version_ < version_) {
          throw json_version_error(file_name_.string(), version_, loaded_version_);
        }
        ar(cereal::make_nvp(key_, t_));
        return true;
      }
      return false;
    } catch (const cereal::RapidJSONException& e) {
      LOG(ERROR) << "Caught cereal::RapidJSONException: " << e.what();
      return false;
    }
  }

  bool Save() {
    std::ostringstream ss;
    try {
      cereal::JSONOutputArchive ar(ss);
      if (version_ != 0) {
        SERIALIZE_NVP("version", version_);
      }
      ar(cereal::make_nvp(key_, t_));
    } catch (const cereal::RapidJSONException& e) {
      LOG(ERROR) << "Caught cereal::RapidJSONException: " << e.what();
      return false;
    }

    TextFile file(file_name_, "w");
    if (!file.IsOpen()) {
      return false;
    }

    return file.Write(ss.str());
  }

  [[nodiscard]] int loaded_version() const noexcept { return loaded_version_; }

private:
  const std::filesystem::path file_name_;
  const std::string key_;
  T& t_;
  int version_;
  int loaded_version_{0};
};

// C++17 Deduction Guide for JsonFile
template <typename X, typename Y, typename Z> JsonFile(X, Y, Z&, int) -> JsonFile<Z>;

}

#endif
