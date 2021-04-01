/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
/*                                                                        */
/**************************************************************************/

#include "bbs/bbs.h"
#include "bbs/bbs_event_handlers.h"
#include "bbs/conf.h"
#include "bbs/connect1.h"
#include "bbs/instmsg.h"
#include "bbs/interpret.h"
#include "bbs/netsup.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xinitini.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/output.h"
#include "common/pause.h"
#include "common/workspace.h"
#include "core/datafile.h"
#include "core/eventbus.h"
#include "core/inifile.h"
#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/version.h"
#include "fmt/printf.h"
#include "local_io/local_io.h"
#include "local_io/wconstants.h"
#include "sdk/arword.h"
#include "sdk/chains.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/gfiles.h"
#include "sdk/instance.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/files/files.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/net/networks.h"
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::seconds;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::os;
using namespace wwiv::strings;
using namespace wwiv::sdk;

void StatusManagerCallback(int i) {
  switch (i) {
  case Status::file_change_names: {
    // re-read names.lst
    if (a()->names()) {
      // We may not have the BBS initialized yet, so only
      // re-read the names file if it's changed from another node.
      a()->names()->Load();
    }
  } break;
  case Status::file_change_upload:
    break;
  case Status::file_change_posts:
    a()->subchg = 1;
    break;
  case Status::file_change_email:
    a()->emchg_ = true;
    break;
  case Status::file_change_net: {
    set_net_num(a()->net_num());
  } break;
  default: // NOP
    break;
  }
}

// Turns a string into a bit mapped unsigned short flag for use with ExecuteExternalProgram.
static uint16_t str2spawnopt(const std::string& s) {
  auto return_val = EFLAG_NONE;
  const auto ts = ToStringUpperCase(s);

  if (ts.find("NOHUP") != std::string::npos) {
    return_val |= EFLAG_NOHUP;
  }
  if (ts.find("COMIO") != std::string::npos) {
    return_val |= EFLAG_COMIO;
  }
  if (ts.find("BINARY") != std::string::npos) {
    return_val |= EFLAG_BINARY;
  }
  if (ts.find("FOSSIL") != std::string::npos) {
    return_val |= EFLAG_SYNC_FOSSIL;
  }
  if (ts.find("NETFOSS") != std::string::npos) {
    return_val |= EFLAG_NETFOSS;
  }
  if (ts.find("NETPROG") != std::string::npos) {
    return_val |= EFLAG_NETPROG;
  }
  if (ts.find("STDIO") != std::string::npos) {
    return_val |= EFLAG_STDIO;
  }
  if (ts.find("NOSCRIPT") != std::string::npos) {
    return_val |= EFLAG_NOSCRIPT;
  }
  if (ts.find("NO_CHANGE_DIR") != std::string::npos) {
    return_val |= EFLAG_NO_CHANGE_DIR;
  }
  if (ts.find("TEMP_DIR") != std::string::npos) {
    return_val |= EFLAG_TEMP_DIR;
  }
  if (ts.find("BATCH_DIR") != std::string::npos) {
    return_val |= EFLAG_BATCH_DIR;
  }
  if (ts.find("QWK_DIR") != std::string::npos) {
    return_val |= EFLAG_QWK_DIR;
  }
  return return_val;
}

// Takes string s and creates restrict val
static uint16_t str2restrict(const std::string& s) {
  const auto s1 = ToStringUpperCase(s);
  uint16_t r = 0;
  for (auto i = ssize(restrict_string) - 1; i >= 0; i--) {
    if (s1.find(restrict_string[i]) != std::string::npos) {
      r |= 1 << i;
    }
  }

  return r;
}

// Reads WWIV.INI info from [WWIV] subsection, overrides some config.dat
// settings (as appropriate), updates config.dat with those values. Also
// tries to read settings from [WWIV-<instnum>] subsection - this overrides
// those in [WWIV] subsection.

// See #defines SPAWNOPT_XXXX in vardec.h for these.
static std::map<std::string, uint16_t> eventinfo = {
    {"NEWUSER", EFLAG_NONE},     {"BEGINDAY", EFLAG_NONE},
    {"LOGON", EFLAG_NONE},       {"ULCHK", EFLAG_NOHUP},
    {"PROT_SINGLE", EFLAG_NONE}, {"PROT_BATCH", EFLAG_BATCH_DIR},
    {"ARCH_E", EFLAG_NONE},      {"ARCH_L", EFLAG_NONE},      {"ARCH_A", EFLAG_NONE},
    {"ARCH_D", EFLAG_NONE},      {"ARCH_K", EFLAG_NONE},      {"ARCH_T", EFLAG_NONE},
    {"NET_CMD1", EFLAG_NETPROG}, {"NET_CMD2", EFLAG_NETPROG}, {"LOGOFF", EFLAG_NONE},
    {"CLEANUP", EFLAG_NONE},
};

// TODO(rushfan): If we need this elsewhere add it into IniFile
static std::string to_array_key(const std::string& n, const std::string& index) {
  return StrCat(n, "[", index, "]");
}

#define INI_GET_ASV(s, f, func, d)                                                                 \
  do {                                                                                                \
    const auto ss = ini.value<std::string>(to_array_key(INI_STR_SIMPLE_ASV, s));                   \
    if (!ss.empty()) {                                                                             \
      (f) = func(ss);                                                                                \
    } else {                                                                                       \
      (f) = d;                                                                                       \
    }                                                                                              \
  } while(0)

static std::vector<ini_flags_type> sysinfo_flags = {
    {INI_STR_FORCE_FBACK, OP_FLAGS_FORCE_NEWUSER_FEEDBACK},
    //{INI_STR_CHECK_DUP_PHONES, OP_FLAGS_CHECK_DUPE_PHONENUM},
    //{INI_STR_HANGUP_DUP_PHONES, OP_FLAGS_HANGUP_DUPE_PHONENUM},
    {INI_STR_USE_SIMPLE_ASV, OP_FLAGS_SIMPLE_ASV},
    {INI_STR_POSTTIME_COMPENS, OP_FLAGS_POSTTIME_COMPENSATE},
    {INI_STR_IDZ_DESC, OP_FLAGS_IDZ_DESC},
//    {INI_STR_SETLDATE, OP_FLAGS_SETLDATE},
    {INI_STR_READ_CD_IDZ, OP_FLAGS_READ_CD_IDZ},
    {INI_STR_FSED_EXT_DESC, OP_FLAGS_FSED_EXT_DESC},
//    {INI_STR_FAST_TAG_RELIST, OP_FLAGS_FAST_TAG_RELIST},
    {INI_STR_MAIL_PROMPT, OP_FLAGS_MAIL_PROMPT},
    {INI_STR_SHOW_CITY_ST, OP_FLAGS_SHOW_CITY_ST},
//    {INI_STR_WFC_SCREEN, OP_FLAGS_WFC_SCREEN},
    {INI_STR_MSG_TAG, OP_FLAGS_MSG_TAG},
//    {INI_STR_CHAIN_REG, OP_FLAGS_CHAIN_REG},
    {INI_STR_CAN_SAVE_SSM, OP_FLAGS_CAN_SAVE_SSM},
    {INI_STR_USE_FORCE_SCAN, OP_FLAGS_USE_FORCESCAN},
    //{INI_STR_NEWUSER_MIN, OP_FLAGS_NEWUSER_MIN},
};

static std::vector<ini_flags_type> sysconfig_flags = {
    {INI_STR_2WAY_CHAT, sysconfig_2_way},
    {INI_STR_NO_NEWUSER_FEEDBACK, sysconfig_no_newuser_feedback},
    {INI_STR_TITLEBAR, sysconfig_titlebar},
    {INI_STR_LOG_DOWNLOADS, sysconfig_log_dl},
    //{INI_STR_CLOSE_XFER, sysconfig_no_xfer},
    {INI_STR_ALL_UL_TO_SYSOP, sysconfig_all_sysop},
    {INI_STR_ALLOW_ALIASES, sysconfig_allow_alias},
    //{INI_STR_EXTENDED_USERINFO, sysconfig_extended_info},
    {INI_STR_FREE_PHONE, sysconfig_free_phone}};

void Application::ReadINIFile(IniFile& ini) {
  // Setup default  data
  chatname_color_ = 95;
  message_color_ = 2;
  max_batch = 50;
  max_extend_lines = 10;
  max_chains = 50;
  max_gfilesec = 32;

  // Found something, pull out event flags.
  for (const auto& kv : eventinfo) {
    spawn_opts_[kv.first] = kv.second;
    const auto key_name = to_array_key(INI_STR_SPAWNOPT, kv.first);
    const auto ss = ini.value<std::string>(key_name);
    if (!ss.empty()) {
      spawn_opts_[kv.first] = str2spawnopt(ss);
    }
  }

  // pull out new user colors
  for (int i = 0; i < 10; i++) {
    auto index = std::to_string(i);
    auto num = ini.value<uint8_t>(to_array_key(INI_STR_NUCOLOR, index));
    if (num != 0) {
      newuser_colors[i] = num;
    }
    num = ini.value<uint8_t>(to_array_key(INI_STR_NUCOLORBW, index));
    if (num != 0) {
      newuser_bwcolors[i] = num;
    }
  }

  SetCarbonCopyEnabled(ini.value<bool>("ALLOW_CC_BCC"));

  // pull out sysop-side colors
  bout.localIO()->SetTopScreenColor(ini.value<uint8_t>(INI_STR_TOPCOLOR, bout.localIO()->GetTopScreenColor()));
  bout.localIO()->SetUserEditorColor(ini.value<uint8_t>(INI_STR_F1COLOR, bout.localIO()->GetUserEditorColor()));
  bout.localIO()->SetEditLineColor(ini.value<uint8_t>(INI_STR_EDITLINECOLOR, bout.localIO()->GetEditLineColor()));
  chatname_color_ = ini.value<int>(INI_STR_CHATSELCOLOR, GetChatNameSelectionColor());

  // pull out sizing options
  max_batch = ini.value<uint16_t>(INI_STR_MAX_BATCH, max_batch);
  max_extend_lines = ini.value<uint16_t>(INI_STR_MAX_EXTEND_LINES, max_extend_lines);
  max_chains = ini.value<uint16_t>(INI_STR_MAX_CHAINS, max_chains);
  max_gfilesec = ini.value<uint16_t>(INI_STR_MAX_GFILESEC, max_gfilesec);

  // pull out strings
  upload_cmd = ini.value<std::string>(INI_STR_UPLOAD_CMD);
  beginday_cmd = ini.value<std::string>(INI_STR_BEGINDAY_CMD);
  newuser_cmd = ini.value<std::string>(INI_STR_NEWUSER_CMD);
  logon_cmd = ini.value<std::string>(INI_STR_LOGON_CMD);
  logoff_cmd = ini.value<std::string>(INI_STR_LOGOFF_CMD);
  cleanup_cmd = ini.value<std::string>(INI_STR_CLEANUP_CMD);
  terminal_command = ini.value<std::string>(INI_STR_TERMINAL_CMD);

  forced_read_subnum_ = ini.value<uint16_t>(INI_STR_FORCE_SCAN_SUBNUM, forced_read_subnum_);
  internal_zmodem_ = ini.value<bool>(INI_STR_INTERNALZMODEM, true);
  internal_fsed_ = ini.value<bool>(INI_STR_INTERNAL_FSED, true);
  newscan_at_login_ = ini.value<bool>(INI_STR_NEW_SCAN_AT_LOGIN, true);
  exec_log_syncfoss_ = ini.value<bool>(INI_STR_EXEC_LOG_SYNCFOSS, false);
  exec_child_process_wait_time_ = ini.value<int>(INI_STR_EXEC_CHILD_WAIT_TIME, 500);
  beginday_node_number_ = ini.value<int>(INI_STR_BEGINDAYNODENUMBER, 1);

  // pull out sysinfo_flags
  flags_ = ini.GetFlags(sysinfo_flags, flags_);

  // allow override of Application::message_color_
  message_color_ = ini.value<int>(INI_STR_MSG_COLOR, GetMessageColor());

  // get asv values
  if (HasConfigFlag(OP_FLAGS_SIMPLE_ASV)) {
    INI_GET_ASV("SL", asv.sl, to_number<uint8_t>, asv.sl);
    INI_GET_ASV("DSL", asv.dsl, to_number<uint8_t>, asv.dsl);
    INI_GET_ASV("EXEMPT", asv.exempt, to_number<uint8_t>, asv.exempt);
    INI_GET_ASV("AR", asv.ar, str_to_arword, asv.ar);
    INI_GET_ASV("DAR", asv.dar, str_to_arword, asv.dar);
    INI_GET_ASV("RESTRICT", asv.restrict, str2restrict, asv.restrict);
  }

  // sysconfig flags
  config()->set_sysconfig(ini.GetFlags(sysconfig_flags, config()->sysconfig_flags()));

  // Sets up the default attach directory.
  const auto attach_dir = ini.value<std::string>(INI_STR_ATTACH_DIR);
  attach_dir_ = !attach_dir.empty() ? attach_dir : FilePath(bbspath(), "attach").string();
  attach_dir_ = File::EnsureTrailingSlash(attach_dir_);

  // Sets up the default net fossil directory.
  const std::filesystem::path netfoss_dir = ini.value<std::string>(INI_STR_NETFOSS_DIR);
  netfoss_dir_ = !netfoss_dir.empty() ? netfoss_dir : FilePath(bbspath(), "netfoss");
  netfoss_dir_ = File::EnsureTrailingSlash(netfoss_dir_);

  screen_saver_time = ini.value<uint16_t>("SCREEN_SAVER_TIME", screen_saver_time);

  max_extend_lines = std::min<uint16_t>(max_extend_lines, 99);
  max_batch = std::min<uint16_t>(max_batch, 999);
  max_chains = std::min<uint16_t>(max_chains, 999);
  max_gfilesec = std::min<uint16_t>(max_gfilesec, 999);

  full_screen_read_prompt_ = ini.value<bool>("FULL_SCREEN_READER", true);
  bin.set_logon_key_timeout(seconds(std::max<int>(10, ini.value<int>("LOGON_KEY_TIMEOUT", 30))));
  bin.set_default_key_timeout(seconds(std::max<int>(30, ini.value<int>("USER_KEY_TIMEOUT", 180))));
  bin.set_sysop_key_timeout(seconds(std::max<int>(30, ini.value<int>("SYSOP_KEY_TIMEOUT", 600))));

  // Set the system wide BPS.
  const auto system_bps = ini.value<int>("SYSTEM_BPS", 0);
  sess().set_system_bps(system_bps);
}

bool Application::ReadInstanceSettings(int instance_number) {
  auto temp_directory = config_->temp_format();
  if (temp_directory.empty()) {
    temp_directory = "e/%n/temp";
  }
  auto batch_directory = config_->batch_format();
  if (batch_directory.empty()) {
    batch_directory = temp_directory;
  }
  auto scratch_directory = config_->scratch_format();
  if (scratch_directory.empty()) {
    scratch_directory = temp_directory;
  }
  temp_directory = File::FixPathSeparators(temp_directory);
  batch_directory = File::FixPathSeparators(batch_directory);
  scratch_directory = File::FixPathSeparators(scratch_directory);

  // Replace %n with instance number value.
  const auto instance_num_string = std::to_string(instance_number);
  StringReplace(&temp_directory, "%n", instance_num_string);
  StringReplace(&batch_directory, "%n", instance_num_string);
  StringReplace(&scratch_directory, "%n", instance_num_string);
  
  // Set the directories (temp, batch, language)
  const auto temp = File::EnsureTrailingSlash(File::absolute(bbspath(), temp_directory));
  const auto batch = File::EnsureTrailingSlash(File::absolute(bbspath(), batch_directory));
  const auto scratch = File::EnsureTrailingSlash(File::absolute(bbspath(), scratch_directory));

  Dirs d(temp, batch, batch, config()->gfilesdir(), scratch);
  sess().dirs(d);

  // Set config for macro processing.
  bbs_macro_context_->set_config(config());

  if (instance_number > config_->num_instances()) {
    LOG(ERROR) << "Not enough instances configured in wwivconfig. Currently: " << config_->num_instances();
    return false;
  }
  return true;
}

bool Application::ReadConfig() {
  config_ = std::make_unique<Config>(bbspath());
  if (!config_->IsInitialized()) {
    LOG(ERROR) << config_->config_filename() << " NOT FOUND.";
    return false;
  }

  if (!config_->versioned_config_dat()) {
    const auto msg = fmt::format(
        "Please run WWIVconfig to upgrade {} to the most recent version.", config_->config_filename());
    std::cerr << msg << std::endl;
    LOG(ERROR) << msg;
    sleep_for(seconds(2));
    return false;
  }

  // initialize the user manager
  user_manager_ = std::make_unique<UserManager>(*config_);
  statusMgr = std::make_unique<StatusMgr>(config_->datadir(), StatusManagerCallback);

  IniFile ini(FilePath(bbspath(), WWIV_INI), {StrCat("WWIV-", sess().instance_number()), INI_TAG});
  if (!ini.IsOpen()) {
    LOG(ERROR) << "Unable to read WWIV.INI.";
    return false;
  }
  ReadINIFile(ini);
  if (!ReadInstanceSettings(sess().instance_number())) {
    return false;
  }

  return true;
}

void Application::read_nextern() {
  externs.clear();
  if (auto externalFile = DataFile<newexternalrec>(FilePath(config()->datadir(), NEXTERN_DAT))) {
    externalFile.ReadVector(externs, 15);
  }
}

void Application::read_arcs() {
  arcs.clear();
  if (auto file = DataFile<arcrec>(FilePath(config()->datadir(), ARCHIVER_DAT))) {
    file.ReadVector(arcs, MAX_ARCS);
  }
}

void Application::read_editors() {
  editors.clear();
  if (auto file = DataFile<editorrec>(FilePath(config()->datadir(), EDITORS_DAT))) {
    file.ReadVector(editors, 10);
  }
}

void Application::read_nintern() {
  over_intern.clear();
  if (auto file = DataFile<newexternalrec>(FilePath(config()->datadir(), NINTERN_DAT))) {
    file.ReadVector(over_intern, 3);
  }
}

bool Application::read_subs() {
  subs_ = std::make_unique<Subs>(config_->datadir(), nets_->networks(), config_->max_backups());
  return subs_->Load();
}

class BBSLastReadImpl final : public msgapi::WWIVLastReadImpl {
  [[nodiscard]] uint32_t last_read(int area) const override { return a()->sess().qsc_p[area]; }

  void set_last_read(int area, uint32_t last_read) override {
    if (area >= 0) {
      a()->sess().qsc_p[area] = last_read;
    }
  }

  void Load() override {
    // Handled by the BBS in read_qscn(usernum, qsc, false);
  }

  void Save() override {
    // Handled by the BBS in write_qscn(usernum, qsc, false);
  }
};

bool Application::create_message_api() {
  // TODO(rushfan): Create the right API type for the right message area.

  msgapi::MessageApiOptions options;
  // Delete ONE matches classic WWIV behavior.
  options.overflow_strategy = msgapi::OverflowStrategy::delete_one;

  // We only support type-2
  msgapis_[2] = std::make_unique<msgapi::WWIVMessageApi>(
      options, *config_, nets_->networks(), new BBSLastReadImpl());

  fileapi_ = std::make_unique<files::FileApi>(config_->datadir());
  return true;
}

seconds Application::extratimecall() const {
  return duration_cast<seconds>(extratimecall_);
}

seconds Application::set_extratimecall(duration<double> et) {
  extratimecall_ = et;
  return duration_cast<seconds>(extratimecall_);
}

seconds Application::add_extratimecall(duration<double> et) {
  extratimecall_ += et;
  return duration_cast<seconds>(extratimecall_);
}

seconds Application::subtract_extratimecall(duration<double> et) {
  extratimecall_ -= et;
  return duration_cast<seconds>(extratimecall_);
}

void Application::read_networks() {
  nets_ = std::make_unique<Networks>(*config());
}

bool Application::read_names() {
  // Load the SDK Names class too.
  names_.reset(new Names(*config_));
  return true;
}

bool Application::read_dirs() {
  dirs_ = std::make_unique<wwiv::sdk::files::Dirs>(config_->datadir(), config_->max_backups());
  return dirs_->Load();
}

void Application::read_chains() {
  chains = std::make_unique<Chains>(*config());
  if (chains->IsInitialized()) {
    chains->Save();
  }
}

void Application::read_gfile() {
  gfiles_ = std::make_unique<GFiles>(config()->datadir(), config()->max_backups());
  gfiles_->Load();
}

static bool mkdir_or_warn(const std::filesystem::path& dir, const std::string& name) {
  if (!File::Exists(dir)) {
    if (!File::mkdirs(dir)) {
      LOG(ERROR) << "Your " << name << " dir isn't valid.";
      LOG(ERROR) << "It is now set to: '" << dir.string() << "'";
      return false;
    }
  }
  return true;
}

bool Application::InitializeBBS(bool cleanup_network) {
  Cls();
  std::clog << std::endl
            << full_version() << ", Copyright (c) 1998-2021, WWIV Software Services."
            << std::endl
            << std::endl
            << "\r\nInitializing BBS..." << std::endl;

  instances_ = std::make_unique<Instances>(*config());
  use_workspace = false;

  bin.clearnsp();

  // Set dirs in the session context first.
  mkdir_or_warn(sess().dirs().temp_directory(), "temp");
  mkdir_or_warn(sess().dirs().batch_directory(), "batch");
  mkdir_or_warn(sess().dirs().scratch_directory(), "scratch");
  mkdir_or_warn(sess().dirs().current_menu_gfiles_directory(), "menus/gfiles");
  mkdir_or_warn(sess().dirs().current_menu_script_directory(), "menus/scripts");
  // Note that the sess().dirs().current_menu_directory() won't be set till the user
  // picks a menuset.
  mkdir_or_warn(config_->menudir(), "menus");
  write_inst(INST_LOC_INIT, 0, INST_FLAGS_NONE);

  // make sure it is the new USERREC structure
  VLOG(1) << "Reading user scan pointers.";
  if (const auto qs_fn = FilePath(config()->datadir(), USER_QSC); !File::Exists(qs_fn)) {
    LOG(ERROR) << "Could not open file '" << qs_fn << "'";
    LOG(ERROR) << "You must go into WWIVconfig and convert your userlist before running the BBS.";
    return false;
  }

  read_networks();
  if (!create_message_api()) {
    return false;
  }

  VLOG(1) << "Reading status information.";
  a()->status_manager()->Run([&](Status& status) {
    status.status_wwiv_version(wwiv_config_version());
    status.ensure_callernum_valid();
  });

  VLOG(1) << "Reading Gfiles.";
  read_gfile();

  VLOG(1) << "Reading user names.";
  if (!read_names()) {
    return false;
  }

  VLOG(1) << "Reading Message Areas.";
  if (!read_subs()) {
    return false;
  }

  VLOG(1) << "Reading File Areas.";
  if (!read_dirs()) {
    return false;
  }

  VLOG(1) << "Reading Chains.";
  read_chains();

  VLOG(1) << "Reading File Transfer Protocols.";
  read_nextern();
  read_nintern();

  VLOG(1) << "Reading File Archivers.";
  read_arcs();

  VLOG(1) << "Reading Full Screen Message Editors.";
  read_editors();

  if (!File::mkdirs(attach_dir_)) {
    LOG(ERROR) << "Your file attachment directory is invalid.";
    LOG(ERROR) << "It is now set to: " << attach_dir_ << "'";
    return false;
  }
  File::set_current_directory(bbs_dir_);

  check_phonenum(); // dupphone addition

  VLOG(1) << "Reading User Information.";
  ReadCurrentUser(1);

  statusMgr->reload_status();
  bout.localIO()->topdata(LocalIO::topdata_t::user);

  // Set DSZLOG
  dsz_logfile_name_ = FilePath(sess().dirs().temp_directory(), "dsz.log").string();
  if (environment_variable("DSZLOG").empty()) {
    set_environment_variable("DSZLOG", dsz_logfile_name_);
  }
  // SET BBS environment variable.
  set_environment_variable("BBS", full_version());
  sess().InitalizeContext(*config());

  network_extension_ = ".net";
  if (const auto wwiv_instance = environment_variable("WWIV_INSTANCE"); !wwiv_instance.empty()) {
    if (const auto inst_num = to_number<int>(wwiv_instance); inst_num > 0) {
      network_extension_ = fmt::sprintf(".%3.3d", inst_num);
      // Fix... Set the global instance variable to match this.  When you run WWIV with the
      // -n<instance> parameter it sets the WWIV_INSTANCE environment variable, however it wasn't
      // doing the reverse.
      sess().instance_number(inst_num);
    }
  }

  frequent_init();
  VLOG(1) << "Reading Conferences.";
  all_confs_ = std::make_unique<Conferences>(
    config()->datadir(), *subs_, *dirs_, config()->max_backups());
  if (!all_confs_->Load()) {
    LOG(ERROR) << "Error Loading Conferences";
  }

  TempDisablePause disable_pause(bout);
  const auto t = sess().dirs().temp_directory();
  remove_from_temp("*.*", sess().dirs().temp_directory(), false);
  remove_from_temp("*.*", sess().dirs().batch_directory(), false);
  remove_from_temp("*.*", sess().dirs().qwk_directory(), false);
  remove_from_temp("*.*", sess().dirs().scratch_directory(), false);

  if (cleanup_network) {
    cleanup_net();
    sysoplog(false) << "";
    sysoplog(false) << "WWIV " << full_version() << ", inst " << sess().instance_number()
                    << ", brought up at " << times() << " on " << fulldate() << ".";
  }

  catsl();
  VLOG(1) << "Saving Instance information.";
  write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);

  wwiv::bbs::bbs_callbacks();

  return true;
}

// begin dupphone additions

void Application::check_phonenum() {
  if (!File::Exists(FilePath(config()->datadir(), PHONENUM_DAT))) {
    create_phone_file();
  }
}

// TODO(rushfan): maybe move this to SDK, but pass in a vector of numbers.
void Application::create_phone_file() {
  phonerec p{};

  File file(FilePath(config()->datadir(), USER_LST));
  if (!file.Open(File::modeReadOnly | File::modeBinary)) {
    return;
  }
  const auto file_size = file.length();
  file.Close();
  const auto numOfRecords = static_cast<int16_t>(file_size / sizeof(userrec));

  File phoneNumFile(FilePath(config()->datadir(), PHONENUM_DAT));
  if (!phoneNumFile.Open(File::modeReadWrite | File::modeAppend | File::modeBinary |
                         File::modeCreateFile)) {
    return;
  }

  for (int16_t temp_user_number = 1; temp_user_number <= numOfRecords; temp_user_number++) {
    if (auto o = users()->readuser(temp_user_number, UserManager::mask::non_deleted)) {
      const auto& user = o.value();
      p.usernum = temp_user_number;
      auto voice_num = user.voice_phone();
      auto data_num = user.data_phone();
      if (!voice_num.empty() && voice_num.find("000-") == std::string::npos) {
        to_char_array(p.phone, voice_num);
        phoneNumFile.Write(&p, sizeof(phonerec));
      }
      if (!data_num.empty() && !iequals(voice_num, data_num) && voice_num.find("000-") == std::string::npos) {
        to_char_array(p.phone, data_num);
        phoneNumFile.Write(&p, sizeof(phonerec));
      }
    }
  }
}

// end dupphone additions
