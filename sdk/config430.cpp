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


Config430::Config430(const Config& config) : Config430(config.root_directory()) {}

Config430::Config430(const configrec& config) { set_config(&config, true); }

bool Config430::IsReadable() const {
  const DataFile<configrec> configFile(FilePath(root_directory_, CONFIG_DAT),
                                       File::modeReadOnly | File::modeBinary);
  return configFile ? true : false;
}

Config430::Config430(const std::filesystem::path& root_directory)
    : root_directory_(root_directory) {
  DataFile<configrec> configFile(FilePath(root_directory, CONFIG_DAT),
                                 File::modeReadOnly | File::modeBinary);
  if (!configFile) {
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
    // We've initialized something.
    // Update absolute paths.
    update_paths();
  }
}

Config430::~Config430() = default;

void Config430::update_paths() {
  if (!config_.scriptdir[0]) {
    strcpy(config_.scriptdir, config_.datadir);
  }

  if (IsEquals("WWIV", config_.header.header.signature)) {
    // WWIV 5.2 style header.
    const auto& h = config_.header.header;
    versioned_config_dat_ = true;
    config_revision_number_ = h.config_revision_number;
    written_by_wwiv_num_version_ = h.written_by_wwiv_num_version;
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

} // namespace wwiv
