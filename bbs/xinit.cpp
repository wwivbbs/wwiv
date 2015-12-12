/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/conf.h"
#include "bbs/datetime.h"
#include "bbs/instmsg.h"
#include "bbs/message_file.h"
#include "bbs/netsup.h"
#include "bbs/pause.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "core/datafile.h"
#include "core/inifile.h"
#include "core/strings.h"
#include "core/os.h"
#include "core/textfile.h"
#include "core/wwivassert.h"
#include "core/wwivport.h"

#include "sdk/config.h"
#include "sdk/filenames.h"

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

uint32_t GetFlagsFromIniFile(IniFile *pIniFile, ini_flags_type * fs, int nFlagNumber, uint32_t flags);


// Turns a string into a bitmapped unsigned short flag for use with
// ExecuteExternalProgram calls.
unsigned short WSession::str2spawnopt(const char *s) {
  char ts[ 255 ];

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

  return static_cast< short >(r);
}

// begin callback addition

unsigned char WSession::stryn2tf(const char *s) {
  char ch = wwiv::UpperCase<char>(*s);

  char yesChar = *(YesNoString(true));
  if (ch == yesChar || ch == 1 || ch == '1' || ch == 'T') {
    return 1;
  }
  return 0;
}


// end callback addition

#define OFFOF(x) static_cast<int16_t>(reinterpret_cast<long>(&user()->data.x) - reinterpret_cast<long>(&user()->data))

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

static void set_string(IniFile* ini, int key_idx, string* out) {
  const char *s = ini->GetValue(get_key_str(key_idx));
  if (s) {
    out->assign(s);
  }
}

#define INI_INIT_STR(n, f) { set_string(ini, n, &syscfg.f); }

#define INI_GET_ASV(s, f, func, d) \
{const char* ss; if ((ss=ini->GetValue(get_key_str(INI_STR_SIMPLE_ASV,s)))!=nullptr) asv.f = func (ss); else asv.f = d;}
#define INI_GET_CALLBACK(s, f, func, d) \
{const char* ss; if ((ss=ini->GetValue(get_key_str(INI_STR_CALLBACK,s)))!=nullptr) \
    cbv.f = func (ss); \
    else cbv.f = d;}

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
  {INI_STR_SLASH_SZ, false, OP_FLAGS_SLASH_SZ},
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
  {INI_STR_USER_REGISTRATION, false, OP_FLAGS_USER_REGISTRATION},
  {INI_STR_MSG_TAG, false, OP_FLAGS_MSG_TAG},
  {INI_STR_CHAIN_REG, false, OP_FLAGS_CHAIN_REG},
  {INI_STR_CAN_SAVE_SSM, false, OP_FLAGS_CAN_SAVE_SSM},
  {INI_STR_EXTRA_COLOR, false, OP_FLAGS_EXTRA_COLOR},
  {INI_STR_THREAD_SUBS, false, OP_FLAGS_THREAD_SUBS},
  {INI_STR_USE_CALLBACK, false, OP_FLAGS_CALLBACK},
  {INI_STR_USE_VOICE_VAL, false, OP_FLAGS_VOICE_VAL},
  {INI_STR_USE_ADVANCED_ASV, false, OP_FLAGS_ADV_ASV},
  {INI_STR_USE_FORCE_SCAN, false, OP_FLAGS_USE_FORCESCAN},
  {INI_STR_NEWUSER_MIN, false, OP_FLAGS_NEWUSER_MIN},

};

static ini_flags_type sysconfig_flags[] = {
  {INI_STR_LOCAL_SYSOP, true, sysconfig_no_local},
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

IniFile* WSession::ReadINIFile() {
  // Setup default  data
  for (int nTempColorNum = 0; nTempColorNum < 10; nTempColorNum++) {
    newuser_colors[ nTempColorNum ] = nucol[ nTempColorNum ];
    newuser_bwcolors[ nTempColorNum ] = nucolbw[ nTempColorNum ];
  }

  SetTopScreenColor(31);
  SetUserEditorColor(31);
  SetEditLineColor(112);
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

  if (ok_modem_stuff) {
    SetConfigFlag(OP_FLAGS_NET_CALLOUT);
  } else {
    ClearConfigFlag(OP_FLAGS_NET_CALLOUT);
  }

  // initialize ini communication
  const string instance_name = StringPrintf("WWIV-%u", GetInstanceNumber());
  IniFile* ini(new IniFile(FilePath(GetHomeDir(), WWIV_INI), instance_name, INI_TAG));
  if (ini->IsOpen()) {
    // found something
    // pull out event flags
    for (size_t nTempSpawnOptNum = 0; nTempSpawnOptNum < NEL(spawn_opts); nTempSpawnOptNum++) {
      const string key_name = StringPrintf("%s[%s]", get_key_str(INI_STR_SPAWNOPT), eventinfo[nTempSpawnOptNum].name);
      const char *ss = ini->GetValue(key_name);
      if (ss != nullptr) {
        spawn_opts[nTempSpawnOptNum] = str2spawnopt(ss);
      }
    }

    // pull out newuser colors
    for (int nTempColorNum = 0; nTempColorNum < 10; nTempColorNum++) {
      string key_name = StringPrintf("%s[%d]", get_key_str(INI_STR_NUCOLOR), nTempColorNum);
      const char *ss = ini->GetValue(key_name);
      if (ss != nullptr && atoi(ss) > 0) {
        newuser_colors[nTempColorNum] = StringToUnsignedChar(ss);
      }
      key_name = StringPrintf("%s[%d]", get_key_str(INI_STR_NUCOLOR), nTempColorNum);
      ss = ini->GetValue(key_name);
      if (ss != nullptr && atoi(ss) > 0) {
        newuser_bwcolors[nTempColorNum] = StringToUnsignedChar(ss);
      }
    }

    SetMessageThreadingEnabled(ini->GetBooleanValue("THREAD_SUBS"));
    SetCarbonCopyEnabled(ini->GetBooleanValue("ALLOW_CC_BCC"));

    // pull out sysop-side colors
    SetTopScreenColor(ini->GetNumericValue(get_key_str(INI_STR_TOPCOLOR),
                                    GetTopScreenColor()));
    SetUserEditorColor(ini->GetNumericValue(get_key_str(INI_STR_F1COLOR),
                                     GetUserEditorColor()));
    SetEditLineColor(ini->GetNumericValue(get_key_str(INI_STR_EDITLINECOLOR),
                                   GetEditLineColor()));
    SetChatNameSelectionColor(ini->GetNumericValue(get_key_str(INI_STR_CHATSELCOLOR),
                                            GetChatNameSelectionColor()));

    // pull out sizing options
    max_batch = ini->GetNumericValue(get_key_str(INI_STR_MAX_BATCH), max_batch);
    max_extend_lines = ini->GetNumericValue(get_key_str(INI_STR_MAX_EXTEND_LINES),
                                     max_extend_lines);
    max_chains = ini->GetNumericValue(get_key_str(INI_STR_MAX_CHAINS), max_chains);
    max_gfilesec = ini->GetNumericValue(get_key_str(INI_STR_MAX_GFILESEC), max_gfilesec);

    // pull out strings
    INI_INIT_STR(INI_STR_UPLOAD_CMD, upload_cmd);
    INI_INIT_STR(INI_STR_BEGINDAY_CMD, beginday_cmd);
    INI_INIT_STR(INI_STR_NEWUSER_CMD, newuser_cmd);
    INI_INIT_STR(INI_STR_LOGON_CMD, logon_cmd);

    m_nForcedReadSubNumber = ini->GetNumericValue(get_key_str(INI_STR_FORCE_SCAN_SUBNUM),
                                           m_nForcedReadSubNumber);
    m_bInternalZmodem = ini->GetBooleanValue(get_key_str(INI_STR_INTERNALZMODEM),
                                      m_bInternalZmodem);
    m_bNewScanAtLogin = ini->GetBooleanValue(get_key_str(INI_STR_NEW_SCAN_AT_LOGIN),
                                      m_bNewScanAtLogin);

    m_bExecLogSyncFoss = ini->GetBooleanValue(get_key_str(INI_STR_EXEC_LOG_SYNCFOSS),
                                       m_bExecLogSyncFoss);
    m_nExecChildProcessWaitTime = ini->GetNumericValue(get_key_str(INI_STR_EXEC_CHILD_WAIT_TIME),
        m_nExecChildProcessWaitTime);

    SetBeginDayNodeNumber(ini->GetNumericValue(get_key_str(INI_STR_BEGINDAYNODENUMBER),
                                        GetBeginDayNodeNumber()));

    // pull out sysinfo_flags
    SetConfigFlags(GetFlagsFromIniFile(ini, sysinfo_flags, NEL(sysinfo_flags),
                                     GetConfigFlags()));

    // allow override of WSession::m_nMessageColor
    SetMessageColor(ini->GetNumericValue(get_key_str(INI_STR_MSG_COLOR), GetMessageColor()));

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
      advasv.reg_wwiv = ini->GetNumericValue<uint8_t>(get_key_str(INI_STR_ADVANCED_ASV, "REG_WWIV"), 1);
      advasv.nonreg_wwiv = ini->GetNumericValue<uint8_t>(get_key_str(INI_STR_ADVANCED_ASV, "NONREG_WWIV"), 1);
      advasv.non_wwiv = ini->GetNumericValue<uint8_t>(get_key_str(INI_STR_ADVANCED_ASV, "NON_WWIV"), 1);
      advasv.cosysop = ini->GetNumericValue<uint8_t>(get_key_str(INI_STR_ADVANCED_ASV, "COSYSOP"), 1);
    }

    // get callback values
    if (HasConfigFlag(OP_FLAGS_CALLBACK)) {
      INI_GET_CALLBACK("SL", sl, StringToUnsignedChar, syscfg.autoval[2].sl);
      INI_GET_CALLBACK("DSL", dsl, StringToUnsignedChar, syscfg.autoval[2].dsl);
      INI_GET_CALLBACK("EXEMPT", exempt, StringToUnsignedChar, 0);
      INI_GET_CALLBACK("AR", ar, str_to_arword, syscfg.autoval[2].ar);
      INI_GET_CALLBACK("DAR", dar, str_to_arword, syscfg.autoval[2].dar);
      INI_GET_CALLBACK("RESTRICT", restrict, str2restrict, 0);
      INI_GET_CALLBACK("FORCED", forced, stryn2tf, 0);
      INI_GET_CALLBACK("LONG_DISTANCE", longdistance, stryn2tf, 0);
      INI_GET_CALLBACK("REPEAT", repeat, StringToUnsignedChar, 0);
    }

    // sysconfig flags
    syscfg.sysconfig = static_cast<unsigned short>(GetFlagsFromIniFile(ini, sysconfig_flags,
                       NEL(sysconfig_flags), syscfg.sysconfig));

    const char* ss;
    // misc stuff
    if (((ss = ini->GetValue(get_key_str(INI_STR_MAIL_WHO_LEN))) != nullptr) &&
        (atoi(ss) > 0 || ss[0] == '0')) {
      mail_who_field_len = StringToUnsignedShort(ss);
    }
    if ((ss = ini->GetValue(get_key_str(INI_STR_RATIO))) != nullptr) {
      syscfg.req_ratio = static_cast<float>(atof(ss));
    }

    if ((ss = ini->GetValue(get_key_str(INI_STR_ATTACH_DIR))) != nullptr) {
      m_attachmentDirectory = ss;
      if (m_attachmentDirectory.at(m_attachmentDirectory.length() - 1) != File::pathSeparatorChar) {
        m_attachmentDirectory += File::pathSeparatorString;
      }
    } else {
      std::ostringstream os;
      os << GetHomeDir() << ATTACH_DIR << File::pathSeparatorString;
      m_attachmentDirectory = os.str();
    }

    screen_saver_time = ini->GetNumericValue(get_key_str(INI_STR_SCREEN_SAVER_TIME),
                                      screen_saver_time);
  }

  max_extend_lines    = std::min<unsigned short>(max_extend_lines, 99);
  max_batch           = std::min<unsigned short>(max_batch , 999);
  max_chains          = std::min<unsigned short>(max_chains, 999);
  max_gfilesec        = std::min<unsigned short>(max_gfilesec, 999);

  // can't allow user to change these on-the-fly
  unsigned short omb = max_batch;
  if (omb) {
    max_batch = omb;
  }
  unsigned short omc = max_chains;
  if (omc) {
    max_chains = omc;
  }
  unsigned short omg = max_gfilesec;
  if (omg) {
    max_gfilesec = omg;
  }

  set_wwivmail_enabled(ini->GetBooleanValue("USE_WWIVMAIL", true));
  set_internal_qwk_enabled(ini->GetBooleanValue("USE_INTERNAL_QWK", true));
  return ini;
}

bool WSession::ReadConfigOverlayFile(int instance_number, IniFile* ini) {
  const char* temp_directory_char = ini->GetValue("TEMP_DIRECTORY");
  if (temp_directory_char == nullptr) {
    std::clog << "TEMP_DIRECTORY must be set in WWIV.INI." << std::endl;
    return false;
  }
  string temp_directory(temp_directory_char);
  // TEMP_DIRECTORY is defined in wwiv.ini, also default the batch_directory to 
  // TEMP_DIRECTORY if BATCH_DIRECTORY does not exist.
  string batch_directory(ini->GetValue("BATCH_DIRECTORY", temp_directory.c_str()));

  // Replace %n with instance number value.
  string instance_num_string = std::to_string(instance_number);
  StringReplace(&temp_directory, "%n", instance_num_string);
  StringReplace(&batch_directory, "%n", instance_num_string);

  const string base_dir = GetHomeDir();
  File::MakeAbsolutePath(base_dir, &batch_directory);
  File::MakeAbsolutePath(base_dir, &temp_directory);
  File::EnsureTrailingSlash(&temp_directory);
  File::EnsureTrailingSlash(&batch_directory);

  syscfgovr.primaryport = 1;
  strcpy(syscfgovr.tempdir, temp_directory.c_str());
  strcpy(syscfgovr.batchdir, batch_directory.c_str());

  int max_num_instances = ini->GetNumericValue("NUM_INSTANCES", 4);
  if (instance_number > max_num_instances) {
    std::clog << "Not enough instances configured (" << max_num_instances << ")." << std::endl;
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
  wwiv::sdk::Config full_config;
  if (!full_config.IsInitialized()) {
    std::clog << CONFIG_DAT << " NOT FOUND." << std::endl;
    return false;
  }

  // initialize the user manager
  users()->InitializeUserManager(full_config.config()->datadir, full_config.config()->userreclen, full_config.config()->maxusers);

  std::unique_ptr<IniFile> ini(ReadINIFile());
  if (!ini->IsOpen()) {
    std::clog << "Insufficient memory for system info structure." << std::endl;
    AbortBBS();
  }

  bool config_ovr_read = ReadConfigOverlayFile(GetInstanceNumber(),ini.get());
  if (!config_ovr_read) {
    return false;
  }

  // update user info data
  int16_t userreclen = static_cast<int16_t>(sizeof(userrec));
  int16_t waitingoffset = OFFOF(waiting);
  int16_t inactoffset = OFFOF(inact);
  int16_t sysstatusoffset = OFFOF(sysstatus);
  int16_t fuoffset = OFFOF(forwardusr);
  int16_t fsoffset = OFFOF(forwardsys);
  int16_t fnoffset = OFFOF(net_num);

  if (userreclen != full_config.config()->userreclen           ||
      waitingoffset != full_config.config()->waitingoffset     ||
      inactoffset != full_config.config()->inactoffset         ||
      sysstatusoffset != full_config.config()->sysstatusoffset ||
      fuoffset != full_config.config()->fuoffset               ||
      fsoffset != full_config.config()->fsoffset               ||
      fnoffset != full_config.config()->fnoffset) {
    full_config.config()->userreclen      = userreclen;
    full_config.config()->waitingoffset   = waitingoffset;
    full_config.config()->inactoffset     = inactoffset;
    full_config.config()->sysstatusoffset = sysstatusoffset;
    full_config.config()->fuoffset        = fuoffset;
    full_config.config()->fsoffset        = fsoffset;
    full_config.config()->fnoffset        = fnoffset;
  }

  syscfg.newuserpw        = strdup(full_config.config()->newuserpw);
  syscfg.systempw         = strdup(full_config.config()->systempw);

  syscfg.msgsdir          = DuplicatePath(full_config.config()->msgsdir);
  syscfg.gfilesdir        = DuplicatePath(full_config.config()->gfilesdir);
  syscfg.datadir          = DuplicatePath(full_config.config()->datadir);
  syscfg.dloadsdir        = DuplicatePath(full_config.config()->dloadsdir);
  syscfg.menudir          = DuplicatePath(full_config.config()->menudir);

  syscfg.systemname       = strdup(full_config.config()->systemname);
  syscfg.systemphone      = strdup(full_config.config()->systemphone);
  syscfg.sysopname        = strdup(full_config.config()->sysopname);

  syscfg.newusersl        = full_config.config()->newusersl;
  syscfg.newuserdsl       = full_config.config()->newuserdsl;
  syscfg.maxwaiting       = full_config.config()->maxwaiting;
  syscfg.newuploads       = full_config.config()->newuploads;
  syscfg.closedsystem     = full_config.config()->closedsystem;

  syscfg.systemnumber     = full_config.config()->systemnumber;
  syscfg.maxusers         = full_config.config()->maxusers;
  syscfg.newuser_restrict = full_config.config()->newuser_restrict;
  syscfg.sysconfig        = full_config.config()->sysconfig;
  syscfg.sysoplowtime     = full_config.config()->sysoplowtime;
  syscfg.sysophightime    = full_config.config()->sysophightime;
  syscfg.executetime      = full_config.config()->executetime;
  syscfg.unused_netlowtime = full_config.config()->unused_netlowtime;
  syscfg.unused_nethightime = full_config.config()->unused_nethightime;
  syscfg.max_subs         = full_config.config()->max_subs;
  syscfg.max_dirs         = full_config.config()->max_dirs;
  syscfg.qscn_len         = full_config.config()->qscn_len;
  syscfg.userreclen       = full_config.config()->userreclen;

  syscfg.post_call_ratio  = full_config.config()->post_call_ratio;
  syscfg.req_ratio        = full_config.config()->req_ratio;
  syscfg.newusergold      = full_config.config()->newusergold;

  syscfg.autoval[0]       = full_config.config()->autoval[0];
  syscfg.autoval[1]       = full_config.config()->autoval[1];
  syscfg.autoval[2]       = full_config.config()->autoval[2];
  syscfg.autoval[3]       = full_config.config()->autoval[3];
  syscfg.autoval[4]       = full_config.config()->autoval[4];
  syscfg.autoval[5]       = full_config.config()->autoval[5];
  syscfg.autoval[6]       = full_config.config()->autoval[6];
  syscfg.autoval[7]       = full_config.config()->autoval[7];
  syscfg.autoval[8]       = full_config.config()->autoval[8];
  syscfg.autoval[9]       = full_config.config()->autoval[9];

  syscfg.wwiv_reg_number  = full_config.config()->wwiv_reg_number;

  make_abs_path(syscfg.gfilesdir);
  make_abs_path(syscfg.datadir);
  make_abs_path(syscfg.msgsdir);
  make_abs_path(syscfg.dloadsdir);
  make_abs_path(syscfg.menudir);

  make_abs_path(syscfgovr.tempdir);
  make_abs_path(syscfgovr.batchdir);

  return true;
}

bool WSession::SaveConfig() {
  File configFile(CONFIG_DAT);
  if (configFile.Open(File::modeBinary | File::modeReadWrite)) {
    configrec full_syscfg;
    configFile.Read(&full_syscfg, sizeof(configrec));
    // These are set by WWIV.INI, set them back so that changes
    // will be propagated to config.dat
    full_syscfg.sysconfig       = syscfg.sysconfig;
    full_syscfg.qscn_len        = syscfg.qscn_len;
    full_syscfg.userreclen      = syscfg.userreclen;

    full_syscfg.req_ratio       = syscfg.req_ratio;

    full_syscfg.autoval[0]      = syscfg.autoval[0];
    full_syscfg.autoval[1]      = syscfg.autoval[1];
    full_syscfg.autoval[2]      = syscfg.autoval[2];
    full_syscfg.autoval[3]      = syscfg.autoval[3];
    full_syscfg.autoval[4]      = syscfg.autoval[4];
    full_syscfg.autoval[5]      = syscfg.autoval[5];
    full_syscfg.autoval[6]      = syscfg.autoval[6];
    full_syscfg.autoval[7]      = syscfg.autoval[7];
    full_syscfg.autoval[8]      = syscfg.autoval[8];
    full_syscfg.autoval[9]      = syscfg.autoval[9];

    for (int nTempArcNum = 0; nTempArcNum < 4; nTempArcNum++) {
      strcpy(full_syscfg.arcs[ nTempArcNum ].extension, arcs[ nTempArcNum ].extension);
      strcpy(full_syscfg.arcs[ nTempArcNum ].arca, arcs[ nTempArcNum ].arca);
      strcpy(full_syscfg.arcs[ nTempArcNum ].arce, arcs[ nTempArcNum ].arce);
      strcpy(full_syscfg.arcs[ nTempArcNum ].arcl, arcs[ nTempArcNum ].arcl);
    }

    full_syscfg.unused_rrd = 0;
    memset(full_syscfg.unused_regcode, '\0', 83);

    configFile.Seek(0, File::seekBegin);
    configFile.Write(&full_syscfg, sizeof(configrec));
    return false;
  }
  return true;
}

void WSession::read_nextern() {
  SetNumberOfExternalProtocols(0);
  if (externs) {
    free(externs);
    externs = nullptr;
  }

  File externalFile(syscfg.datadir, NEXTERN_DAT);
  if (externalFile.Open(File::modeBinary | File::modeReadOnly)) {
    unsigned long lFileSize = externalFile.GetLength();
    if (lFileSize > 15 * sizeof(newexternalrec)) {
      lFileSize = 15 * sizeof(newexternalrec);
    }
    externs = static_cast<newexternalrec *>(BbsAllocA(lFileSize + 10));
    SetNumberOfExternalProtocols(externalFile.Read(externs, lFileSize) / sizeof(newexternalrec));
  }
}

void WSession::read_arcs() {
  if (arcs) {
    free(arcs);
    arcs = nullptr;
  }

  File archiverFile(syscfg.datadir, ARCHIVER_DAT);
  if (archiverFile.Open(File::modeBinary | File::modeReadOnly)) {
    unsigned long lFileSize = archiverFile.GetLength();
    if (lFileSize > MAX_ARCS * sizeof(arcrec)) {
      lFileSize = MAX_ARCS * sizeof(arcrec);
    }
    arcs = static_cast<arcrec *>(BbsAllocA(lFileSize));
    archiverFile.Read(arcs, lFileSize);
  }
}

void WSession::read_editors() {
  editors.clear();
  DataFile<editorrec> file(syscfg.datadir, EDITORS_DAT);
  if (!file) {
    return;
  }
  int num_editors = std::min(10, file.number_of_records());
  editors.resize(num_editors);
  file.Read(&editors[0], num_editors);
}

void WSession::read_nintern() {
  if (over_intern) {
    free(over_intern);
    over_intern = nullptr;
  }
  File file(syscfg.datadir, NINTERN_DAT);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    over_intern = static_cast<newexternalrec *>(BbsAllocA(3 * sizeof(newexternalrec)));

    file.Read(over_intern, 3 * sizeof(newexternalrec));
  }
}

bool WSession::read_subs() {
  if (subboards) {
    free(subboards);
  }
  subboards = nullptr;
  SetMaxNumberMessageAreas(syscfg.max_subs);
  subboards = static_cast< subboardrec * >(BbsAllocA(GetMaxNumberMessageAreas() * sizeof(subboardrec)));

  File file(syscfg.datadir, SUBS_DAT);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    std::clog << file.GetName() << " NOT FOUND." << std::endl;
    return false;
  }
  num_subs = (file.Read(subboards,
                                      (GetMaxNumberMessageAreas() * sizeof(subboardrec)))) / sizeof(subboardrec);
  return (read_subs_xtr(GetMaxNumberMessageAreas(), num_subs, subboards));
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
      char szBuffer[255];
      fileNetIni.ReadLine(szBuffer, 80);
      szBuffer[sizeof(szBuffer) - 1] = 0;
      StringRemoveWhitespace(szBuffer);
      if (!strncasecmp(szBuffer, "DOMAIN=", 7) && internetEmailDomain.empty()) {
        internetEmailDomain = &(szBuffer[7]);
      } else if (!strncasecmp(szBuffer, "POPNAME=", 8) && internetEmailName.empty()) {
        internetEmailName = &(szBuffer[8]);
      } else if (!strncasecmp(szBuffer, "FWDDOM=", 7)) {
        internetEmailDomain = &(szBuffer[7]);
      } else if (!strncasecmp(szBuffer, "FWDNAME=", 8)) {
        internetEmailName = &(szBuffer[8]);
      } else if (!strncasecmp(szBuffer, "POPDOMAIN=", 10)) {
        internetPopDomain = &(szBuffer[10]);
      } else if (!strncasecmp(szBuffer, "REALNAME=", 9) &&
                 (szBuffer[9] == 'Y' || szBuffer[9] == 'y')) {
        SetInternetUseRealNames(true);
      }
    }
    fileNetIni.Close();
  }
  if (net_networks) {
    free(net_networks);
  }
  net_networks = nullptr;

  File networksfile(syscfg.datadir, NETWORKS_DAT);
  if (networksfile.Open(File::modeBinary | File::modeReadOnly)) {
    int net_num_max = networksfile.GetLength() / sizeof(net_networks_rec_disk);
    SetMaxNetworkNumber(net_num_max);
    std::unique_ptr<net_networks_rec_disk[]> net_networks_disk(new net_networks_rec_disk[net_num_max]());
    net_networks = static_cast<net_networks_rec *>(BbsAllocA(net_num_max * sizeof(net_networks_rec)));
    if (net_num_max) {
      networksfile.Read(net_networks_disk.get(), net_num_max * sizeof(net_networks_rec_disk));
      for (int i = 0; i < net_num_max; i++) {
        net_networks[i].type = net_networks_disk[i].type;
        strcpy(net_networks[i].name, net_networks_disk[i].name);
        strcpy(net_networks[i].dir, net_networks_disk[i].dir);
        net_networks[i].sysnum = net_networks_disk[i].sysnum;
      }
    }
    networksfile.Close();
    for (int nTempNetNumber = 0; nTempNetNumber < GetMaxNetworkNumber(); nTempNetNumber++) {
      char* ss = strchr(net_networks[nTempNetNumber].name, ' ');
      if (ss) {
        *ss = '\0';
      }
    }
  }

  if (!net_networks) {
    net_networks = static_cast<net_networks_rec *>(BbsAllocA(sizeof(net_networks_rec)));
    SetMaxNetworkNumber(1);
    strcpy(net_networks->name, "WWIVnet");
    strcpy(net_networks->dir, syscfg.datadir);
    net_networks->sysnum = syscfg.systemnumber;
  }
}

bool WSession::read_names() {
  if (smallist) {
    free(smallist);
  }

  int maxNumberOfUsers = std::max<int>(statusMgr->GetUserCount(), syscfg.maxusers);

  smallist = static_cast<smalrec *>(BbsAllocA(static_cast<long>(maxNumberOfUsers) * static_cast<long>(sizeof(smalrec))));

  File file(syscfg.datadir, NAMES_LST);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    std::clog << file.GetName() << " NOT FOUND." << std::endl;
    return false;
  }
  file.Read(smallist, maxNumberOfUsers * sizeof(smalrec));
  file.Close();
  return true;
}

void WSession::read_voting() {
  for (int nTempQuestionNumber = 0; nTempQuestionNumber < 20; nTempQuestionNumber++) {
    questused[ nTempQuestionNumber ] = 0;
  }

  File file(syscfg.datadir, VOTING_DAT);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    int n = static_cast<int>(file.GetLength() / sizeof(votingrec)) - 1;
    for (int nTempQuestUsedNum = 0; nTempQuestUsedNum < n; nTempQuestUsedNum++) {
      votingrec v;
      file.Seek(static_cast<long>(nTempQuestUsedNum) * sizeof(votingrec), File::seekBegin);
      file.Read(&v, sizeof(votingrec));
      if (v.numanswers) {
        questused[ nTempQuestUsedNum ] = 1;
      }
    }
  }
}

bool WSession::read_dirs() {
  if (directories) {
    free(directories);
  }
  directories = nullptr;
  SetMaxNumberFileAreas(syscfg.max_dirs);
  directories = static_cast<directoryrec *>(BbsAllocA(static_cast<long>(GetMaxNumberFileAreas()) *
                static_cast<long>(sizeof(directoryrec))));

  File file(syscfg.datadir, DIRS_DAT);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    std::clog << file.GetName() << " NOT FOUND." << std::endl;
    return false;
  }
  num_dirs = file.Read(directories,
                                     (sizeof(directoryrec) * GetMaxNumberFileAreas())) / sizeof(directoryrec);
  return true;
}

void WSession::read_chains() {
  chains.clear();
  DataFile<chainfilerec> file(syscfg.datadir, CHAINS_DAT);
  if (!file) {
    return;
  }
  int num_editors = std::min(
    static_cast<int>(max_chains), file.number_of_records());
  chains.resize(num_editors);
  file.Read(&chains[0], num_editors);

  if (HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
    chains_reg.clear();

    DataFile<chainregrec> regFile(syscfg.datadir, CHAINS_REG);
    if (regFile) {
      int num_chains = std::min<int>(max_chains, regFile.number_of_records());
      chains_reg.resize(num_chains);
      regFile.Read(&chains_reg[0], num_chains);
    } else {
      regFile.Close();
      for (size_t nTempChainNum = 0; nTempChainNum < chains.size(); nTempChainNum++) {
        chainregrec reg;
        memset(&reg, 0, sizeof(chainregrec));
        reg.maxage = 255;
        chains_reg.push_back(reg);
      }
      DataFile<chainregrec> writeFile(syscfg.datadir, CHAINS_REG, 
          File::modeReadWrite | File::modeBinary | File::modeCreateFile);
      writeFile.Write(&chains_reg[0], chains_reg.size());
    }
  }
}

bool WSession::read_language() {
  if (languages) {
    free(languages);
  }
  languages = nullptr;
  File file(syscfg.datadir, LANGUAGE_DAT);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    num_languages = file.GetLength() / sizeof(languagerec);
    if (num_languages) {
      languages = static_cast<languagerec *>(BbsAllocA(num_languages * sizeof(languagerec)));
      file.Read(languages, num_languages * sizeof(languagerec));
    }
    file.Close();
  }
  if (!num_languages) {
    languages = static_cast<languagerec *>(BbsAllocA(sizeof(languagerec)));
    num_languages = 1;
    strcpy(languages->name, "English");
    strncpy(languages->dir, syscfg.gfilesdir, sizeof(languages->dir) - 1);
    strncpy(languages->mdir, syscfg.menudir, sizeof(languages->mdir) - 1);
  }
  SetCurrentLanguageNumber(-1);
  if (!set_language(0)) {
    std::clog << "You need the default language installed to run the BBS." << std::endl;
    return false;
  }
  return true;
}

void WSession::read_gfile() {
  if (gfilesec != nullptr) {
    free(gfilesec);
    gfilesec = nullptr;
  }
  gfilesec = static_cast<gfiledirrec *>(BbsAllocA(static_cast<long>(max_gfilesec * sizeof(gfiledirrec))));
  File file(syscfg.datadir, GFILE_DAT);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    num_sec = 0;
  } else {
    num_sec = file.Read(gfilesec, max_gfilesec * sizeof(gfiledirrec)) / sizeof(gfiledirrec);
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

void WSession::InitializeBBS() {
  localIO()->SetScreenBottom(localIO()->GetDefaultScreenBottom());
  defscreenbottom = localIO()->GetDefaultScreenBottom();
  // set default screenlinest
  screenlinest = defscreenbottom + 1;

  localIO()->LocalCls();
#if !defined( __unix__ )
  std::clog << std::endl << wwiv_version << beta_version << ", Copyright (c) 1998-2015, WWIV Software Services."
            << std::endl << std::endl
            << "\r\nInitializing BBS..." << std::endl;
#endif // __unix__
  SetCurrentReadMessageArea(-1);
  use_workspace = false;
  chat_file = false;
  localIO()->SetSysopAlert(false);
  nsp = 0;
  capture()->set_global_handle(false, true);
  bquote = 0;
  equote = 0;
  SetQuoting(false);
  tagptr = 0;

  if (!ReadConfig()) {
    AbortBBS(true);
  }

  XINIT_PRINTF("Processing configuration file: WWIV.INI.");
  if (!File::Exists(syscfgovr.tempdir)) {
    if (!File::mkdirs(syscfgovr.tempdir)) {
      std::clog << std::endl << "Your temp dir isn't valid." << std::endl;
      std::clog << "It is now set to: '" << syscfgovr.tempdir << "'" << std::endl;
      AbortBBS();
    }
  }

  if (!File::Exists(syscfgovr.batchdir)) {
    if (!File::mkdirs(syscfgovr.batchdir)) {
      std::clog << std::endl << "Your batch dir isn't valid." << std::endl;
      std::clog << "It is now set to: '" << syscfgovr.batchdir << "'" << std::endl;
      AbortBBS();
    }
  }

  write_inst(INST_LOC_INIT, 0, INST_FLAGS_NONE);

  // make sure it is the new USERREC structure
  XINIT_PRINTF("Reading user scan pointers.");
  File fileQScan(syscfg.datadir, USER_QSC);
  if (!fileQScan.Exists()) {
    std::clog << "Could not open file '" << fileQScan.full_pathname() << "'" << std::endl;
    std::clog << "You must go into INIT and convert your userlist before running the BBS." << std::endl;
    AbortBBS();
  }

  if (!syscfgovr.primaryport) {
    // On all platforms primaryport is now 1 (this case should only ever happen
    // on an upgraded system.
    syscfgovr.primaryport = 1;
  }

  languages = nullptr;
  if (!read_language()) {
    AbortBBS();
  }

  net_networks = nullptr;
  SetNetworkNumber(0);
  read_networks();
  set_net_num(0);

  XINIT_PRINTF("Reading status information.");
  WStatus* pStatus = statusMgr->BeginTransaction();
  if (!pStatus) {
    std::clog << "Unable to return status.dat." << std::endl;
    AbortBBS();
  }

  XINIT_PRINTF("Reading color information.");
  File fileColor(syscfg.datadir, COLOR_DAT);
  if (!fileColor.Exists()) {
    buildcolorfile();
  }
  get_colordata();

  pStatus->SetWWIVVersion(wwiv_num_version);
  pStatus->EnsureCallerNumberIsValid();
  statusMgr->CommitTransaction(pStatus);

  gat = static_cast<unsigned short *>(BbsAllocA(2048 * sizeof(short)));

  XINIT_PRINTF("Reading Gfiles.");
  gfilesec = nullptr;
  read_gfile();

  smallist = nullptr;

  XINIT_PRINTF("Reading user names.");
  if (!read_names()) {
    AbortBBS();
  }

  XINIT_PRINTF("Reading Message Areas.");
  subboards = nullptr;
  if (!read_subs()) {
    AbortBBS();
  }

  XINIT_PRINTF("Reading File Areas.");
  directories = nullptr;
  if (!read_dirs()) {
    AbortBBS();
  }

  XINIT_PRINTF("Reading Chains.");
  read_chains();

  XINIT_PRINTF("Reading File Transfer Protocols.");
  externs = nullptr;
  read_nextern();

  over_intern = nullptr;
  read_nintern();

  XINIT_PRINTF("Reading File Archivers.");
  read_arcs();
  SaveConfig();

  XINIT_PRINTF("Reading Full Screen Message Editors.");
  read_editors();

  if (!File::mkdirs(m_attachmentDirectory)) {
    std::clog << std::endl << "Your file attachment directory is invalid." << std::endl;
    std::clog << "It is now set to: " << m_attachmentDirectory << "' << std::endl";
    AbortBBS();
  }
  CdHome();

  check_phonenum(); // dupphone addition

  batch = static_cast<batchrec *>(BbsAllocA(max_batch * sizeof(batchrec)));

  XINIT_PRINTF("Reading User Information.");
  ReadCurrentUser(1, false);
  fwaiting = (user()->IsUserDeleted()) ? 0 : user()->GetNumMailWaiting();
  statusMgr->RefreshStatusCache();
  topdata = (syscfg.sysconfig & sysconfig_no_local) ? LocalIO::topdataNone : LocalIO::topdataUser;

  snprintf(g_szDSZLogFileName, sizeof(g_szDSZLogFileName), "%sdsz.log", syscfgovr.tempdir);
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
    set_environment_variable("DSZLOG", g_szDSZLogFileName);
  }
  set_environment_variable("BBS", wwiv_version);

#endif // defined ( __unix__ )

  XINIT_PRINTF("Reading Voting Booth Configuration.");
  read_voting();

  XINIT_PRINTF("Reading External Events.");
  init_events();
  last_time = time_event - timer();
  if (last_time < 0.0) {
    last_time += HOURS_PER_DAY_FLOAT * SECONDS_PER_HOUR_FLOAT;
  }

  XINIT_PRINTF("Allocating Memory for Message/File Areas.");
  do_event = 0;
  usub = static_cast<usersubrec *>(BbsAllocA(GetMaxNumberMessageAreas() * sizeof(usersubrec)));
  udir = static_cast<usersubrec *>(BbsAllocA(GetMaxNumberFileAreas() * sizeof(usersubrec)));
  uconfsub = static_cast<userconfrec *>(BbsAllocA(MAX_CONFERENCES * sizeof(userconfrec)));
  uconfdir = static_cast<userconfrec *>(BbsAllocA(MAX_CONFERENCES * sizeof(userconfrec)));
  qsc = static_cast<uint32_t *>(BbsAllocA(syscfg.qscn_len));
  qsc_n = qsc + 1;
  qsc_q = qsc_n + (GetMaxNumberFileAreas() + 31) / 32;
  qsc_p = qsc_q + (GetMaxNumberMessageAreas() + 31) / 32;

  network_extension = ".net";
  const string wwiv_instance(environment_variable("WWIV_INSTANCE"));
  if (!wwiv_instance.empty()) {
    int nTempInstanceNumber = atoi(wwiv_instance.c_str());
    if (nTempInstanceNumber > 0) {
      network_extension = StringPrintf(".%3.3d", nTempInstanceNumber);
      // Fix... Set the global instance variable to match this.  When you run WWIV with the -n<instance> parameter
      // it sets the WWIV_INSTANCE environment variable, however it wasn't doing the reverse.
      instance_number_ = nTempInstanceNumber;
    }
  }

  read_bbs_list_index();
  frequent_init();
  if (!m_bUserAlreadyOn) {
    TempDisablePause disable_pause;
    remove_from_temp("*.*", syscfgovr.tempdir, true);
    remove_from_temp("*.*", syscfgovr.batchdir, true);
    cleanup_net();
  }
  subconfnum = dirconfnum = 0;

  XINIT_PRINTF("Reading Conferences.");
  read_all_conferences();

  if (!m_bUserAlreadyOn) {
    sysoplog("", false);
    sysoplogfi(false, "WWIV %s%s, inst %ld, brought up at %s on %s.", wwiv_version, beta_version, 
        GetInstanceNumber(), times(), fulldate());
  }
  if (GetInstanceNumber() > 1) {
    char szFileName[MAX_PATH];
    snprintf(szFileName, sizeof(szFileName), "%s.%3.3u", WWIV_NET_NOEXT, GetInstanceNumber());
    File::Remove(szFileName);
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
  File phoneFile(syscfg.datadir, PHONENUM_DAT);
  if (!phoneFile.Exists()) {
    create_phone_file();
  }
}

void WSession::create_phone_file() {
  phonerec p;

  File file(syscfg.datadir, USER_LST);
  if (!file.Open(File::modeReadOnly | File::modeBinary)) {
    return;
  }
  long lFileSize = file.GetLength();
  file.Close();
  int nNumberOfRecords = static_cast<int>(lFileSize / sizeof(userrec));

  File phoneNumFile(syscfg.datadir, PHONENUM_DAT);
  if (!phoneNumFile.Open(File::modeReadWrite | File::modeAppend | File::modeBinary | File::modeCreateFile)) {
    return;
  }

  for (int16_t nTempUserNumber = 1; nTempUserNumber <= nNumberOfRecords; nTempUserNumber++) {
    WUser user;
    users()->ReadUser(&user, nTempUserNumber);
    if (!user.IsUserDeleted()) {
      p.usernum = nTempUserNumber;
      char szTempVoiceNumber[ 255 ], szTempDataNumber[ 255 ];
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

uint32_t GetFlagsFromIniFile(IniFile *pIniFile, ini_flags_type * fs, int nFlagNumber, uint32_t flags) {
  for (int i = 0; i < nFlagNumber; i++) {
    const char* ss = INI_OPTIONS_ARRAY[ fs[i].strnum ];
    if (ss && pIniFile->GetValue(ss)) {
      if (pIniFile->GetBooleanValue(ss)) {
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
