/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2021, WWIV Software Services             */
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

#include "sdk/vardec.h"
#include <filesystem>
#include <map>
#include <string>

namespace wwiv::sdk {

struct config_header_t {
  int written_by_wwiv_num_version;
  std::string written_by_wwiv_version;
  std::string last_written_date;
  int config_revision_number;
};

enum class newuser_item_type_t { unused, optional, required };

struct newuser_config_t {
  newuser_item_type_t use_real_name{newuser_item_type_t::required};
  newuser_item_type_t use_voice_phone{newuser_item_type_t::required};
  newuser_item_type_t use_data_phone{newuser_item_type_t::optional};
  newuser_item_type_t use_address_street{newuser_item_type_t::optional};
  newuser_item_type_t use_address_city_state{newuser_item_type_t::required};
  newuser_item_type_t use_address_zipcode{newuser_item_type_t::required};
  newuser_item_type_t use_address_country{newuser_item_type_t::required};
  newuser_item_type_t use_callsign{newuser_item_type_t::optional};
  newuser_item_type_t use_gender{newuser_item_type_t::required};
  newuser_item_type_t use_birthday{newuser_item_type_t::required};
  newuser_item_type_t use_computer_type{newuser_item_type_t::required};
  newuser_item_type_t use_email_address{newuser_item_type_t::required};
};

struct system_toggles_t {
  bool lastnet_at_logon{false};
};

struct validation_config_t {
  // Friendly name of the validation type.
  std::string name;
  // SL
  uint8_t sl; 
  // DSL
  uint8_t dsl;

  // AR
  uint16_t ar;
  // DAR
  uint16_t dar;
  // restrictions
  uint16_t restrict; 
};

struct config_t {
  config_header_t header;

  // system password
  std::string systempw;
  // path for msgs directory
  std::string msgsdir;
  // path for gfiles dir
  std::string gfilesdir;
  // path for data directory
  std::string datadir;
  // path for dloads dir
  std::string dloadsdir;
  // script directory.
  std::string scriptdir;
  // New in 5.4: Default Log Directory to use.
  std::string logdir;

  // New in 5.7 (moved from wwiv.ini): User temp directory
  std::string tempdir_format;
  // New in 5.7 (moved from wwiv.ini): User batch file transfer directory
  std::string batchdir_format;
  // New in 5.7: System scratch directory
  std::string scratchdir_format;
  // Number of instances (moved from wwiv.ini in 5.7)
  int num_instances{4};

  // BBS system name
  std::string systemname;
  // BBS system phone number
  std::string systemphone;
  // sysop's name
  std::string sysopname;

  // new user SL
  uint8_t newusersl;
  // new user DSL
  uint8_t newuserdsl;
  // Lowest SL given to a validated user.  If you want the BBS to
  // not require validation, set this to the same value as newusersl.
  uint8_t validated_sl;
  // max mail waiting
  uint8_t maxwaiting;
  // file dir new uploads go
  int newuploads;
  // if system is closed
  bool closedsystem;

  // max users on system
  uint16_t maxusers;
  // new user restrictions
  uint16_t newuser_restrict;
  // System configuration
  uint16_t sysconfig;
  // Chat time on
  uint16_t sysoplowtime;
  // Chat time off
  uint16_t sysophightime;
  // Formerly required up/down ratio. This has been moved to wwiv.ini
  float req_ratio;
  // new user gold
  float newusergold;
  // security level data
  std::map<int, slrec> sl;
  // sysop quick validation data
  std::map<int, validation_config_t> autoval;
  // user record length
  uint16_t userreclen;
  // mail waiting offset
  uint16_t waitingoffset;
  // inactive offset
  uint16_t inactoffset;
  // SysOp's WWIV 4.x Registration Number
  uint32_t wwiv_reg_number;
  // New User Password
  std::string newuserpw;
  // New user config
  newuser_config_t newuser_config;
  // Post/Call Ratio required to access transfers
  float post_call_ratio;
  // system status offset
  uint16_t sysstatusoffset;
  // offset values into user record, used by net3x.
  uint16_t fuoffset;
  uint16_t fsoffset;
  uint16_t fnoffset;

  // max subboards
  uint16_t max_subs;
  // max directories
  uint16_t max_dirs;
  // qscan pointer length in bytes
  uint16_t qscn_len;
  /** Max number of backup files to make of datafiles. 0 = unlimited */
  uint8_t max_backups;
  /** Flags that control the execution of scripts */
  uint16_t script_flags;
  // path for menu dir
  std::string menudir;

  // System toggles (on/off)
  system_toggles_t toggles;
};

class Config final {
public:
  Config(std::filesystem::path root_directory, config_t config);
  explicit Config(const Config& c);
  explicit Config(std::filesystem::path root_directory);
  ~Config();

  Config& operator=(const Config&);
  explicit Config(Config&& config);
  Config& operator=(Config&&);

  // remove unneeded operators/constructors
  Config() = delete;

  // members

  bool Load();
  bool Save();

  [[nodiscard]] bool IsInitialized() const { return initialized_; }
  void set_initialized_for_test(bool initialized) { initialized_ = initialized; }
  void set_config(const config_t& config, bool update_paths);
  [[nodiscard]] const config_t& to_config_t() const;
  void set_paths_for_test(const std::string& datadir, const std::string& msgsdir,
                          const std::string& gfilesdir, const std::string& menudir,
                          const std::string& dloadsdir, const std::string& scriptdir);

  [[nodiscard]] bool versioned_config_dat() const { return versioned_config_dat_; }
  // ReSharper disable once CppMemberFunctionMayBeStatic
  [[nodiscard]] bool is_5xx_or_later() const { return true; }
  [[nodiscard]] int written_by_wwiv_num_version() const {
    return config_.header.written_by_wwiv_num_version;
  }
  [[nodiscard]] int config_revision_number() const { return config_.header.config_revision_number; }
  void config_revision_number(int v) { config_.header.config_revision_number = v; }

  [[nodiscard]] std::string root_directory() const { return root_directory_.string(); }
  [[nodiscard]] std::string datadir() const { return datadir_; }
  void datadir(const std::string& d) { config_.datadir = d; }
  [[nodiscard]] std::string msgsdir() const { return msgsdir_; }
  void msgsdir(const std::string& d) { config_.msgsdir = d; }
  [[nodiscard]] std::string gfilesdir() const { return gfilesdir_; }
  void gfilesdir(const std::string& d) { config_.gfilesdir = d; }
  [[nodiscard]] std::string menudir() const { return menudir_; }
  void menudir(const std::string& d) { config_.menudir = d; }
  [[nodiscard]] std::string dloadsdir() const { return dloadsdir_; }
  void dloadsdir(const std::string& d) { config_.dloadsdir = d; }
  [[nodiscard]] std::string scriptdir() const { return script_dir_; }
  void scriptdir(const std::string& d) { config_.scriptdir = d; }
  [[nodiscard]] std::string logdir() const { return log_dir_; }
  void logdir(const std::string& d) { config_.logdir = d; }

  // moved from wwiv.ini
  [[nodiscard]] std::string temp_format() const { return config_.tempdir_format; }
  void temp_format(const std::string& d) { config_.tempdir_format = d; }
  [[nodiscard]] std::string batch_format() const { return config_.batchdir_format; }
  void batch_format(const std::string& d) { config_.batchdir_format = d; }
  [[nodiscard]] std::string scratch_format() const { return config_.scratchdir_format; }
  void scratch_format(const std::string& d) { config_.scratchdir_format = d; }

  /**
   * Returns the scrarch directory for a given node.
   */
  [[nodiscard]] std::filesystem::path scratch_dir(int node) const;
  
  [[nodiscard]] int num_instances() const { return config_.num_instances; }
  void num_instances(int n) { config_.num_instances = n; }

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
  [[nodiscard]] int max_waiting() const { return config_.maxwaiting; }
  void max_waiting(int d) { config_.maxwaiting = static_cast<uint8_t>(d); }
  // Directory number where uploads go by default.
  [[nodiscard]] int new_uploads_dir() const { return config_.newuploads; }

  // Sets the value for the sysconfig flags.
  // These have been moved to the ini file.
  void set_sysconfig(uint16_t sysconfig) { config_.sysconfig = sysconfig; }
  [[nodiscard]] uint16_t sysconfig_flags() const { return config_.sysconfig; }
  // QScan record length.
  [[nodiscard]] int qscn_len() const { return config_.qscn_len; }
  void qscn_len(uint16_t n) { config_.qscn_len = n; }
  // Size in bytes of the userrec structure.
  [[nodiscard]] int userrec_length() const { return config_.userreclen; }

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
  void max_users(int n) { config_.maxusers = static_cast<uint16_t>(n); }
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
  /**
   * Lowest SL given to a validated user.  If you want the BBS to
   * not require validation, set this to the same value as newusersl.
   */
  [[nodiscard]] uint8_t validated_sl() const { return config_.validated_sl; }
  void validated_sl(int n) { config_.validated_sl = static_cast<uint8_t>(n); }
  // New User Gold given when they sign up.
  [[nodiscard]] float newuser_gold() const { return config_.newusergold; }
  void newuser_gold(float f) { config_.newusergold = f; }
  // New User password (password needed to create a new user account)
  [[nodiscard]] std::string newuser_password() const { return config_.newuserpw; }

  // New user configuration
  [[nodiscard]] newuser_config_t& newuser_config() { return config_.newuser_config; }

  // System Toggles (mostly toggle settings moved from wwiv.ini)
  [[nodiscard]] system_toggles_t& toggles() { return config_.toggles; }

  void newuser_password(const std::string&);
  // New User restrictions given by default when they sign up.
  [[nodiscard]] uint16_t newuser_restrict() const { return config_.newuser_restrict; }
  void newuser_restrict(uint16_t);
  // Is this a closed system?
  [[nodiscard]] bool closed_system() const { return config_.closedsystem; }
  void closed_system(bool b) { config_.closedsystem = b; }
  // Auto validation record
  [[nodiscard]] validation_config_t auto_val(int n) const;
  void auto_val(int n, const validation_config_t& v) { config_.autoval[n] = v; }
  // Security Level information
  [[nodiscard]] const slrec& sl(int n) const;
  void sl(int n, slrec& s);
  // Registration Number
  [[nodiscard]] uint32_t wwiv_reg_number() const { return config_.wwiv_reg_number; }
  void wwiv_reg_number(int d) { config_.wwiv_reg_number = d; }
  // max number of backups of datafiles to keep.
  [[nodiscard]] int max_backups() const noexcept { return config_.max_backups; }
  void max_backups(int n) { config_.max_backups = static_cast<uint8_t>(n); }

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
  [[nodiscard]] config_header_t& header() { return config_.header; }

  /** Raw config_t. This should not be used outside of wwivconfig */
  [[nodiscard]] config_t& raw_config() { return config_; }

  // Is this config read only.
  [[nodiscard]] bool readonly() const noexcept { return readonly_; }
  // You probably shouldn't do this unless you are upgrading a dat to JSON.
  void set_readonly(bool r) { readonly_ = r; }

private:
  [[nodiscard]] std::string to_abs_path(const std::string& dir) const;
  void update_paths();

  bool initialized_{false};
  std::filesystem::path root_directory_;
  bool versioned_config_dat_{false};
  // Is this config read only (meaning it was loaded from a dat file).
  bool readonly_{true};

  std::string datadir_;
  std::string msgsdir_;
  std::string gfilesdir_;
  std::string menudir_;
  std::string dloadsdir_;
  std::string script_dir_;
  std::string log_dir_;

  config_t config_{};
};

/**
 * Helper to load the config and get the logdir from it.
 * Used by the Logger code.
 */
std::string LogDirFromConfig(const std::string& bbsdir);

/**
 * Loads a config from either JSON or DAT.
 */
std::unique_ptr<Config> load_any_config(const std::string& bbsdir);

} // namespace wwiv::sdk

#endif
