/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2020, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_CONFIG430_H
#define INCLUDED_SDK_CONFIG430_H

#include "sdk/config.h"
#include "sdk/vardec.h"
#include <filesystem>

namespace wwiv::sdk {

class Config430 {
public:
  explicit Config430(const Config430& config);
  explicit Config430(const configrec& config);
  explicit Config430(const config_t& c5);
  explicit Config430(const std::filesystem::path& root_directory);
  ~Config430();
  [[nodiscard]] bool IsInitialized() const { return initialized_; }
  void set_initialized_for_test(bool initialized) { initialized_ = initialized; }
  void set_config(const configrec* config, bool update_paths);

  /** Provides a modern JSON version of the config from here. */
  config_t to_json_config(std::vector<arcrec>);
  [[nodiscard]] const configrec* config() const;
  bool Load();
  bool Save();

  [[nodiscard]] bool versioned_config_dat() const { return versioned_config_dat_; }
  [[nodiscard]] bool is_5xx_or_later() const { return written_by_wwiv_num_version_ >= 500; }
  [[nodiscard]] uint16_t written_by_wwiv_num_version() const { return written_by_wwiv_num_version_; }
  [[nodiscard]] uint32_t config_revision_number() const { return config_revision_number_; }

  [[nodiscard]] bool IsReadable() const;

private:
  void update_paths();

  bool initialized_{false};
  configrec config_{};
  const std::filesystem::path root_directory_;
  bool versioned_config_dat_{false};
  uint32_t config_revision_number_{0};
  uint16_t written_by_wwiv_num_version_{0};
}; // namespace sdk


} // namespace

#endif
