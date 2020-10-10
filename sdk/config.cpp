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
#include "sdk/config.h"

#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::sdk {

static const int CONFIG_DAT_SIZE_424 = 5660;

Config::Config(const configrec& config) : initialized_(true), config_430_(std::make_unique<Config430>(config)), config_(config) {
  update_paths();
}

Config::Config(const Config& c) : Config(*c.config()) {
  // Chained constructor sets initialized to true, but we want to
  // match it from the copy.
  initialized_ = c.IsInitialized();
}

Config::~Config() = default;

Config::Config(const std::filesystem::path& root_directory)
    : root_directory_(root_directory), config_430_(std::make_unique<Config430>(root_directory)) {
  if (!config_430_->IsReadable()) {
    //LOG(ERROR) << CONFIG_DAT << " NOT FOUND.";
    return;
  }
  initialized_ = config_430_->IsInitialized();
  set_config(config_430_->config(), true);

  if (initialized_) {
    // We've initialized something. Update absolute paths.
    update_paths();
    versioned_config_dat_ = config_430_->versioned_config_dat();
    config_revision_number_ = config_430_->config_revision_number();
    written_by_wwiv_num_version_ = config_430_->written_by_wwiv_num_version();
  }
}

bool Config::Load() { 
  if (!config_430_->Load()) {
    return false;
  }
  // After we load, reset our config.
  set_config(config_430_->config(), true);

  if (initialized_) {
    // We've initialized something. Update absolute paths.
    update_paths();
    versioned_config_dat_ = config_430_->versioned_config_dat();
    config_revision_number_ = config_430_->config_revision_number();
    written_by_wwiv_num_version_ = config_430_->written_by_wwiv_num_version();
  }
  
  return true;
}


bool Config::Save() { 
  // Before we save, update the config on the 4.3x config.
  config_430_->set_config(config(), true);
  return config_430_->Save();
}

void Config::set_config(const configrec* config, bool need_to_update_paths) {
  config_ = *config;

  // Update absolute paths.
  if (need_to_update_paths) {
    update_paths();
  }
}

std::string Config::config_filename() const { return FilePath(root_directory(), CONFIG_DAT).string(); }

void Config::update_paths() {
  datadir_ = to_abs_path(config_.datadir);
  msgsdir_ = to_abs_path(config_.msgsdir);
  gfilesdir_ = to_abs_path(config_.gfilesdir);
  menudir_ = to_abs_path(config_.menudir);
  dloadsdir_ = to_abs_path(config_.dloadsdir);
  if (!config_.scriptdir[0]) {
    strcpy(config_.scriptdir, config_.datadir);
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

Config430::Config430(const Config& config) : Config430(config.root_directory()) {}

Config430::Config430(const configrec& config) { set_config(&config, true); }

bool Config430::IsReadable() {
  DataFile<configrec> configFile(FilePath(root_directory_, CONFIG_DAT),
                                 File::modeReadOnly | File::modeBinary);
  if (!configFile) {
    //LOG(ERROR) << CONFIG_DAT << " NOT FOUND.";
    return false;
  }
  return true;
}

Config430::Config430(const std::filesystem::path& root_directory)
    : root_directory_(root_directory) {
  DataFile<configrec> configFile(FilePath(root_directory, CONFIG_DAT),
                                 File::modeReadOnly | File::modeBinary);
  if (!configFile) {
    //LOG(ERROR) << CONFIG_DAT << " NOT FOUND.";
    return;
  }
  initialized_ = configFile.Read(&config_);
  // Handle 4.24 datafile
  if (!initialized_) {
    configFile.Seek(0);
    auto size_read = configFile.file().Read(&config_, CONFIG_DAT_SIZE_424);
    initialized_ = (size_read == CONFIG_DAT_SIZE_424);
    written_by_wwiv_num_version_ = 424;
    config_revision_number_ = 0;
    VLOG(1) << "WWIV 4.24 CONFIG.DAT FOUND with size " << size_read << ".";
  } else {
    // We're in a 4.3x, 5.x format.
    if (IsEquals("WWIV", config_.header.header.signature)) {
      // WWIV 5.2 style header.
      const auto& h = config_.header.header;
      versioned_config_dat_ = true;
      config_revision_number_ = h.config_revision_number;
      written_by_wwiv_num_version_ = h.written_by_wwiv_num_version;
    }
  }

  if (initialized_) {
    // We've initialized something.
    // Update absolute paths.
    update_paths();
  }
}

Config430::~Config430() = default;

std::string Config::to_abs_path(const char* dir) {
  return File::absolute(root_directory_.string(), dir).string();
}

void Config430::update_paths() {
  if (!config_.scriptdir[0]) {
    strcpy(config_.scriptdir, config_.datadir);
  }
}

void Config430::set_config(const configrec* config, bool need_to_update_paths) {
  config_ = *config;

  // Update absolute paths.
  if (need_to_update_paths) {
    update_paths();
  }
}

const configrec* Config430::config() const { return &config_; }

bool Config430::Load() {
  DataFile<configrec> configFile(FilePath(root_directory_, CONFIG_DAT),
                                 File::modeReadOnly | File::modeBinary);
  if (!configFile) {
    //LOG(ERROR) << CONFIG_DAT << " NOT FOUND.";
    return false;
  }
  if (!configFile.Read(&config_)) {
    return false;
  }
  update_paths();
  return true;
}

bool Config430::Save() {
  File file(FilePath(root_directory_, CONFIG_DAT));
  if (!file.Open(File::modeBinary | File::modeReadWrite)) {
    return false;
  }
  return file.Write(&config_, sizeof(configrec)) == sizeof(configrec);
}

std::string LogDirFromConfig(const std::string& bbsdir) { 
  Config config{bbsdir}; 
  if (!config.IsInitialized()) {
    return {};
  }
  return config.logdir();
}

} // namespace wwiv
