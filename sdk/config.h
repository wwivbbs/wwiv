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

#include "sdk/vardec.h"
#include <memory>

namespace wwiv {
namespace sdk {

// fwd declaration
class Config;

class Config430 {
public:
  explicit Config430(const Config& config);
  explicit Config430(const configrec& config);
  explicit Config430(const std::string& root_directory);
  bool IsInitialized() const { return initialized_; }
  void set_initialized_for_test(bool initialized) { initialized_ = initialized; }
  void set_config(const configrec* config, bool update_paths);
  const configrec* config() const;
  bool Load();
  bool Save();

  bool versioned_config_dat() const { return versioned_config_dat_; }
  bool is_5xx_or_later() const { return written_by_wwiv_num_version_ >= 500; }
  uint16_t written_by_wwiv_num_version() const { return written_by_wwiv_num_version_; }
  uint32_t config_revision_number() const { return config_revision_number_; }

  bool IsReadable();

private:
  void update_paths();

  bool initialized_{false};
  configrec config_{};
  const std::string root_directory_;
  bool versioned_config_dat_{false};
  uint32_t config_revision_number_{0};
  uint16_t written_by_wwiv_num_version_{0};
}; // namespace sdk

class Config {
public:
  explicit Config(const configrec& config);
  explicit Config(const std::string& root_directory);

  bool Load();
  bool Save();

  bool IsInitialized() const { return initialized_; }
  void set_initialized_for_test(bool initialized) { initialized_ = initialized; }
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
  const std::string system_password() const { return config_.systempw; }

  const std::string config_filename() const;

  // Sets the value for required upload/download ratio.
  // This has been moved to the ini file.
  void set_req_ratio(float req_ratio) { config_.req_ratio = req_ratio; }

  // Upload/Download ratio
  float req_ratio() const { return config_.req_ratio; }
  // Post to Call Ratio
  float post_to_call_ratio() const { return config_.post_call_ratio; }
  // Max number of emails waiting allowed
  uint8_t max_waiting() const { return config_.maxwaiting; }
  // Directory number where uploads go by default.
  uint8_t new_uploads_dir() const { return config_.newuploads; }

  // Sets the value for the sysconfig flags.
  // These hava been moved to the ini file.
  void set_sysconfig(uint16_t sysconfig) { config_.sysconfig = sysconfig; }
  uint16_t sysconfig_flags() const { return config_.sysconfig; }
  // QScan record length.
  uint16_t qscn_len() const { return config_.qscn_len; }
  void qscn_len(uint16_t n) { config_.qscn_len = n; }
  // Size in bytes of the userrec structure.
  uint16_t userrec_length() const { return config_.userreclen; }

  uint16_t waitingoffset() const { return config_.waitingoffset; }
  uint16_t inactoffset() const { return config_.inactoffset; }
  uint16_t fuoffset() const { return config_.fuoffset; }
  uint16_t fsoffset() const { return config_.fsoffset; }
  uint16_t fnoffset() const { return config_.fnoffset; }
  uint16_t sysstatusoffset() const { return config_.sysstatusoffset; }

  // Max directories.
  uint16_t max_dirs() const { return config_.max_dirs; }
  void max_dirs(uint16_t n) { config_.max_dirs = n; }
  // Max Subs.
  uint16_t max_subs() const { return config_.max_subs; }
  void max_subs(uint16_t n) { config_.max_subs = n; }
  // Max Users.
  uint16_t max_users() const { return config_.maxusers; }
  // Sysop Low Time.
  uint16_t sysop_high_time() const { return config_.sysophightime; }
  // Sysop Low Time.
  uint16_t sysop_low_time() const { return config_.sysoplowtime; }
  // New User SL.
  uint8_t newuser_sl() const { return config_.newusersl; }
  // New User DSL
  uint8_t newuser_dsl() const { return config_.newuserdsl; }
  // New User Gold given when they sign up.
  float newuser_gold() const { return config_.newusergold; }
  // New User password (password needed to create a new user account)
  std::string newuser_password() const { return config_.newuserpw; }
  // New User restrictions given by default when they sign up.
  uint16_t newuser_restrict() const { return config_.newuser_restrict; }
  // Is this a closed system?
  bool closed_system() const { return config_.closedsystem; }
  // Autoval record
  const valrec& auto_val(int n) const { return config_.autoval[n]; }
  void auto_val(int n, const valrec& v) { config_.autoval[n] = v; }
  // Security Level information
  const slrec& sl(int n) const { return config_.sl[n]; }
  void sl(int n, slrec& s) { config_.sl[n] = s; }
  // Legacy 4.x Arcs
  const arcrec_424_t& arc(int n) const { return config_.arcs[n]; }
  void arc(int n, arcrec_424_t& a) { config_.arcs[n] = a; }
  // Registration Number
  uint32_t wwiv_reg_number() const { return config_.wwiv_reg_number; }
  void set_wwiv_reg_number(uint32_t n) { config_.wwiv_reg_number = n; }

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

  Config430 config_430;
};

} // namespace sdk
} // namespace wwiv

#endif // __INCLUDED_SDK_CONFIG_H__
