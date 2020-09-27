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
#ifndef __INCLUDED_SDK_CONFIG_H__
#define __INCLUDED_SDK_CONFIG_H__

#include <filesystem>
#include <memory>
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {

// fwd declaration
class Config;

class Config430 {
public:
  explicit Config430(const Config& config);
  explicit Config430(const configrec& config);
  explicit Config430(const std::filesystem::path& root_directory);
  ~Config430();
  [[nodiscard]] bool IsInitialized() const { return initialized_; }
  void set_initialized_for_test(bool initialized) { initialized_ = initialized; }
  void set_config(const configrec* config, bool update_paths);
  [[nodiscard]] const configrec* config() const;
  bool Load();
  bool Save();

  [[nodiscard]] bool versioned_config_dat() const { return versioned_config_dat_; }
  [[nodiscard]] bool is_5xx_or_later() const { return written_by_wwiv_num_version_ >= 500; }
  [[nodiscard]] uint16_t written_by_wwiv_num_version() const { return written_by_wwiv_num_version_; }
  [[nodiscard]] uint32_t config_revision_number() const { return config_revision_number_; }

  bool IsReadable();

private:
  void update_paths();

  bool initialized_{false};
  configrec config_{};
  const std::filesystem::path root_directory_;
  bool versioned_config_dat_{false};
  uint32_t config_revision_number_{0};
  uint16_t written_by_wwiv_num_version_{0};
}; // namespace sdk

class Config {
public:
  explicit Config(const configrec& config);
  explicit Config(const Config& c);
  explicit Config(const std::filesystem::path& root_directory);
  ~Config();

  bool Load();
  bool Save();

  [[nodiscard]] bool IsInitialized() const { return initialized_; }
  void set_initialized_for_test(bool initialized) { initialized_ = initialized; }
  [[nodiscard]] const configrec* config() const { return &config_; }
  void set_config(const configrec* config, bool update_paths);
  void set_paths_for_test(const std::string& datadir, const std::string& msgsdir,
                          const std::string& gfilesdir, const std::string& menudir,
                          const std::string& dloadsdir, const std::string& scriptdir);

  [[nodiscard]] bool versioned_config_dat() const { return versioned_config_dat_; }
  [[nodiscard]] bool is_5xx_or_later() const { return written_by_wwiv_num_version_ >= 500; }
  [[nodiscard]] uint16_t written_by_wwiv_num_version() const { return written_by_wwiv_num_version_; }
  [[nodiscard]] uint32_t config_revision_number() const { return config_revision_number_; }

  [[nodiscard]] std::string root_directory() const { return root_directory_.string(); }
  [[nodiscard]] std::string datadir() const { return datadir_; }
  [[nodiscard]] std::string msgsdir() const { return msgsdir_; }
  [[nodiscard]] std::string gfilesdir() const { return gfilesdir_; }
  [[nodiscard]] std::string menudir() const { return menudir_; }
  [[nodiscard]] std::string dloadsdir() const { return dloadsdir_; }
  [[nodiscard]] std::string scriptdir() const { return script_dir_; }
  [[nodiscard]] std::string logdir() const { return log_dir_; }

  [[nodiscard]] std::string system_name() const { return config_.systemname; }
  [[nodiscard]] std::string sysop_name() const { return config_.sysopname; }
  [[nodiscard]] std::string system_phone() const { return config_.systemphone; }
  [[nodiscard]] std::string system_password() const { return config_.systempw; }

  [[nodiscard]] std::string config_filename() const;

  // Sets the value for required upload/download ratio.
  // This has been moved to the ini file.
  void set_req_ratio(float req_ratio) { config_.req_ratio = req_ratio; }

  // Upload/Download ratio
  [[nodiscard]] float req_ratio() const { return config_.req_ratio; }
  // Post to Call Ratio
  [[nodiscard]] float post_to_call_ratio() const { return config_.post_call_ratio; }
  // Max number of emails waiting allowed
  [[nodiscard]] int max_waiting() const { return static_cast<int>(config_.maxwaiting); }
  // Directory number where uploads go by default.
  [[nodiscard]] uint8_t new_uploads_dir() const { return config_.newuploads; }

  // Sets the value for the sysconfig flags.
  // These have been moved to the ini file.
  void set_sysconfig(uint16_t sysconfig) { config_.sysconfig = sysconfig; }
  [[nodiscard]] uint16_t sysconfig_flags() const { return config_.sysconfig; }
  // QScan record length.
  [[nodiscard]] uint16_t qscn_len() const { return config_.qscn_len; }
  void qscn_len(uint16_t n) { config_.qscn_len = n; }
  // Size in bytes of the userrec structure.
  [[nodiscard]] uint16_t userrec_length() const { return config_.userreclen; }

  [[nodiscard]] uint16_t waitingoffset() const { return config_.waitingoffset; }
  [[nodiscard]] uint16_t inactoffset() const { return config_.inactoffset; }
  [[nodiscard]] uint16_t fuoffset() const { return config_.fuoffset; }
  [[nodiscard]] uint16_t fsoffset() const { return config_.fsoffset; }
  [[nodiscard]] uint16_t fnoffset() const { return config_.fnoffset; }
  [[nodiscard]] uint16_t sysstatusoffset() const { return config_.sysstatusoffset; }

  // Max directories.
  [[nodiscard]] uint16_t max_dirs() const { return config_.max_dirs; }
  void max_dirs(uint16_t n) { config_.max_dirs = n; }
  // Max Subs.
  [[nodiscard]] uint16_t max_subs() const { return config_.max_subs; }
  void max_subs(uint16_t n) { config_.max_subs = n; }
  // Max Users.
  [[nodiscard]] uint16_t max_users() const { return config_.maxusers; }
  // Sysop Low Time.
  [[nodiscard]] uint16_t sysop_high_time() const { return config_.sysophightime; }
  // Sysop Low Time.
  [[nodiscard]] uint16_t sysop_low_time() const { return config_.sysoplowtime; }
  // New User SL.
  [[nodiscard]] uint8_t newuser_sl() const { return config_.newusersl; }
  // New User DSL
  [[nodiscard]] uint8_t newuser_dsl() const { return config_.newuserdsl; }
  // New User Gold given when they sign up.
  [[nodiscard]] float newuser_gold() const { return config_.newusergold; }
  // New User password (password needed to create a new user account)
  [[nodiscard]] std::string newuser_password() const { return config_.newuserpw; }
  // New User restrictions given by default when they sign up.
  [[nodiscard]] uint16_t newuser_restrict() const { return config_.newuser_restrict; }
  // Is this a closed system?
  [[nodiscard]] bool closed_system() const { return config_.closedsystem; }
  // Auto validation record
  [[nodiscard]] const valrec& auto_val(int n) const { return config_.autoval[n]; }
  void auto_val(int n, const valrec& v) { config_.autoval[n] = v; }
  // Security Level information
  [[nodiscard]] const slrec& sl(int n) const { return config_.sl[n]; }
  void sl(int n, slrec& s) { config_.sl[n] = s; }
  // Legacy 4.x Arcs
  [[nodiscard]] const arcrec_424_t& arc(int n) const { return config_.arcs[n]; }
  void arc(int n, arcrec_424_t& a) { config_.arcs[n] = a; }
  // Registration Number
  [[nodiscard]] uint32_t wwiv_reg_number() const { return config_.wwiv_reg_number; }
  // max number of backups of datafiles to keep.
  [[nodiscard]] int max_backups() const noexcept { return config_.max_backups; }

private:
  std::string to_abs_path(const char* dir);
  void update_paths();

  bool initialized_{false};
  const std::filesystem::path root_directory_;
  bool versioned_config_dat_{false};
  uint32_t config_revision_number_{0};
  uint16_t written_by_wwiv_num_version_{0};

  std::string datadir_;
  std::string msgsdir_;
  std::string gfilesdir_;
  std::string menudir_;
  std::string dloadsdir_;
  std::string script_dir_;
  std::string log_dir_;

  std::unique_ptr<Config430> config_430_;
  configrec config_{};
};

/**
 * Helper to load the config and get the logdir from it. 
 * Used by the Logger code.
 */
std::string LogDirFromConfig(const std::string& bbsdir);

} // namespace sdk
} // namespace wwiv

#endif // __INCLUDED_SDK_CONFIG_H__
