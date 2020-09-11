/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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

#include "bbs/arword.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/conf.h"
#include "bbs/connect1.h"
#include "bbs/instmsg.h"
#include "bbs/multinst.h"
#include "bbs/netsup.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xinitini.h"
#include "common/common_events.h"
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
#include "local_io/wconstants.h"
#include "sdk/chains.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/files/files.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/names.h"
#include "sdk/net/networks.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>

using std::string;
using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::seconds;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::strings;
using namespace wwiv::sdk;

void StatusManagerCallback(int i) {
  switch (i) {
  case WStatus::fileChangeNames: {
    // re-read names.lst
    if (a()->names()) {
      // We may not have the BBS initialized yet, so only
      // re-read the names file if it's changed from another node.
      a()->names()->Load();
    }
  } break;
  case WStatus::fileChangeUpload:
    break;
  case WStatus::fileChangePosts:
    a()->subchg = 1;
    break;
  case WStatus::fileChangeEmail:
    a()->emchg_ = true;
    break;
  case WStatus::fileChangeNet: {
    set_net_num(a()->net_num());
  } break;
  default: // NOP
    ;
  }
}

// Turns a string into a bitmapped unsigned short flag for use with ExecuteExternalProgram.
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
    return_val |= EFLAG_FOSSIL;
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
  char s1[81];

  to_char_array(s1, ToStringUpperCase(s));
  uint16_t r = 0;
  for (auto i = ssize(restrict_string) - 1; i >= 0; i--) {
    if (strchr(s1, restrict_string[i])) {
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

// TODO(rushfan): If we nee this elsewhere add it into IniFile
static std::string to_array_key(const std::string& n, const std::string& index) {
  return StrCat(n, "[", index, "]");
}

template <typename A>
void ini_get_asv(const IniFile& ini, const std::string& s, A& f,
                 std::function<A(const std::string&)> func, A d) {
  const auto ss = ini.value<std::string>(StrCat(INI_STR_SIMPLE_ASV, "[", s, "]"));
  if (!ss.empty()) {
    f = func(ss);
  } else {
    f = d;
  }
}

#define INI_GET_ASV(s, f, func, d)                                                                 \
  {                                                                                                \
    const auto ss = ini.value<std::string>(to_array_key(INI_STR_SIMPLE_ASV, s));                   \
    if (!ss.empty()) {                                                                             \
      (f) = func(ss);                                                                                \
    } else {                                                                                       \
      (f) = d;                                                                                       \
    }                                                                                              \
  }

static std::vector<ini_flags_type> sysinfo_flags = {
    {INI_STR_FORCE_FBACK, OP_FLAGS_FORCE_NEWUSER_FEEDBACK},
    {INI_STR_CHECK_DUP_PHONES, OP_FLAGS_CHECK_DUPE_PHONENUM},
    {INI_STR_HANGUP_DUP_PHONES, OP_FLAGS_HANGUP_DUPE_PHONENUM},
    {INI_STR_USE_SIMPLE_ASV, OP_FLAGS_SIMPLE_ASV},
    {INI_STR_POSTTIME_COMPENS, OP_FLAGS_POSTTIME_COMPENSATE},
    {INI_STR_SHOW_HIER, OP_FLAGS_SHOW_HIER},
    {INI_STR_IDZ_DESC, OP_FLAGS_IDZ_DESC},
    {INI_STR_SETLDATE, OP_FLAGS_SETLDATE},
    {INI_STR_READ_CD_IDZ, OP_FLAGS_READ_CD_IDZ},
    {INI_STR_FSED_EXT_DESC, OP_FLAGS_FSED_EXT_DESC},
    {INI_STR_FAST_TAG_RELIST, OP_FLAGS_FAST_TAG_RELIST},
    {INI_STR_MAIL_PROMPT, OP_FLAGS_MAIL_PROMPT},
    {INI_STR_SHOW_CITY_ST, OP_FLAGS_SHOW_CITY_ST},
    {INI_STR_WFC_SCREEN, OP_FLAGS_WFC_SCREEN},
    {INI_STR_MSG_TAG, OP_FLAGS_MSG_TAG},
    {INI_STR_CHAIN_REG, OP_FLAGS_CHAIN_REG},
    {INI_STR_CAN_SAVE_SSM, OP_FLAGS_CAN_SAVE_SSM},
    {INI_STR_USE_FORCE_SCAN, OP_FLAGS_USE_FORCESCAN},
    {INI_STR_NEWUSER_MIN, OP_FLAGS_NEWUSER_MIN},
};

static std::vector<ini_flags_type> sysconfig_flags = {
    {INI_STR_2WAY_CHAT, sysconfig_2_way},
    {INI_STR_NO_NEWUSER_FEEDBACK, sysconfig_no_newuser_feedback},
    {INI_STR_TITLEBAR, sysconfig_titlebar},
    {INI_STR_LOG_DOWNLOADS, sysconfig_log_dl},
    {INI_STR_CLOSE_XFER, sysconfig_no_xfer},
    {INI_STR_ALL_UL_TO_SYSOP, sysconfig_all_sysop},
    {INI_STR_ALLOW_ALIASES, sysconfig_allow_alias},
    {INI_STR_EXTENDED_USERINFO, sysconfig_extended_info},
    {INI_STR_FREE_PHONE, sysconfig_free_phone}};

void Application::ReadINIFile(IniFile& ini) {
  // Setup default  data
  chatname_color_ = 95;
  message_color_ = 2;
  max_batch = 50;
  max_extend_lines = 10;
  max_chains = 50;
  max_gfilesec = 32;
  mail_who_field_len = 35;

  // Found something, pull out event flags.
  for (const auto& kv : eventinfo) {
    spawn_opts_[kv.first] = kv.second;
    const auto key_name = to_array_key(INI_STR_SPAWNOPT, kv.first);
    const auto ss = ini.value<string>(key_name);
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
  localIO()->SetTopScreenColor(ini.value<uint8_t>(INI_STR_TOPCOLOR, localIO()->GetTopScreenColor()));
  localIO()->SetUserEditorColor(ini.value<uint8_t>(INI_STR_F1COLOR, localIO()->GetUserEditorColor()));
  localIO()->SetEditLineColor(ini.value<uint8_t>(INI_STR_EDITLINECOLOR, localIO()->GetEditLineColor()));
  chatname_color_ = ini.value<int>(INI_STR_CHATSELCOLOR, GetChatNameSelectionColor());

  // pull out sizing options
  max_batch = ini.value<uint16_t>(INI_STR_MAX_BATCH, max_batch);
  max_extend_lines = ini.value<uint16_t>(INI_STR_MAX_EXTEND_LINES, max_extend_lines);
  max_chains = ini.value<uint16_t>(INI_STR_MAX_CHAINS, max_chains);
  max_gfilesec = ini.value<uint16_t>(INI_STR_MAX_GFILESEC, max_gfilesec);

  // pull out strings
  upload_cmd = ini.value<string>(INI_STR_UPLOAD_CMD);
  beginday_cmd = ini.value<string>(INI_STR_BEGINDAY_CMD);
  newuser_cmd = ini.value<string>(INI_STR_NEWUSER_CMD);
  logon_cmd = ini.value<string>(INI_STR_LOGON_CMD);
  logoff_cmd = ini.value<string>(INI_STR_LOGOFF_CMD);
  cleanup_cmd = ini.value<string>(INI_STR_CLEANUP_CMD);
  terminal_command = ini.value<string>(INI_STR_TERMINAL_CMD);

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

  // misc stuff
  auto num = ini.value<uint16_t>(INI_STR_MAIL_WHO_LEN);
  if (num > 0) {
    mail_who_field_len = num;
  }

  const auto attach_dir = ini.value<string>(INI_STR_ATTACH_DIR);
  attach_dir_ = !attach_dir.empty() ? attach_dir : FilePath(bbsdir(), ATTACH_DIR).string();
  attach_dir_ = File::EnsureTrailingSlash(attach_dir_);

  screen_saver_time = ini.value<uint16_t>("SCREEN_SAVER_TIME", screen_saver_time);

  max_extend_lines = std::min<uint16_t>(max_extend_lines, 99);
  max_batch = std::min<uint16_t>(max_batch, 999);
  max_chains = std::min<uint16_t>(max_chains, 999);
  max_gfilesec = std::min<uint16_t>(max_gfilesec, 999);

  full_screen_read_prompt_ = ini.value<bool>("FULL_SCREEN_READER", true);
  bin.set_logon_key_timeout(seconds(std::max<int>(10, ini.value<int>("LOGON_KEY_TIMEOUT", 30))));
  bin.set_default_key_timeout(seconds(std::max<int>(30, ini.value<int>("USER_KEY_TIMEOUT", 180))));
  bin.set_sysop_key_timeout(seconds(std::max<int>(30, ini.value<int>("SYSOP_KEY_TIMEOUT", 600))));
}

bool Application::ReadInstanceSettings(int instance_number, IniFile& ini) {
  auto temp_directory = ini.value<string>("TEMP_DIRECTORY");
  if (temp_directory.empty()) {
    LOG(ERROR) << "TEMP_DIRECTORY must be set in WWIV.INI.";
    return false;
  }

  temp_directory = File::FixPathSeparators(temp_directory);
  // TEMP_DIRECTORY is defined in wwiv.ini, also default the batch_directory to
  // TEMP_DIRECTORY if BATCH_DIRECTORY does not exist.
  auto batch_directory(ini.value<string>("BATCH_DIRECTORY", temp_directory));
  batch_directory = File::FixPathSeparators(batch_directory);

  // Replace %n with instance number value.
  const auto instance_num_string = std::to_string(instance_number);
  StringReplace(&temp_directory, "%n", instance_num_string);
  StringReplace(&batch_directory, "%n", instance_num_string);

  // Set the directories (temp, batch, language)
  const auto base_dir = bbsdir();
  const auto temp = File::EnsureTrailingSlash(File::absolute(base_dir, temp_directory));
  const auto batch = File::EnsureTrailingSlash(File::absolute(base_dir, batch_directory));

  wwiv::common::Dirs d(temp, batch, batch, config()->gfilesdir());
  sess().dirs(d);

  // Set config for macro processing.
  bbs_macro_context_.set_config(config());

  const auto max_num_instances = ini.value<int>("NUM_INSTANCES", 4);
  if (instance_number > max_num_instances) {
    LOG(ERROR) << "Not enough instances configured (" << max_num_instances << ").";
    return false;
  }
  return true;
}

bool Application::ReadConfig() {
  config_.reset(new Config(bbsdir()));
  if (!config_->IsInitialized()) {
    LOG(ERROR) << CONFIG_DAT << " NOT FOUND.";
    return false;
  }

  if (!config_->versioned_config_dat()) {
    std::cerr << "Please run wwivconfig to upgrade " << CONFIG_DAT
              << " to the most recent version.\r\n";
    LOG(ERROR) << "Please run wwivconfig to upgrade " << CONFIG_DAT
               << " to the most recent version.";
    sleep_for(seconds(2));
    return false;
  }

  // initialize the user manager
  user_manager_.reset(new UserManager(*config_));
  statusMgr.reset(new StatusMgr(config_->datadir(), StatusManagerCallback));

  IniFile ini(FilePath(bbsdir(), WWIV_INI), {StrCat("WWIV-", instance_number()), INI_TAG});
  if (!ini.IsOpen()) {
    LOG(ERROR) << "Unable to read WWIV.INI.";
    return false;
  }
  ReadINIFile(ini);
  if (!ReadInstanceSettings(instance_number(), ini)) {
    return false;
  }

  return true;
}

void Application::read_nextern() {
  externs.clear();
  DataFile<newexternalrec> externalFile(FilePath(config()->datadir(), NEXTERN_DAT));
  if (externalFile) {
    externalFile.ReadVector(externs, 15);
  }
}

void Application::read_arcs() {
  arcs.clear();
  DataFile<arcrec> file(FilePath(config()->datadir(), ARCHIVER_DAT));
  if (file) {
    file.ReadVector(arcs, MAX_ARCS);
  }
}

void Application::read_editors() {
  editors.clear();
  DataFile<editorrec> file(FilePath(config()->datadir(), EDITORS_DAT));
  if (!file) {
    return;
  }
  file.ReadVector(editors, 10);
}

void Application::read_nintern() {
  over_intern.clear();
  DataFile<newexternalrec> file(FilePath(config()->datadir(), NINTERN_DAT));
  if (file) {
    file.ReadVector(over_intern, 3);
  }
}

bool Application::read_subs() {
  subs_ = std::make_unique<Subs>(config_->datadir(), nets_->networks());
  return subs_->Load();
}

class BBSLastReadImpl : public wwiv::sdk::msgapi::WWIVLastReadImpl {
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
  internetEmailName = "";
  internetEmailDomain = "";
  internetPopDomain = "";
  SetInternetUseRealNames(false);

  // TODO(rushfan): Remove these and put them somewhere else.
  // Like on a per-network config when we add proper internet
  // support.
  IniFile ini("net.ini", {"NETWORK"});
  if (ini.IsOpen()) {
    // Note FWDNAME isn't listed here.
    internetEmailName = ini.value<string>("POPNAME");
    // Note FWDDOM isn't listed here.
    internetEmailDomain = ini.value<string>("DOMAIN");
    internetPopDomain = ini.value<string>("POPDOMAIN");
    SetInternetUseRealNames(ini.value<bool>("REALNAME"));
  }

  nets_ = std::make_unique<Networks>(*config());
}

bool Application::read_names() {
  // Load the SDK Names class too.
  names_.reset(new Names(*config_));
  return true;
}

bool Application::read_dirs() {
  dirs_ = std::make_unique<wwiv::sdk::files::Dirs>(config_->datadir());
  return dirs_->Load();
}

void Application::read_chains() {
  chains = std::make_unique<Chains>(*config());
  if (chains->IsInitialized()) {
    chains->Save();
  }
}

bool Application::read_language() {
  {
    DataFile<languagerec> file(FilePath(config()->datadir(), LANGUAGE_DAT));
    if (file) {
      file.ReadVector(languages);
    }
  }
  if (languages.empty()) {
    // Add a default language to the list.
    languagerec lang{};
    to_char_array(lang.name, "English");
    to_char_array(lang.dir, config()->gfilesdir());
    to_char_array(lang.mdir, config()->menudir());

    languages.emplace_back(lang);
  }

  set_language_number(-1);
  if (!set_language(0)) {
    LOG(ERROR) << "You need the default language installed to run the BBS.";
    return false;
  }
  return true;
}

void Application::read_gfile() {
  DataFile<gfiledirrec> file(FilePath(config()->datadir(), GFILE_DAT));
  if (file) {
    file.ReadVector(gfilesec, max_gfilesec);
  }
}

static void PrintTime(const wwiv::common::SessionContext& context) {
  SavedLine line = bout.SaveCurrentLine();

  bout.Color(0);
  bout.nl(2);
  auto dt = DateTime::now();
  bout << "|#2" << dt.to_string() << wwiv::endl;
  if (context.IsUserOnline()) {
    auto time_on = std::chrono::system_clock::now() - context.system_logon_time();
    auto seconds_on =
        static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(time_on).count());
    bout << "|#9Time on   = |#1" << ctim(seconds_on) << wwiv::endl;
    bout << "|#9Time left = |#1" << ctim(nsl()) << wwiv::endl;
  }
  bout.nl();
  bout.RestoreCurrentLine(line);
}

bool Application::InitializeBBS(bool cleanup_network) {
  Cls();
  std::clog << std::endl
            << full_version() << ", Copyright (c) 1998-2020, WWIV Software Services."
            << std::endl
            << std::endl
            << "\r\nInitializing BBS..." << std::endl;
  use_workspace = false;

  bin.clearnsp();

  // Set dirs in the session context first.

  VLOG(1) << "Processing configuration file: WWIV.INI.";
  if (!File::Exists(sess().dirs().temp_directory())) {
    if (!File::mkdirs(sess().dirs().temp_directory())) {
      LOG(ERROR) << "Your temp dir isn't valid.";
      LOG(ERROR) << "It is now set to: '" << sess().dirs().temp_directory() << "'";
      return false;
    }
  }

  if (!File::Exists(sess().dirs().batch_directory())) {
    if (!File::mkdirs(sess().dirs().batch_directory())) {
      LOG(ERROR) << "Your batch dir isn't valid.";
      LOG(ERROR) << "It is now set to: '" << sess().dirs().batch_directory() << "'";
      return false;
    }
  }

  write_inst(INST_LOC_INIT, 0, INST_FLAGS_NONE);

  // make sure it is the new USERREC structure
  VLOG(1) << "Reading user scan pointers.";
  const auto qs_fn = FilePath(config()->datadir(), USER_QSC);
  if (!File::Exists(qs_fn)) {
    LOG(ERROR) << "Could not open file '" << qs_fn << "'";
    LOG(ERROR) << "You must go into wwivconfig and convert your userlist before running the BBS.";
    return false;
  }

  if (!read_language()) {
    return false;
  }

  read_networks();
  if (!create_message_api()) {
    return false;
  }

  VLOG(1) << "Reading status information.";
  auto status = statusMgr->BeginTransaction();
  if (!status) {
    LOG(ERROR) << "Unable to return statusrec.dat.";
    return false;
  }

  status->SetWWIVVersion(wwiv_config_version());
  status->EnsureCallerNumberIsValid();
  statusMgr->CommitTransaction(std::move(status));

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
  statusMgr->RefreshStatusCache();
  localIO()->topdata(LocalIO::topdata_t::user);

  // Set DSZLOG
  dsz_logfile_name_ = FilePath(sess().dirs().temp_directory(), "dsz.log").string();
  if (environment_variable("DSZLOG").empty()) {
    set_environment_variable("DSZLOG", dsz_logfile_name_);
  }
  // SET BBS environment variable.
  set_environment_variable("BBS", full_version());
  sess().InitalizeContext(*config());

  VLOG(1) << "Allocating Memory for Message/File Areas.";
  usub.resize(config()->max_subs());
  udir.resize(config()->max_dirs());

  network_extension_ = ".net";
  const auto wwiv_instance = environment_variable("WWIV_INSTANCE");
  if (!wwiv_instance.empty()) {
    const auto inst_num = to_number<int>(wwiv_instance);
    if (inst_num > 0) {
      network_extension_ = fmt::sprintf(".%3.3d", inst_num);
      // Fix... Set the global instance variable to match this.  When you run WWIV with the
      // -n<instance> parameter it sets the WWIV_INSTANCE environment variable, however it wasn't
      // doing the reverse.
      instance_number_ = inst_num;
    }
  }

  frequent_init();
  VLOG(1) << "Reading Conferences.";
  read_all_conferences();

  TempDisablePause disable_pause(bout);
  const auto t = session_context_.dirs().temp_directory();
  const auto t2 = sess().dirs().temp_directory();
  remove_from_temp("*.*", sess().dirs().temp_directory(), false);
  remove_from_temp("*.*", sess().dirs().batch_directory(), false);
  remove_from_temp("*.*", sess().dirs().qwk_directory(), false);

  if (cleanup_network) {
    cleanup_net();
    sysoplog(false) << "";
    sysoplog(false) << "WWIV " << full_version() << ", inst " << instance_number()
                    << ", brought up at " << times() << " on " << fulldate() << ".";
  }

  catsl();

  VLOG(1) << "Saving Instance information.";
  write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);

  //using namespace wwiv::common;
  // Register Application Level Callbacks
  bus().add_handler<ProcessInstanceMessages>([this]() {
    if (inst_msg_waiting() && !sess().chatline()) {
      process_inst_msgs();
    }
  });
  bus().add_handler<ResetProcessingInstanceMessages>([]() { setiia(std::chrono::seconds(5)); });
  bus().add_handler<PauseProcessingInstanceMessages>([]() { setiia(std::chrono::seconds(0)); });
  bus().add_handler<CheckForHangupEvent>([]() { a()->CheckForHangup(); });
  bus().add_handler<HangupEvent>([]() { a()->Hangup(); });
  bus().add_handler<UpdateTopScreenEvent>([]() { a()->UpdateTopScreen(); });
  bus().add_handler<UpdateTimeLeft>(
      [](const UpdateTimeLeft& u) { a()->tleft(u.check_for_timeout); });
  bus().add_handler<HandleSysopKey>(
      [](const HandleSysopKey& k) { a()->handle_sysop_key(k.key); });
  bus().add_handler<GiveupTimeslices>([]() { 
    yield();
    if (inst_msg_waiting() && (!a()->sess().in_chatroom() || !a()->sess().chatline())) {
      process_inst_msgs();
    } else {
      sleep_for(std::chrono::milliseconds(100));
    }
    yield();
  });
  bus().add_handler<DisplayTimeLeft>([]() { PrintTime(a()->sess()); });
  bus().add_handler<DisplayMultiInstanceStatus>([]() { multi_instance(); });

  bus().add_handler<ToggleAvailable>([]() { toggle_avail(); });
  bus().add_handler<ToggleInvisble>([]() { toggle_invis(); });

  return true;
}

// begin dupphone additions

void Application::check_phonenum() {
  const auto fn = FilePath(config()->datadir(), PHONENUM_DAT);
  if (!File::Exists(fn)) {
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
  auto file_size = file.length();
  file.Close();
  const auto numOfRecords = static_cast<int16_t>(file_size / sizeof(userrec));

  File phoneNumFile(FilePath(config()->datadir(), PHONENUM_DAT));
  if (!phoneNumFile.Open(File::modeReadWrite | File::modeAppend | File::modeBinary |
                         File::modeCreateFile)) {
    return;
  }

  for (int16_t nTempUserNumber = 1; nTempUserNumber <= numOfRecords; nTempUserNumber++) {
    User user;
    users()->readuser(&user, nTempUserNumber);
    if (!user.IsUserDeleted()) {
      p.usernum = nTempUserNumber;
      std::string voice_num = user.GetVoicePhoneNumber();
      std::string data_num = user.GetDataPhoneNumber();
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
  phoneNumFile.Close();
}

// end dupphone additions
