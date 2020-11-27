/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
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
#ifndef INCLUDED_WWIVCONFIG_CONVERT_DATAFILE_H
#define INCLUDED_WWIVCONFIG_CONVERT_DATAFILE_H

#include "core/file.h"
#include "core/jsonfile.h"
#include "sdk/chains_cereal.h"
#include "sdk/subs_cereal.h"
#include "wwivconfig/convert.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/files/dirs_cereal.h"

template <typename OLDT, typename NEWT>
class ConvertJsonFile final {
public:
  ConvertJsonFile(std::filesystem::path dir, std::string file_name, std::string key,
                  int old_version, int new_version)
      : dir_(std::move(dir)), file_name_(std::move(file_name)), key_(std::move(key)),
        old_version_(old_version), new_version_(new_version) {}

  bool Convert() {
    if (const auto ov = Load()) {
      std::vector<NEWT> nv;
      for (const auto&o : ov.value()) {
        nv.emplace_back(ConvertType(o));
      }
      return Save(nv);
    }
    return false;
  }

 private:
  NEWT ConvertType(const OLDT& o);

  std::optional<std::vector<OLDT>> Load() {
    std::vector<OLDT> old;
    const auto path = wwiv::core::FilePath(dir_, file_name_);
    wwiv::core::JsonFile f(path, key_, old, old_version_);
    if (!f.Load()) {
      return std::nullopt;
    }
    if (f.loaded_version() != old_version_) {
      return std::nullopt;
    }
    return { old };
  }

  bool Save(const std::vector<NEWT>& e) {
    const auto path = wwiv::core::FilePath(dir_, file_name_);
    const auto saved_path =
        wwiv::core::FilePath(dir_, wwiv::strings::StrCat(file_name_, "v", old_version_));
    wwiv::core::File::Copy(path, saved_path);
    wwiv::core::JsonFile f(path, key_, e, new_version_);
    return f.Save();
  }

  std::filesystem::path dir_;
  std::string file_name_;
  std::string key_;
  int old_version_;
  int new_version_;
};


#endif // INCLUDED_WWIVCONFIG_CONVERT_H
