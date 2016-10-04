/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif  // _WIN32

#include "bbs/arword.h"
#include "bbs/bbs.h"
#include "bbs/colors.h"
#include "bbs/connect1.h"
#include "bbs/events.h"
#include "bbs/execexternal.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/conf.h"
#include "bbs/datetime.h"
#include "bbs/instmsg.h"
#include "bbs/message_file.h"
#include "bbs/netsup.h"
#include "bbs/pause.h"
#include "bbs/wconstants.h"
#include "bbs/workspace.h"
#include "sdk/status.h"
#include "core/datafile.h"
#include "core/inifile.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/os.h"
#include "core/textfile.h"
#include "core/wwivassert.h"
#include "core/wwivport.h"

#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/networks.h"
#include "sdk/subxtr.h"

// Additional INI file function and structure
#include "bbs/xinitini.h"

#ifdef __unix__
#define XINIT_PRINTF( x )
#else
#define XINIT_PRINTF( x ) std::clog << '\xFE' << ' ' << ( x )  << std::endl
#endif // __unix__

struct ini_flags_type {
  int     strnum;
  bool    sense;
  uint32_t value;
};

using std::string;
using wwiv::bbs::TempDisablePause;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::strings;
using namespace wwiv::sdk;

uint32_t GetFlagsFromIniFile(IniFile& pIniFile, ini_flags_type * fs, int nFlagNumber, uint32_t flags);

void StatusManagerCallback(int i) {
  switch (i) {
  case WStatus::fileChangeNames:
  {
    // re-read names.lst
    if (session()->names()) {
      // We may not have the BBS initialized yet, so only
      // re-read the names file if it's changed from another node.
      session()->names()->Load();
    }
  } break;
  case WStatus::fileChangeUpload:
    break;
  case WStatus::fileChangePosts:
    session()->subchg = 1;
    break;
  case WStatus::fileChangeEmail:
    emchg = true;
    break;
  case WStatus::fileChangeNet:
  {
    set_net_num(session()->net_num());
  }
  break;
  }
}

// Turns a string into a bitmapped unsigned short flag for use with
// ExecuteExternalProgram calls.
unsigned short WSession::str2spawnopt(const char *s) {
  char ts[255];

  unsigned short return_val = 0;
  strcpy(ts, s);
  strupr(ts);

  if (strstr(ts, "NOHUP") != nullptr) {
    return_val |= EFLAG_NOHUP;
  }
  if (strstr(ts, "COMIO") != nullptr) {
    return_val |= EFLAG_COMIO;
  }
  if (strstr(ts, "FOSSIL") != nullptr) {
    return_val |= EFLAG_FOSSIL; // RF20020929
  }
  if (strstr(ts, "NETPROG") != nullptr) {
    return_val |= EFLAG_NETPROG;
  }
  return return_val;
}


// Takes string s and creates restrict val
unsigned short WSession::str2restrict(const char *s) {
  const char *rs = restrict_string;
  char s1[81];

  strcpy(s1, s);
  strupr(s1);
  int r = 0;
  for (int i = strlen(rs) - 1; i >= 0; i--) {
    if (strchr(s1, rs[i])) {
      r |= (1 << i);
    }
  }

  return static_cast<int16_t>(r);
}

// Reads WWIV.INI info from [WWIV] subsection, overrides some config.dat
// settings (as appropriate), updates config.dat with those values. Also
// tries to read settings from [WWIV-<instnum>] subsection - this overrides
// those in [WWIV] subsection.

static unsigned char nucol[] = {7, 11, 14, 5, 31, 2, 12, 9, 6, 3};
static unsigned char nucolbw[] = {7, 15, 15, 15, 112, 15, 15, 7, 7, 7};

struct eventinfo_t {
  const char *name;
  unsigned short eflags;
};

// See #defines SPAWNOPT_XXXX in vardec.h for these.
static eventinfo_t eventinfo[] = {
  {"TIMED",         EFLAG_NONE},
  {"NEWUSER",       EFLAG_NONE },
  {"BEGINDAY",      EFLAG_NONE },
  {"LOGON",         EFLAG_NONE},
  {"ULCHK",         EFLAG_NOHUP},
  {"CHAT",          EFLAG_FOSSIL},  // UNUSED (5)
  {"PROT_SINGLE",   EFLAG_NONE },
  {"PROT_BATCH",    EFLAG_NONE},
  {"CHAT",          EFLAG_NONE },
  {"ARCH_E",        EFLAG_NONE},
  {"ARCH_L",        EFLAG_NONE},
  {"ARCH_A",        EFLAG_NONE},
  {"ARCH_D",        EFLAG_NONE},
  {"ARCH_K",        EFLAG_NONE},
  {"ARCH_T",        EFLAG_NONE},
  {"NET_CMD1",      EFLAG_NONE},
  {"NET_CMD2",      EFLAG_NETPROG},
  {"LOGOFF",        EFLAG_NONE},
  {"",              EFLAG_NONE}, // UNUSED (18)
  {"NETWORK",       EFLAG_NONE},
};

static const char *get_key_str(int n, const char *index = nullptr) {
  static char str[255];
  if (!index) {
    return INI_OPTIONS_ARRAY[n];
  }
  sprintf(str, "%s[%s]", INI_OPTIONS_ARRAY[ n ], index);
  return str;
}

static void set_string(IniFile& ini, int key_idx, string* out) {
  const char *s = ini.GetValue(get_key_str(key_idx));
  if (s) {
    out->assign(s);
  }
}

#define INI_INIT_STR(n, f) { set_string(ini, n, &syscfg.f); }

#define INI_GET_ASV(s, f, func, d) \
{const char* ss; if ((ss=ini.GetValue(get_key_str(INI_STR_SIMPLE_ASV,s)))!=nullptr) asv.f = func (ss); else asv.f = d;}

#define NEL(s) (sizeof(s) / sizeof((s)[0]))

static ini_flags_type sysinfo_flags[] = {
  {INI_STR_FORCE_FBACK, false, OP_FLAGS_FORCE_NEWUSER_FEEDBACK},
  {INI_STR_CHECK_DUP_PHONES, false, OP_FLAGS_CHECK_DUPE_PHONENUM},
  {INI_STR_HANGUP_DUP_PHONES, false, OP_FLAGS_HANGUP_DUPE_PHONENUM},
  {INI_STR_USE_SIMPLE_ASV, false, OP_FLAGS_SIMPLE_ASV},
  {INI_STR_POSTTIME_COMPENS, false, OP_FLAGS_POSTTIME_COMPENSATE},
  {INI_STR_SHOW_HIER, false, OP_FLAGS_SHOW_HIER},
  {INI_STR_IDZ_DESC, false, OP_FLAGS_IDZ_DESC},
  {INI_STR_SETLDATE, false, OP_FLAGS_SETLDATE},
  {INI_STR_READ_CD_IDZ, false, OP_FLAGS_READ_CD_IDZ},
  {INI_STR_FSED_EXT_DESC, false, OP_FLAGS_FSED_EXT_DESC},
  {INI_STR_FAST_TAG_RELIST, false, OP_FLAGS_FAST_TAG_RELIST},
  {INI_STR_MAIL_PROMPT, false, OP_FLAGS_MAIL_PROMPT},
  {INI_STR_SHOW_CITY_ST, false, OP_FLAGS_SHOW_CITY_ST},
  {INI_STR_NO_EASY_DL, false, OP_FLAGS_NO_EASY_DL},
  {INI_STR_NEW_EXTRACT, false, OP_FLAGS_NEW_EXTRACT},
  {INI_STR_FAST_SEARCH, false, OP_FLAGS_FAST_SEARCH},
  {INI_STR_NET_CALLOUT, false, OP_FLAGS_NET_CALLOUT},
  {INI_STR_WFC_SCREEN, false, OP_FLAGS_WFC_SCREEN},
  {INI_STR_FIDO_PROCESS, false, OP_FLAGS_FIDO_PROCESS},
  {INI_STR_NET_PROCESS, false, OP_FLAGS_NET_PROCESS},
  {INI_STR_USER_REGISTRATION, false, OP_FLAGS_USER_REGISTRATION},
  {INI_STR_MSG_TAG, false, OP_FLAGS_MSG_TAG},
  {INI_STR_CHAIN_REG, false, OP_FLAGS_CHAIN_REG},
  {INI_STR_CAN_SAVE_SSM, false, OP_FLAGS_CAN_SAVE_SSM},
  {INI_STR_EXTRA_COLOR, false, OP_FLAGS_EXTRA_COLOR},
  {INI_STR_USE_ADVANCED_ASV, false, OP_FLAGS_ADV_ASV},
  {INI_STR_USE_FORCE_SCAN, false, OP_FLAGS_USE_FORCESCAN},
  {INI_STR_NEWUSER_MIN, false, OP_FLAGS_NEWUSER_MIN},

};

static ini_flags_type sysconfig_flags[] = {
  {INI_STR_2WAY_CHAT, false, sysconfig_2_way},
  {INI_STR_NO_NEWUSER_FEEDBACK, false, sysconfig_no_newuser_feedback},
  {INI_STR_TITLEBAR, false, sysconfig_titlebar},
  {INI_STR_LOG_DOWNLOADS, false, sysconfig_log_dl},
  {INI_STR_CLOSE_XFER, false, sysconfig_no_xfer},
  {INI_STR_ALL_UL_TO_SYSOP, false, sysconfig_all_sysop},
  {INI_STR_BEEP_CHAT, true, sysconfig_no_beep},
  {INI_STR_TWO_COLOR_CHAT, false, sysconfig_two_color},
  {INI_STR_ALLOW_ALIASES, true, sysconfig_no_alias},
  {INI_STR_USE_LIST, false, sysconfig_list},
  {INI_STR_EXTENDED_USERINFO, false, sysconfig_extended_info},
  {INI_STR_FREE_PHONE, false, sysconfig_free_phone},
  {INI_STR_ENABLE_PIPES, false, sysconfig_enable_pipes},
  {INI_STR_ENABLE_MCI, false, sysconfig_enable_mci},
};

void WSession::ReadINIFile(IniFile& ini) {
  // Setup default  data
  for (int nTempColorNum = 0; nTempColorNum < 10; nTempColorNum++) {
    newuser_colors[ nTempColorNum ] = nucol[ nTempColorNum ];
    newuser_bwcolors[ nTempColorNum ] = nucolbw[ nTempColorNum ];
  }

  SetChatNameSelectionColor(95);
  SetMessageColor(2);
  max_batch = 50;
  max_extend_lines = 10;
  max_chains = 50;
  max_gfilesec = 32;
  mail_who_field_len = 35;
  SetBeginDayNodeNumber(1);
  SetUseInternalZmodem(true);
  SetExecChildProcessWaitTime(500);
  SetExecLogSyncFoss(true);
  SetNewScanAtLogin(false);

  for (size_t nTempEventNum = 0; nTempEventNum < NEL(eventinfo); nTempEventNum++) {
    spawn_opts[ nTempEventNum ] = eventinfo[ nTempEventNum ].eflags;
  }

  // put in default WSession::flags
  SetConfigFlags(OP_FLAGS_FIDO_PROCESS);
  if (instance_number() == 1) {
    // By default allow node1 to process wwivnet packets.
    SetConfigFlags(OP_FLAGS_NET_PROCESS);
  }

  if (ok_modem_stuff) {
    SetConfigFlag(OP_FLAGS_NET_CALLOUT);
  } else {
    ClearConfigFlag(OP_FLAGS_NET_CALLOUT);
  }

  // found something
  // pull out event flags
  for (size_t nTempSpawnOptNum = 0; nTempSpawnOptNum < NEL(spawn_opts); nTempSpawnOptNum++) {
    const string key_name = StringPrintf("%s[%s]", get_key_str(INI_STR_SPAWNOPT), eventinfo[nTempSpawnOptNum].name);
    const char *ss = ini.GetValue(key_name);
    if (ss != nullptr) {
      spawn_opts[nTempSpawnOptNum] = str2spawnopt(ss);
    }
  }

  // pull out newuser colors
  for (int nTempColorNum = 0; nTempColorNum < 10; nTempColorNum++) {
    string key_name = StringPrintf("%s[%d]", get_key_str(INI_STR_NUCOLOR), nTempColorNum);
    const char *ss = ini.GetValue(key_name);
    if (ss != nullptr && atoi(ss) > 0) {
      newuser_colors[nTempColorNum] = StringToUnsignedChar(ss);
    }
    key_name = StringPrintf("%s[%d]", get_key_str(INI_STR_NUCOLOR), nTempColorNum);
    ss = ini.GetValue(key_name);
    if (ss != nullptr && atoi(ss) > 0) {
      newuser_bwcolors[nTempColorNum] = StringToUnsignedChar(ss);
    }
  }

  SetCarbonCopyEnabled(ini.GetBooleanValue("ALLOW_CC_BCC"));

  // pull out sysop-side colors
  localIO()->SetTopScreenColor(ini.GetNumericValue(
    get_key_str(INI_STR_TOPCOLOR), localIO()->GetTopScreenColor()));
  localIO()->SetUserEditorColor(ini.GetNumericValue(
    get_key_str(INI_STR_F1COLOR), localIO()->GetUserEditorColor()));
  localIO()->SetEditLineColor(ini.GetNumericValue(
    get_key_str(INI_STR_EDITLINECOLOR), localIO()->GetEditLineColor()));
  SetChatNameSelectionColor(ini.GetNumericValue(
    get_key_str(INI_STR_CHATSELCOLOR),GetChatNameSelectionColor()));

  // pull out sizing options
  max_batch = ini.GetNumericValue(get_key_str(INI_STR_MAX_BATCH), max_batch);
  max_extend_lines = ini.GetNumericValue(get_key_str(INI_STR_MAX_EXTEND_LINES),
                                          max_extend_lines);
  max_chains = ini.GetNumericValue(get_key_str(INI_STR_MAX_CHAINS), max_chains);
  max_gfilesec = ini.GetNumericValue(get_key_str(INI_STR_MAX_GFILESEC), max_gfilesec);

  // pull out strings
  INI_INIT_STR(INI_STR_UPLOAD_CMD, upload_cmd);
  INI_INIT_STR(INI_STR_BEGINDAY_CMD, beginday_cmd);
  INI_INIT_STR(INI_STR_NEWUSER_CMD, newuser_cmd);
  INI_INIT_STR(INI_STR_LOGON_CMD, logon_cmd);
  INI_INIT_STR(INI_STR_TERMINAL_CMD, terminal_command);

  m_nForcedReadSubNumber = ini.GetNumericValue(get_key_str(INI_STR_FORCE_SCAN_SUBNUM),
                                                m_nForcedReadSubNumber);
  m_bInternalZmodem = ini.GetBooleanValue(get_key_str(INI_STR_INTERNALZMODEM),
                                            m_bInternalZmodem);
  m_bNewScanAtLogin = ini.GetBooleanValue(get_key_str(INI_STR_NEW_SCAN_AT_LOGIN),
                                            m_bNewScanAtLogin);

  m_bExecLogSyncFoss = ini.GetBooleanValue(get_key_str(INI_STR_EXEC_LOG_SYNCFOSS),
                                            m_bExecLogSyncFoss);
  m_nExecChildProcessWaitTime = 
      ini.GetNumericValue(get_key_str(INI_STR_EXEC_CHILD_WAIT_TIME), m_nExecChildProcessWaitTime);

  SetBeginDayNodeNumber(ini.GetNumericValue(get_key_str(INI_STR_BEGINDAYNODENUMBER),
                                      GetBeginDayNodeNumber()));

  // pull out sysinfo_flags
  SetConfigFlags(GetFlagsFromIniFile(ini, sysinfo_flags, NEL(sysinfo_flags),
                                    GetConfigFlags()));

  // allow override of WSession::message_color_
  SetMessageColor(ini.GetNumericValue(get_key_str(INI_STR_MSG_COLOR), GetMessageColor()));

  // get asv values
  if (HasConfigFlag(OP_FLAGS_SIMPLE_ASV)) {
    INI_GET_ASV("SL", sl, StringToUnsignedChar, syscfg.autoval[9].sl);
    INI_GET_ASV("DSL", dsl, StringToUnsignedChar, syscfg.autoval[9].dsl);
    INI_GET_ASV("EXEMPT", exempt, StringToUnsignedChar, 0);
    INI_GET_ASV("AR", ar, str_to_arword, syscfg.autoval[9].ar);
    INI_GET_ASV("DAR", dar, str_to_arword, syscfg.autoval[9].dar);
    INI_GET_ASV("RESTRICT", restrict, str2restrict, 0);
  }
  if (HasConfigFlag(OP_FLAGS_ADV_ASV)) {
    advasv.reg_wwiv = ini.GetNumericValue<uint8_t>(get_key_str(INI_STR_ADVANCED_ASV, "REG_WWIV"), 1);
    advasv.nonreg_wwiv = ini.GetNumericValue<uint8_t>(get_key_str(INI_STR_ADVANCED_ASV, "NONREG_WWIV"), 1);
    advasv.non_wwiv = ini.GetNumericValue<uint8_t>(get_key_str(INI_STR_ADVANCED_ASV, "NON_WWIV"), 1);
    advasv.cosysop = ini.GetNumericValue<uint8_t>(get_key_str(INI_STR_ADVANCED_ASV, "COSYSOP"), 1);
  }

  // sysconfig flags
  syscfg.sysconfig = static_cast<uint16_t>(GetFlagsFromIniFile(ini, sysconfig_flags,
                      NEL(sysconfig_flags), syscfg.sysconfig));

  const char* ss;
  // misc stuff
  if (((ss = ini.GetValue(get_key_str(INI_STR_MAIL_WHO_LEN))) != nullptr) &&
      (atoi(ss) > 0 || ss[0] == '0')) {
    mail_who_field_len = StringToUnsignedShort(ss);
  }
  if ((ss = ini.GetValue(get_key_str(INI_STR_RATIO))) != nullptr) {
    syscfg.req_ratio = static_cast<float>(atof(ss));
  }

  if ((ss = ini.GetValue(get_key_str(INI_STR_ATTACH_DIR))) != nullptr) {
    m_attachmentDirectory = ss;
    if (m_attachmentDirectory.at(m_attachmentDirectory.length() - 1) != File::pathSeparatorChar) {
      m_attachmentDirectory += File::pathSeparatorString;
    }
  } else {
    std::ostringstream os;
    os << GetHomeDir() << ATTACH_DIR << File::pathSeparatorString;
    m_attachmentDirectory = os.str();
  }

  screen_saver_time = ini.GetNumericValue(get_key_str(INI_STR_SCREEN_SAVER_TIME),
                                            screen_saver_time);

  max_extend_lines    = std::min<uint16_t>(max_extend_lines, 99);
  max_batch           = std::min<uint16_t>(max_batch , 999);
  max_chains          = std::min<uint16_t>(max_chains, 999);
  max_gfilesec        = std::min<uint16_t>(max_gfilesec, 999);
}

bool WSession::ReadInstanceSettings(int instance_number, IniFile& ini) {
  const char* temp_directory_char = ini.GetValue("TEMP_DIRECTORY");
  if (temp_directory_char == nullptr) {
    LOG(ERROR) << "TEMP_DIRECTORY must be set in WWIV.INI.";
    return false;
  }
  string temp_directory(temp_directory_char);
  // TEMP_DIRECTORY is defined in wwiv.ini, also default the batch_directory to 
  // TEMP_DIRECTORY if BATCH_DIRECTORY does not exist.
  string batch_directory(ini.GetValue("BATCH_DIRECTORY", temp_directory.c_str()));

  // Replace %n with instance number value.
  string instance_num_string = std::to_string(instance_number);
  StringReplace(&temp_directory, "%n", instance_num_string);
  StringReplace(&batch_directory, "%n", instance_num_string);

  const string base_dir = GetHomeDir();
  File::MakeAbsolutePath(base_dir, &batch_directory);
  File::MakeAbsolutePath(base_dir, &temp_directory);
  File::EnsureTrailingSlash(&temp_directory);
  File::EnsureTrailingSlash(&batch_directory);

  temp_directory_ = temp_directory;
  batch_directory_ = batch_directory;

  int max_num_instances = ini.GetNumericValue("NUM_INSTANCES", 4);
  if (instance_number > max_num_instances) {
    LOG(ERROR) << "Not enough instances configured (" << max_num_instances << ").";
    return false;
  }
  return true;
}

static char* DuplicatePath(const char* path) {
  char* out = new char[MAX_PATH + 1];
  strncpy(out, path, MAX_PATH);
  return out;
}

bool WSession::ReadConfig() {
  config_.reset(new Config());
  if (!config_->IsInitialized()) {
    LOG(ERROR) << CONFIG_DAT << " NOT FOUND.";
    return false;
  }

  if (!config_->versioned_config_dat()) {
    LOG(ERROR) << "Please run INIT to upgrade " << CONFIG_DAT << " to the most recent version.";
    return false;
  }

  // initialize the user manager
  const configrec* config = config_->config();
  user_manager_.reset(new UserManager(config->datadir, config->userreclen, config->maxusers));
  statusMgr.reset(new StatusMgr(config_->datadir(), StatusManagerCallback));
  
  const string instance_name = StringPrintf("WWIV-%u", instance_number());
  std::unique_ptr<IniFile> ini = std::make_unique<IniFile>(FilePath(GetHomeDir(), WWIV_INI), instance_name, INI_TAG);
  if (!ini->IsOpen()) {
    LOG(ERROR) << "Unable to read WWIV.INI.";
    AbortBBS();
  }
  ReadINIFile(*ini);
  bool config_ovr_read = ReadInstanceSettings(instance_number(), *ini);
  if (!config_ovr_read) {
    return false;
  }

  // update user info data
  int16_t userreclen = static_cast<int16_t>(sizeof(userrec));
  int16_t waitingoffset = offsetof(userrec, waiting);
  int16_t inactoffset = offsetof(userrec, inact);
  int16_t sysstatusoffset = offsetof(userrec, sysstatus);
  int16_t fuoffset = offsetof(userrec, forwardusr);
  int16_t fsoffset = offsetof(userrec, forwardsys);
  int16_t fnoffset = offsetof(userrec, net_num);

  if (userreclen != config_->config()->userreclen           ||
      waitingoffset != config_->config()->waitingoffset     ||
      inactoffset != config_->config()->inactoffset         ||
      sysstatusoffset != config_->config()->sysstatusoffset ||
      fuoffset != config_->config()->fuoffset               ||
      fsoffset != config_->config()->fsoffset               ||
      fnoffset != config_->config()->fnoffset) {
    config_->config()->userreclen      = userreclen;
    config_->config()->waitingoffset   = waitingoffset;
    config_->config()->inactoffset     = inactoffset;
    config_->config()->sysstatusoffset = sysstatusoffset;
    config_->config()->fuoffset        = fuoffset;
    config_->config()->fsoffset        = fsoffset;
    config_->config()->fnoffset        = fnoffset;
  }

  syscfg.newuserpw        = strdup(config_->config()->newuserpw);
  syscfg.systempw         = strdup(config_->config()->systempw);

  syscfg.datadir          = DuplicatePath(config_->config()->datadir);
  
  syscfg.systemname       = strdup(config_->config()->systemname);
  syscfg.systemphone      = strdup(config_->config()->systemphone);
  syscfg.sysopname        = strdup(config_->config()->sysopname);

  syscfg.newusersl        = config_->config()->newusersl;
  syscfg.newuserdsl       = config_->config()->newuserdsl;
  syscfg.maxwaiting       = config_->config()->maxwaiting;
  syscfg.newuploads       = config_->config()->newuploads;
  syscfg.closedsystem     = config_->config()->closedsystem;

  syscfg.systemnumber     = config_->config()->systemnumber;
  syscfg.maxusers         = config_->config()->maxusers;
  syscfg.newuser_restrict = config_->config()->newuser_restrict;
  syscfg.sysconfig        = config_->config()->sysconfig;
  syscfg.sysoplowtime     = config_->config()->sysoplowtime;
  syscfg.sysophightime    = config_->config()->sysophightime;
  syscfg.executetime      = config_->config()->executetime;
  syscfg.max_subs         = config_->config()->max_subs;
  syscfg.max_dirs         = config_->config()->max_dirs;
  syscfg.qscn_len         = config_->config()->qscn_len;
  syscfg.userreclen       = config_->config()->userreclen;

  syscfg.post_call_ratio  = config_->config()->post_call_ratio;
  syscfg.req_ratio        = config_->config()->req_ratio;
  syscfg.newusergold      = config_->config()->newusergold;

  syscfg.autoval[0]       = config_->config()->autoval[0];
  syscfg.autoval[1]       = config_->config()->autoval[1];
  syscfg.autoval[2]       = config_->config()->autoval[2];
  syscfg.autoval[3]       = config_->config()->autoval[3];
  syscfg.autoval[4]       = config_->config()->autoval[4];
  syscfg.autoval[5]       = config_->config()->autoval[5];
  syscfg.autoval[6]       = config_->config()->autoval[6];
  syscfg.autoval[7]       = config_->config()->autoval[7];
  syscfg.autoval[8]       = config_->config()->autoval[8];
  syscfg.autoval[9]       = config_->config()->autoval[9];

  syscfg.wwiv_reg_number  = config_->config()->wwiv_reg_number;

  make_abs_path(syscfg.datadir);

  char temp_dir[MAX_PATH];
  to_char_array(temp_dir, temp_directory());
  make_abs_path(temp_dir);
  temp_directory_ = temp_dir;

  char batch_dir[MAX_PATH];
  to_char_array(batch_dir, batch_directory());
  make_abs_path(batch_dir);
  batch_directory_ = batch_dir;

  return true;
}


void WSession::read_nextern() {
  externs.clear();
  DataFile<newexternalrec> externalFile(config()->datadir(), NEXTERN_DAT);
  if (externalFile) {
    externalFile.ReadVector(externs, 15);
  }
}

void WSession::read_arcs() {
  arcs.clear();
  DataFile<arcrec> file(config()->datadir(), ARCHIVER_DAT);
  if (file) {
    file.ReadVector(arcs, MAX_ARCS);
  }
}

void WSession::read_editors() {
  editors.clear();
  DataFile<editorrec> file(config()->datadir(), EDITORS_DAT);
  if (!file) {
    return;
  }
  file.ReadVector(editors, 10);
}

void WSession::read_nintern() {
  over_intern.clear();
  DataFile<newexternalrec> file(config()->datadir(), NINTERN_DAT);
  if (file) {
    file.ReadVector(over_intern, 3);
  }
}

bool WSession::read_subs() {
  subs_.reset(new wwiv::sdk::Subs(config_->datadir(), net_networks));
  subs_->Load();
  return true;
}

void WSession::read_networks() {
  internetEmailName = "";
  internetEmailDomain = "";
  internetPopDomain = "";
  SetInternetUseRealNames(false);

  // TODO: wire up for NetX
  TextFile fileNetIni("NET.INI", "rt");
  if (fileNetIni.IsOpen()) {
    while (!fileNetIni.IsEndOfFile()) {
      char buffer[255];
      fileNetIni.ReadLine(buffer, 80);
      buffer[sizeof(buffer) - 1] = 0;
      StringRemoveWhitespace(buffer);
      if (!strncasecmp(buffer, "DOMAIN=", 7) && internetEmailDomain.empty()) {
        internetEmailDomain = &(buffer[7]);
      } else if (!strncasecmp(buffer, "POPNAME=", 8) && internetEmailName.empty()) {
        internetEmailName = &(buffer[8]);
      } else if (!strncasecmp(buffer, "FWDDOM=", 7)) {
        internetEmailDomain = &(buffer[7]);
      } else if (!strncasecmp(buffer, "FWDNAME=", 8)) {
        internetEmailName = &(buffer[8]);
      } else if (!strncasecmp(buffer, "POPDOMAIN=", 10)) {
        internetPopDomain = &(buffer[10]);
      } else if (!strncasecmp(buffer, "REALNAME=", 9) &&
                 (buffer[9] == 'Y' || buffer[9] == 'y')) {
        SetInternetUseRealNames(true);
      }
    }
    fileNetIni.Close();
  }

  wwiv::sdk::Networks networks(*config());
  if (networks.IsInitialized()) {
    net_networks = networks.networks();
  }

  // Add a default entry for us.
  if (net_networks.empty()) {
    net_networks_rec n{};
    strcpy(n.name, "WWIVnet");
    string datadir = config()->datadir();
    strcpy(n.dir, datadir.c_str());
    n.sysnum = syscfg.systemnumber;
  }
}

bool WSession::read_names() {
  // Load the SDK Names class too.
  names_.reset(new Names(*config_.get()));
  return true;
}

bool WSession::read_dirs() {
  directories.clear();
  DataFile<directoryrec> file(config()->datadir(), DIRS_DAT);
  if (!file) {
    LOG(ERROR) << file.file().GetName() << " NOT FOUND.";
    return false;
  }
  file.ReadVector(directories, syscfg.max_dirs);
  return true;
}

void WSession::read_chains() {
  chains.clear();
  DataFile<chainfilerec> file(config()->datadir(), CHAINS_DAT);
  if (!file) {
    return;
  }
  file.ReadVector(chains, max_chains);

  if (HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
    chains_reg.clear();

    DataFile<chainregrec> regFile(config()->datadir(), CHAINS_REG);
    if (regFile) {
      regFile.ReadVector(chains_reg, max_chains);
    } else {
      regFile.Close();
      for (size_t nTempChainNum = 0; nTempChainNum < chains.size(); nTempChainNum++) {
        chainregrec reg;
        memset(&reg, 0, sizeof(chainregrec));
        reg.maxage = 255;
        chains_reg.push_back(reg);
      }
      DataFile<chainregrec> writeFile(config()->datadir(), CHAINS_REG, 
          File::modeReadWrite | File::modeBinary | File::modeCreateFile);
      writeFile.WriteVector(chains_reg);
    }
  }
}

bool WSession::read_language() {
  {
    DataFile<languagerec> file(config()->datadir(), LANGUAGE_DAT);
    if (file) {
      file.ReadVector(languages);
    }
  }
  if (languages.empty()) {
    // Add a default language to the list.
    languagerec lang;
    memset(&lang, 0, sizeof(languagerec));
    strcpy(lang.name, "English");
    to_char_array(lang.dir, session()->config()->gfilesdir());
    to_char_array(lang.mdir, session()->config()->menudir());
    
    languages.emplace_back(lang);
  }

  set_language_number(-1);
  if (!set_language(0)) {
    LOG(ERROR) << "You need the default language installed to run the BBS.";
    return false;
  }
  return true;
}

void WSession::read_gfile() {
  DataFile<gfiledirrec> file(config()->datadir(), GFILE_DAT);
  if (file) {
    file.ReadVector(gfilesec, max_gfilesec);
  }
}

/**
 * Makes a path into an absolute path, returns true if original path altered,
 * else returns false
 */
void WSession::make_abs_path(char *pszDirectory) {
  string base(GetHomeDir());
  string dir(pszDirectory);
  File::MakeAbsolutePath(base, &dir);
  strcpy(pszDirectory, dir.c_str());
}

static bool SaveConfig() {
  File file(CONFIG_DAT);
  if (!file.Open(File::modeBinary | File::modeReadWrite)) {
    return false;
  }
  configrec full_syscfg;
  file.Read(&full_syscfg, sizeof(configrec));
  // These are set by WWIV.INI, set them back so that changes
  // will be propagated to config.dat
  full_syscfg.sysconfig = syscfg.sysconfig;
  full_syscfg.qscn_len = syscfg.qscn_len;
  full_syscfg.userreclen = syscfg.userreclen;

  full_syscfg.req_ratio = syscfg.req_ratio;

  full_syscfg.autoval[0] = syscfg.autoval[0];
  full_syscfg.autoval[1] = syscfg.autoval[1];
  full_syscfg.autoval[2] = syscfg.autoval[2];
  full_syscfg.autoval[3] = syscfg.autoval[3];
  full_syscfg.autoval[4] = syscfg.autoval[4];
  full_syscfg.autoval[5] = syscfg.autoval[5];
  full_syscfg.autoval[6] = syscfg.autoval[6];
  full_syscfg.autoval[7] = syscfg.autoval[7];
  full_syscfg.autoval[8] = syscfg.autoval[8];
  full_syscfg.autoval[9] = syscfg.autoval[9];

  file.Seek(0, File::seekBegin);
  file.Write(&full_syscfg, sizeof(configrec));
  return true;
}

void WSession::InitializeBBS() {
  localIO()->SetScreenBottom(localIO()->GetDefaultScreenBottom());
  defscreenbottom = localIO()->GetDefaultScreenBottom();
  // set default screenlinest
  screenlinest = defscreenbottom + 1;

  localIO()->Cls();
#if !defined( __unix__ )
  std::clog << std::endl << wwiv_version << beta_version << ", Copyright (c) 1998-2016, WWIV Software Services."
            << std::endl << std::endl
            << "\r\nInitializing BBS..." << std::endl;
#endif // __unix__
  SetCurrentReadMessageArea(-1);
  use_workspace = false;
  chat_file = false;
  clearnsp();
  bquote = 0;
  equote = 0;

  XINIT_PRINTF("Processing configuration file: WWIV.INI.");
  if (!File::Exists(temp_directory())) {
    if (!File::mkdirs(temp_directory())) {
      LOG(ERROR) << "Your temp dir isn't valid.";
      LOG(ERROR) << "It is now set to: '" << temp_directory() << "'";
      AbortBBS();
    }
  }

  if (!File::Exists(batch_directory())) {
    if (!File::mkdirs(batch_directory())) {
      LOG(ERROR) << "Your batch dir isn't valid.";
      LOG(ERROR) << "It is now set to: '" << batch_directory() << "'";
      AbortBBS();
    }
  }

  write_inst(INST_LOC_INIT, 0, INST_FLAGS_NONE);

  // make sure it is the new USERREC structure
  XINIT_PRINTF("Reading user scan pointers.");
  File fileQScan(config()->datadir(), USER_QSC);
  if (!fileQScan.Exists()) {
    LOG(ERROR) << "Could not open file '" << fileQScan.full_pathname() << "'";
    LOG(ERROR) << "You must go into INIT and convert your userlist before running the BBS.";
    AbortBBS();
  }

  if (!read_language()) {
    AbortBBS();
  }

  set_net_num(0);
  read_networks();
  set_net_num(0);

  XINIT_PRINTF("Reading status information.");
  WStatus* pStatus = statusMgr->BeginTransaction();
  if (!pStatus) {
    LOG(ERROR) << "Unable to return statusrec.dat.";
    AbortBBS();
  }

  XINIT_PRINTF("Reading color information.");
  File fileColor(config()->datadir(), COLOR_DAT);
  if (!fileColor.Exists()) {
    buildcolorfile();
  }
  get_colordata();

  pStatus->SetWWIVVersion(wwiv_num_version);
  pStatus->EnsureCallerNumberIsValid();
  statusMgr->CommitTransaction(pStatus);

  gat = static_cast<unsigned short*>(BbsAllocA(2048 * sizeof(uint16_t)));

  XINIT_PRINTF("Reading Gfiles.");
  read_gfile();

  XINIT_PRINTF("Reading user names.");
  if (!read_names()) {
    AbortBBS();
  }

  XINIT_PRINTF("Reading Message Areas.");
  if (!read_subs()) {
    AbortBBS();
  }

  XINIT_PRINTF("Reading File Areas.");
  if (!read_dirs()) {
    AbortBBS();
  }

  XINIT_PRINTF("Reading Chains.");
  read_chains();

  XINIT_PRINTF("Reading File Transfer Protocols.");
  read_nextern();
  read_nintern();

  XINIT_PRINTF("Reading File Archivers.");
  read_arcs();
  SaveConfig();

  XINIT_PRINTF("Reading Full Screen Message Editors.");
  read_editors();

  if (!File::mkdirs(m_attachmentDirectory)) {
    LOG(ERROR) << "Your file attachment directory is invalid.";
    LOG(ERROR) << "It is now set to: " << m_attachmentDirectory << "'";
    AbortBBS();
  }
  CdHome();

  check_phonenum(); // dupphone addition
  batch().clear();

  XINIT_PRINTF("Reading User Information.");
  ReadCurrentUser(1);
  statusMgr->RefreshStatusCache();
  topdata = LocalIO::topdataUser;

  dsz_logfile_name_ = StrCat(session()->temp_directory(), "dsz.log");

#if !defined ( __unix__ ) && !defined ( __APPLE__ )
  string newprompt = "WWIV: ";
  const string old_prompt = environment_variable("PROMPT");
  if (!old_prompt.empty()) {
    newprompt.append(old_prompt);
  } else {
    newprompt.append("$P$G");
  }
  // put in our environment since passing the xenviron wasn't working
  // with sync emulated fossil
  set_environment_variable("PROMPT", newprompt);

  if (environment_variable("DSZLOG").empty()) {
    set_environment_variable("DSZLOG", dsz_logfile_name_);
  }
  set_environment_variable("BBS", wwiv_version);

#endif // defined ( __unix__ )

  XINIT_PRINTF("Reading External Events.");
  init_events();
  last_time = time_event - timer();
  if (last_time < 0.0) {
    last_time += SECONDS_PER_DAY;
  }

  XINIT_PRINTF("Allocating Memory for Message/File Areas.");
  do_event = 0;
  usub.resize(syscfg.max_subs);
  udir.resize(syscfg.max_dirs);
  uconfsub = static_cast<userconfrec *>(BbsAllocA(MAX_CONFERENCES * sizeof(userconfrec)));
  uconfdir = static_cast<userconfrec *>(BbsAllocA(MAX_CONFERENCES * sizeof(userconfrec)));
  qsc = static_cast<uint32_t *>(BbsAllocA(syscfg.qscn_len));
  qsc_n = qsc + 1;
  qsc_q = qsc_n + (syscfg.max_dirs + 31) / 32;
  qsc_p = qsc_q + (syscfg.max_subs + 31) / 32;

  network_extension_ = ".net";
  const string wwiv_instance(environment_variable("WWIV_INSTANCE"));
  if (!wwiv_instance.empty()) {
    int inst_num = atoi(wwiv_instance.c_str());
    if (inst_num > 0) {
      network_extension_ = StringPrintf(".%3.3d", inst_num);
      // Fix... Set the global instance variable to match this.  When you run WWIV with the -n<instance> parameter
      // it sets the WWIV_INSTANCE environment variable, however it wasn't doing the reverse.
      instance_number_ = inst_num;
    }
  }

  frequent_init();
  if (!user_already_on_) {
    TempDisablePause disable_pause;
    remove_from_temp("*.*", temp_directory(), true);
    remove_from_temp("*.*", batch_directory(), true);
    cleanup_net();
  }
  subconfnum = dirconfnum = 0;

  XINIT_PRINTF("Reading Conferences.");
  read_all_conferences();

  if (!user_already_on_) {
    sysoplog(false);
    sysoplog(false) << "WWIV " << wwiv_version << beta_version << ", inst " << instance_number()
      << ", brought up at " << times() << " on " << fulldate() << ".";
  }
  if (instance_number() > 1) {
    File::Remove(StringPrintf("%s.%3.3u", WWIV_NET_NOEXT, instance_number()));
  } else {
    File::Remove(WWIV_NET_DAT);
  }

  srand(static_cast<unsigned int>(time(nullptr)));
  catsl();

  XINIT_PRINTF("Saving Instance information.");
  write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
}


// begin dupphone additions

void WSession::check_phonenum() {
  File phoneFile(config()->datadir(), PHONENUM_DAT);
  if (!phoneFile.Exists()) {
    create_phone_file();
  }
}

void WSession::create_phone_file() {
  phonerec p;

  File file(config()->datadir(), USER_LST);
  if (!file.Open(File::modeReadOnly | File::modeBinary)) {
    return;
  }
  long lFileSize = file.GetLength();
  file.Close();
  int numOfRecords = static_cast<int>(lFileSize / sizeof(userrec));

  File phoneNumFile(config()->datadir(), PHONENUM_DAT);
  if (!phoneNumFile.Open(File::modeReadWrite | File::modeAppend | File::modeBinary | File::modeCreateFile)) {
    return;
  }

  for (int16_t nTempUserNumber = 1; nTempUserNumber <= numOfRecords; nTempUserNumber++) {
    User user;
    users()->ReadUser(&user, nTempUserNumber);
    if (!user.IsUserDeleted()) {
      p.usernum = nTempUserNumber;
      char szTempVoiceNumber[255], szTempDataNumber[255];
      strcpy(szTempVoiceNumber, user.GetVoicePhoneNumber());
      strcpy(szTempDataNumber, user.GetDataPhoneNumber());
      if (szTempVoiceNumber[0] && !strstr(szTempVoiceNumber, "000-")) {
        strcpy(reinterpret_cast<char*>(p.phone), szTempVoiceNumber);
        phoneNumFile.Write(&p, sizeof(phonerec));
      }
      if (szTempDataNumber[0] &&
          !IsEquals(szTempVoiceNumber, szTempDataNumber) &&
          !strstr(szTempVoiceNumber, "000-")) {
        strcpy(reinterpret_cast<char*>(p.phone), szTempDataNumber);
        phoneNumFile.Write(&p, sizeof(phonerec));
      }
    }
  }
  phoneNumFile.Close();
}

uint32_t GetFlagsFromIniFile(IniFile& ini, ini_flags_type * fs, int nFlagNumber, uint32_t flags) {
  for (int i = 0; i < nFlagNumber; i++) {
    const char* ss = INI_OPTIONS_ARRAY[ fs[i].strnum ];
    if (ss && ini.GetValue(ss)) {
      if (ini.GetBooleanValue(ss)) {
        if (fs[i].sense) {
          flags &= ~fs[i].value;
        } else {
          flags |= fs[i].value;
        }
      } else {
        if (fs[i].sense) {
          flags |= fs[i].value;
        } else {
          flags &= ~fs[i].value;
        }
      }
    }
  }

  return flags;
}

// end dupphone additions
