/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2017, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_CONFIG_H__
#define __INCLUDED_SDK_CONFIG_H__

#include <memory>
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {

class Config {
public:
  explicit Config(const configrec& config);
  explicit Config(const std::string& root_directory);

  bool IsInitialized() const { return initialized_; }
  void set_initialized_for_test(bool initialized) { initialized_ = initialized;  }
  const configrec* config() const { return &config_; }
  void set_config(const configrec* config, bool update_paths);
  void set_paths_for_test(const std::string& datadir, const std::string& msgsdir,
    const std::string& gfilesdir, const std::string& menudir,
    const std::string& dloadsdir, const std::string& scriptdir);

  bool versioned_config_dat() const { return versioned_config_dat_; }
  bool is_5xx_or_later() const { return written_by_wwiv_num_version_ >= 500; }
  uint16_t written_by_wwiv_num_version() const { return written_by_wwiv_num_version_; }
  uint32_t config_revision_number() const { return config_revision_number_; }

  const std::string root_directory() const { return root_directory_; }
  const std::string datadir() const { return datadir_; }
  const std::string msgsdir() const { return msgsdir_; }
  const std::string gfilesdir() const { return gfilesdir_; }
  const std::string menudir() const { return menudir_; }
  const std::string dloadsdir() const { return dloadsdir_; }
  const std::string scriptdir() const { return script_dir_; }

  const std::string system_name() const { return config_.systemname; }
  const std::string sysop_name() const { return config_.sysopname; }
  const std::string system_phone() const { return config_.systemphone; }

  const std::string config_filename() const;

  // Sets the value for required upload/download ratio. 
  // This has been moved to the ini file.
  void set_req_ratio(float req_ratio) {
    config_.req_ratio = req_ratio;
  }

  float req_ratio() const {
    return config_.req_ratio;
  }

  // Sets the value for the sysconfig flags. 
  // These hava been moved to the ini file.
  void set_sysconfig(uint16_t sysconfig) {
    config_.sysconfig = sysconfig;
  }
  uint16_t sysconfig_flags() const {
    return config_.sysconfig;

  }

private:
  std::string to_abs_path(const char* dir);
  void update_paths();

  bool initialized_ = false;
  configrec config_{};
  const std::string root_directory_;
  bool versioned_config_dat_ = false;
  uint32_t config_revision_number_ = 0;
  uint16_t written_by_wwiv_num_version_ = 0;

  std::string datadir_;
  std::string msgsdir_;
  std::string gfilesdir_;
  std::string menudir_;
  std::string dloadsdir_;
  std::string script_dir_;
};

}  // namespace sdk
}  // namespace wwiv

#endif  // __INCLUDED_SDK_CONFIG_H__
