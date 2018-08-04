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
#include "sdk/config.h"

#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

  static const int CONFIG_DAT_SIZE_424 = 5660;

  Config::Config(const configrec& config) {
    set_config(&config, true);
  }


Config::Config(const std::string& root_directory)  : initialized_(false), root_directory_(root_directory) {
    DataFile<configrec> configFile(FilePath(root_directory, CONFIG_DAT),
                                   File::modeReadOnly | File::modeBinary);
  if (!configFile) {
    LOG(ERROR) << CONFIG_DAT << " NOT FOUND.";
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

void Config::set_config(const configrec* config, bool need_to_update_paths) {
  config_ = *config;

  // Update absolute paths.
  if (need_to_update_paths) {
    update_paths();
  }
}

const std::string Config::config_filename() const {
  return FilePath(root_directory(), CONFIG_DAT);
}

std::string Config::to_abs_path(const char* dir) {
  std::string directory = dir;
  File::absolute(root_directory_, &directory);
  return directory;
}

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
}

void Config::set_paths_for_test(const std::string& datadir, const std::string& msgsdir,
  const std::string& gfilesdir, const std::string& menudir, const std::string& dloadsdir,
  const std::string& scriptdir) {
  datadir_ = datadir; msgsdir_ = msgsdir; gfilesdir_ = gfilesdir;
  menudir_ = menudir; dloadsdir_ = dloadsdir; script_dir_ = scriptdir;
}


}
}
