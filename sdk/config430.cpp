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
#include "sdk/config430.h"

#include "core/datafile.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/version.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::sdk {

static const int CONFIG_DAT_SIZE_424 = 5660;

Config430::Config430(const Config430& config) : Config430(config.root_directory_) {}

Config430::Config430(const configrec& config) { set_config(&config, true); }

// Generate a WWIV 4.30 config from a 5.6+ JSON config.
Config430::Config430(const config_t& c5) {
  memset(&config_, 0, sizeof(configrec));
  auto& h = config_.header.header;
  to_char_array(h.signature, "WWIV");
  h.written_by_wwiv_num_version = static_cast<uint16_t>(c5.header.written_by_wwiv_num_version);
  h.config_revision_number = c5.header.config_revision_number;
  h.config_size = sizeof(configrec);
  // update_paths() takes care of the header

  auto& c = config_;
  to_char_array_trim(c.systempw, c5.systempw);
  to_char_array_trim(c.msgsdir, c5.msgsdir);
  to_char_array_trim(c.gfilesdir, c5.gfilesdir);
  to_char_array_trim(c.datadir, c5.datadir);
  to_char_array_trim(c.dloadsdir, c5.dloadsdir);
  to_char_array_trim(c.tempdir, "");
  to_char_array_trim(c.scriptdir, c5.scriptdir);
  to_char_array_trim(c.logdir, c5.logdir);
  to_char_array_trim(c.systemname, c5.systemname);
  to_char_array_trim(c.systemphone, c5.systemphone);
  to_char_array_trim(c.sysopname, c5.sysopname);
  to_char_array_trim(c.systemphone, c5.systemphone);

  c.newusersl = c5.newusersl;
  c.newuserdsl = c5.newuserdsl;
  c.maxwaiting = c5.maxwaiting;
  c.primaryport = 1;
  c.newuploads = static_cast<uint8_t>(c5.newuploads);
  c.closedsystem = c5.closedsystem ? 1 : 0;

  // max users on system
  c.maxusers = c5.maxusers;
  c.newuser_restrict = c5.newuser_restrict;
  c.sysconfig = c5.sysconfig;
  c.sysoplowtime = c5.sysoplowtime;
  c.sysophightime = c5.sysophightime;

  c.req_ratio = c5.req_ratio;
  c.newusergold = c5.newusergold;

  for (const auto& [pos, sl] : c5.sl) {
    c.sl[pos] = sl;
  }
  for (const auto& [pos, v] : c5.autoval) {
    c.autoval[pos] = v;
  }

  // We're skipping arcs
  // TODO(rushfan): Is this ok?
  c.userreclen = c5.userreclen;
  c.waitingoffset = c5.waitingoffset;
  c.inactoffset = c5.inactoffset;
  c.wwiv_reg_number = c5.wwiv_reg_number;
  to_char_array_trim(c.newuserpw, c5.newuserpw);
  // Post/Call Ratio required to access transfers
  c.post_call_ratio = c5.post_call_ratio;
  c.sysstatusoffset = c5.sysstatusoffset;
  c.fuoffset = c5.fuoffset;
  c.fsoffset = c5.fsoffset;
  c.fnoffset = c5.fnoffset;
  c.max_subs = c5.max_subs;
  c.max_dirs = c5.max_dirs;
  c.qscn_len = c5.qscn_len;
  c.max_backups = c5.max_backups;
  c.script_flags = c5.script_flags;
  to_char_array_trim(c.menudir, c5.menudir);
  update_paths();
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
    initialized_ = size_read == CONFIG_DAT_SIZE_424;
    written_by_wwiv_num_version_ = 424;
    config_revision_number_ = 0;
    VLOG(1) << "WWIV 4.24 CONFIG.DAT FOUND with size " << size_read << ".";
  } else {
    // We've initialized something.
    // Update absolute paths.
    update_paths();
  }
}

Config430::Config430(const std::filesystem::path& root_directory, const config_t& c5)
    : Config430(c5) {
  root_directory_ = root_directory;
}

Config430::~Config430() = default;

std::string Config430::config_filename() const {
  return FilePath(root_directory(), CONFIG_DAT).string();
}

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

config_t Config430::to_json_config(std::vector<arcrec> arcs) const {
  config_t c{};

  auto& h = c.header;
  h.config_revision_number = config_.header.header.config_revision_number;
  h.written_by_wwiv_num_version = config_.header.header.written_by_wwiv_num_version;
  h.last_written_date = DateTime::now().to_string();
  h.written_by_wwiv_version = full_version();

  c.systempw = config_.systempw;
  c.msgsdir = config_.msgsdir;

  c.gfilesdir = config_.gfilesdir;
  c.datadir = config_.datadir;
  c.dloadsdir = config_.dloadsdir;
  c.scriptdir = config_.scriptdir;
  c.logdir = config_.logdir;
  c.systemname = config_.systemname;
  c.systemphone = config_.systemphone;
  c.sysopname = config_.sysopname;

  c.newusersl = config_.newusersl;
  c.newuserdsl = config_.newuserdsl;
  c.maxwaiting = config_.maxwaiting;
  c.newuploads = config_.newuploads;
  c.closedsystem = config_.closedsystem;

  c.maxusers = config_.maxusers;
  c.newuser_restrict = config_.newuser_restrict;
  c.sysconfig = config_.sysconfig;
  c.sysoplowtime = config_.sysoplowtime;
  c.sysophightime = config_.sysophightime;
  c.req_ratio = config_.req_ratio;
  c.newusergold = config_.newusergold;
  for (auto i = 0; i <= 255; i++) {
    c.sl.emplace(i, config_.sl[i]);
  }
  for (auto i = 0; i < 10; i++) {
    c.autoval.emplace(i, config_.autoval[i]);
  }
  c.userreclen = config_.userreclen;
  c.waitingoffset = config_.waitingoffset;
  c.inactoffset = config_.inactoffset;
  c.wwiv_reg_number = config_.wwiv_reg_number;
  c.newuserpw = config_.newuserpw;
  c.post_call_ratio = config_.post_call_ratio;
  c.sysstatusoffset = config_.sysstatusoffset;
  c.fuoffset = config_.fuoffset;
  c.fsoffset = config_.fsoffset;
  c.fnoffset = config_.fnoffset;
  c.max_subs = config_.max_subs;
  c.max_dirs = config_.max_dirs;
  c.qscn_len = config_.qscn_len;
  c.max_backups = config_.max_backups;
  c.script_flags = config_.script_flags;
  c.menudir = config_.menudir;

  // Copy first four new format archiver to arcrec_424_t
  // This was the 4.24 and lower place for them.  4.31 introduced the new archive record.
  for (auto j = 0; j < 4 && j < stl::size_int(arcs); j++) {
    arcrec_424_t a{};
    auto& oa = stl::at(arcs, j);
    to_char_array(a.extension, oa.extension);
    to_char_array(a.arca, oa.arca);
    to_char_array(a.arce, oa.arce);
    to_char_array(a.arcl, oa.arcl);
  }
  return c;
}

config_t Config430::to_json_config() const {
  const std::vector<arcrec> empty_arcs;
  return to_json_config(empty_arcs);
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

// ReSharper disable once CppMemberFunctionMayBeConst
bool Config430::Save() {
  File file(FilePath(root_directory_, CONFIG_DAT));
  if (!file.Open(File::modeBinary | File::modeReadWrite | File::modeCreateFile)) {
    // We may have to create this now since config.json is source of truth
    return false;
  }
  return file.Write(&config_, sizeof(configrec)) == sizeof(configrec);
}

} // namespace wwiv::sdk
