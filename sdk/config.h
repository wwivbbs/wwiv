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
#ifndef INCLUDED_SDK_CONFIG_H
#define INCLUDED_SDK_CONFIG_H

#include "sdk/config430.h"
#include "sdk/vardec.h"
#include <filesystem>
#include <memory>

namespace wwiv::sdk {


class Config {
public:
  Config(const std::filesystem::path& root_directory, const configrec& config);
  explicit Config(const Config& c);
  explicit Config(const std::filesystem::path& root_directory);
  ~Config();

  bool Load();
  bool Save();

  [[nodiscard]] bool IsInitialized() const { return initialized_; }
  void set_initialized_for_test(bool initialized) { initialized_ = initialized; }
  //[[nodiscard]] const configrec* config() const { return &config_; }
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
  void datadir(const std::string& d) { datadir_ = d; }
  [[nodiscard]] std::string msgsdir() const { return msgsdir_; }
  void msgsdir(const std::string& d) { msgsdir_ = d; }
  [[nodiscard]] std::string gfilesdir() const { return gfilesdir_; }
  void gfilesdir(const std::string& d) { gfilesdir_ = d; }
  [[nodiscard]] std::string menudir() const { return menudir_; }
  void menudir(const std::string& d) { menudir_ = d; }
  [[nodiscard]] std::string dloadsdir() const { return dloadsdir_; }
  void dloadsdir(const std::string& d) { dloadsdir_ = d; }
  [[nodiscard]] std::string scriptdir() const { return script_dir_; }
  void scriptdir(const std::string& d) { script_dir_ = d; }
  [[nodiscard]] std::string logdir() const { return log_dir_; }
  void logdir(const std::string& d) { log_dir_ = d; }

  [[nodiscard]] std::string system_name() const { return config_.systemname; }
  void system_name(const std::string& d);
  [[nodiscard]] std::string sysop_name() const { return config_.sysopname; }
  void sysop_name(const std::string& d);
  [[nodiscard]] std::string system_phone() const { return config_.systemphone; }
  void system_phone(const std::string& d);
  [[nodiscard]] std::string system_password() const { return config_.systempw; }
  void system_password(const std::string& d);

  [[nodiscard]] std::string config_filename() const;

  // Sets the value for required upload/download ratio.
  // This has been moved to the ini file.
  void set_req_ratio(float req_ratio) { config_.req_ratio = req_ratio; }

  // Upload/Download ratio
  [[nodiscard]] float req_ratio() const { return config_.req_ratio; }
  void req_ratio(float f) { config_.req_ratio = f; }
  // Post to Call Ratio
  [[nodiscard]] float post_to_call_ratio() const { return config_.post_call_ratio; }
  void post_to_call_ratio(float f) { config_.post_call_ratio = f; }
  // Max number of emails waiting allowed
  [[nodiscard]] int max_waiting() const { return static_cast<int>(config_.maxwaiting); }
  void max_waiting(int d) { config_.maxwaiting = static_cast<uint8_t>(d); }
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
  void max_users(int n) { config_.maxusers = static_cast<uint16_t>(n);}
  // Sysop Low Time.
  [[nodiscard]] uint16_t sysop_high_time() const { return config_.sysophightime; }
  void sysop_high_time(int n) { config_.sysophightime = static_cast<uint16_t>(n); }
  // Sysop Low Time.
  [[nodiscard]] uint16_t sysop_low_time() const { return config_.sysoplowtime; }
  void sysop_low_time(int n) { config_.sysoplowtime = static_cast<uint16_t>(n); }
  // New User SL.
  [[nodiscard]] uint8_t newuser_sl() const { return config_.newusersl; }
  void newuser_sl(int n) { config_.newusersl = static_cast<uint8_t>(n); }
  // New User DSL
  [[nodiscard]] uint8_t newuser_dsl() const { return config_.newuserdsl; }
  void newuser_dsl(int n) { config_.newuserdsl = static_cast<uint8_t>(n); }
  // New User Gold given when they sign up.
  [[nodiscard]] float newuser_gold() const { return config_.newusergold; }
  void newuser_gold(float f) { config_.newusergold = f; }
  // New User password (password needed to create a new user account)
  [[nodiscard]] std::string newuser_password() const { return config_.newuserpw; }
  void newuser_password(const std::string&);
  // New User restrictions given by default when they sign up.
  [[nodiscard]] uint16_t newuser_restrict() const { return config_.newuser_restrict; }
  void newuser_restrict(uint16_t);
  // Is this a closed system?
  [[nodiscard]] bool closed_system() const { return config_.closedsystem; }
  void closed_system(bool b) { config_.closedsystem = b; }
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
  void wwiv_reg_number(int d) { config_.wwiv_reg_number = d; }
  // max number of backups of datafiles to keep.
  [[nodiscard]] int max_backups() const noexcept { return config_.max_backups; }
  void max_backups(int n) { config_.max_backups = static_cast<uint8_t>(n);}

  /** Is WWIVBasic scripting enabled */
  [[nodiscard]] bool scripting_enabled() const noexcept;
  void scripting_enabled(bool) noexcept;
  /** Is WWIVBasic package wwiv.io.file enabled */
  [[nodiscard]] bool script_package_file_enabled() const noexcept;
  void script_package_file_enabled(bool) noexcept;
  /** Is WWIVBasic package wwiv.os enabled */
  [[nodiscard]] bool script_package_os_enabled() const noexcept;
  void script_package_os_enabled(bool) noexcept;

/** Modern header */
  [[nodiscard]] configrec_header_t& header() { return config_.header.header; }

private:
  std::string to_abs_path(const char* dir) const;
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

} // namespace

#endif
