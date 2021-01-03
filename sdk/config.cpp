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
#include <utility>

#include "sdk/config.h"

#include "core/file.h"
#include "core/jsonfile.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/config_cereal.h"

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::sdk {

Config::Config(std::filesystem::path root_directory, config_t config)
    : initialized_(true), root_directory_(std::move(root_directory)),
      config_(std::move(config)) {
  update_paths();
}

Config::Config(const Config& c) : Config(c.root_directory(), c.config_) {
  // Chained constructor sets initialized to true, but we want to
  // match it from the copy.
  initialized_ = c.IsInitialized();
}

Config::~Config() = default;

Config::Config(std::filesystem::path root_directory) : root_directory_(std::move(root_directory)) {
  initialized_ = Load();
  if (!initialized_) {
    // LOG(ERROR) << CONFIG_DAT << " NOT FOUND.";
  }
}

bool Config::Load() {
  JsonFile f(FilePath(root_directory_, "config.json"), "config", config_, 1);
  if (!f.Load()) {
    return false;
  }
  // We've initialized something. Update absolute paths.
  update_paths();
  versioned_config_dat_ = true;
  return true;
}

bool Config::Save() {
  JsonFile f(FilePath(root_directory_, "config.json"), "config", config_, 1);
  return f.Save();
}

void Config::set_config(const config_t& config, bool need_to_update_paths) {
  config_ = config;

  // Update absolute paths.
  if (need_to_update_paths) {
    update_paths();
  }
}

void Config::system_name(const std::string& d) { config_.systemname = d; }

void Config::sysop_name(const std::string& d) { config_.sysopname = d; }

void Config::system_phone(const std::string& d) { config_.systemphone = d; }

void Config::system_password(const std::string& d) { config_.systempw = d; }

std::string Config::config_filename() const {
  return FilePath(root_directory(), CONFIG_DAT).string();
}

void Config::update_paths() {
  datadir_ = to_abs_path(config_.datadir);
  msgsdir_ = to_abs_path(config_.msgsdir);
  gfilesdir_ = to_abs_path(config_.gfilesdir);
  menudir_ = to_abs_path(config_.menudir);
  dloadsdir_ = to_abs_path(config_.dloadsdir);
  if (config_.scriptdir.empty()) {
    config_.scriptdir = config_.datadir;
  }
  script_dir_ = to_abs_path(config_.scriptdir);
  if (config_.logdir[0]) {
    // If the logdir is empty, leave log_dir_ empty.
    log_dir_ = to_abs_path(config_.logdir);
  }
}

void Config::set_paths_for_test(const std::string& datadir, const std::string& msgsdir,
                                const std::string& gfilesdir, const std::string& menudir,
                                const std::string& dloadsdir, const std::string& scriptdir) {
  datadir_ = datadir;
  msgsdir_ = msgsdir;
  gfilesdir_ = gfilesdir;
  menudir_ = menudir;
  dloadsdir_ = dloadsdir;
  script_dir_ = scriptdir;
}

static void set_script_flag(uint16_t& data, uint16_t flg, bool on) {
  if (on) {
    data |= flg;
  } else {
    data &= ~flg;
  }
}

void Config::newuser_password(const std::string& s) { config_.newuserpw = s; }

void Config::newuser_restrict(uint16_t d) { config_.newuser_restrict = d; }

const valrec& Config::auto_val(int n) const { return stl::at(config_.autoval, n); }

static const slrec empty_sl{};

const slrec& Config::sl(int n) const {
  if (!stl::contains(config_.sl, n)) {
    return empty_sl;
  }
  return stl::at(config_.sl, n);
}

bool Config::scripting_enabled() const noexcept {
  return !(config_.script_flags & script_flag_disable_script);
}

void Config::scripting_enabled(bool b) noexcept {
  set_script_flag(config_.script_flags, script_flag_disable_script, !b);
}

bool Config::script_package_file_enabled() const noexcept {
  return config_.script_flags & script_flag_enable_file;
}

void Config::script_package_file_enabled(bool b) noexcept {
  set_script_flag(config_.script_flags, script_flag_enable_file, b);
}

bool Config::script_package_os_enabled() const noexcept {
  return config_.script_flags & script_flag_enable_os;
}

void Config::script_package_os_enabled(bool b) noexcept {
  set_script_flag(config_.script_flags, script_flag_enable_os, b);
}

std::string Config::to_abs_path(const std::string& dir) const {
  return File::absolute(root_directory_.string(), dir).string();
}

std::string LogDirFromConfig(const std::string& bbsdir) {
  const Config config{bbsdir};
  if (!config.IsInitialized()) {
    return {};
  }
  return config.logdir();
}

} // namespace wwiv::sdk
