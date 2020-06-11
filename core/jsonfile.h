/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2016-2020, WWIV Software Services            */
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
#ifndef __INCLUDED_JSONFILE_H__
#define __INCLUDED_JSONFILE_H__

#include <string>
#include <sstream>

#include <filesystem>
#include "core/log.h"
#include "core/textfile.h"

// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/access.hpp>
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/types/set.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/types/vector.hpp>

#include <utility>

namespace wwiv::core {

template <typename T>
class JsonFile final {
public:
  JsonFile(std::filesystem::path file_name, std::string key, T& t)
    : file_name_(std::move(file_name)), key_(std::move(key)), t_(t) {
  }
  JsonFile(const JsonFile&) = delete;
  JsonFile(JsonFile&&) = delete;
  JsonFile& operator=(const JsonFile&) = delete;
  JsonFile& operator=(JsonFile&&) = delete;

  ~JsonFile() = default;

  bool Load() {
    try {
      TextFile file(file_name_, "r");
      if (!file.IsOpen()) {
        return false;
      }
      const auto text = file.ReadFileIntoString();
      if (text.empty()) {
        return false;
      }
      std::stringstream ss(text);
      cereal::JSONInputArchive load(ss);
      load(cereal::make_nvp(key_, t_));
      return true;
    } catch (const cereal::RapidJSONException& e) {
      LOG(ERROR) << "Caught cereal::RapidJSONException: " << e.what();
      return false;
    }
  }

  bool Save() {
    std::ostringstream ss;
    try {
      cereal::JSONOutputArchive save(ss);
      save(cereal::make_nvp(key_, t_));
    } catch (const cereal::RapidJSONException& e) {
      LOG(ERROR) << "Caught cereal::RapidJSONException: " << e.what();
      return false;
    }

    TextFile file(file_name_, "w");
    if (!file.IsOpen()) {
      // rapidjson will assert if the file does not exist, so we need to 
      // verify that the file exists first.
      return false;
    }

    return file.Write(ss.str());
  }

private:
  const std::filesystem::path file_name_;
  const std::string key_;
  T& t_;
};

}

#endif
