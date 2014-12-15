/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include <memory>
#include <string>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif  // _WIN32

#include "bbs/wwiv.h"
#include "bbs/datetime.h"
#include "bbs/instmsg.h"
#include "bbs/pause.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "core/inifile.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/wwivassert.h"
#include "core/wwivport.h"

#include "sdk/config.h"

// Additional INI file function and structure
#include "bbs/xinitini.h"

// %%TODO: Turn this warning back on once the rest of the code is cleaned up
#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4244 )
#endif

#ifdef __unix__
#define XINIT_PRINTF( x )
#else
#define XINIT_PRINTF( x ) std::clog << ( x )
#endif // __unix__

struct ini_flags_type {
  int     strnum;
  bool    sense;
  uint32_t value;
};

using std::string;
using wwiv::bbs::TempDisablePause;
using namespace wwiv::core;
using namespace wwiv::strings;

uint32_t GetFlagsFromIniFile(IniFile *pIniFile, ini_flags_type * fs, int nFlagNumber, uint32_t flags);


// Turns a string into a bitmapped unsigned short flag for use with
// ExecuteExternalProgram calls.
unsigned short WApplication::str2spawnopt(const char *s) {
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
unsigned short WApplication::str2restrict(const char *s) {
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

unsigned char WApplication::stryn2tf(const char *s) {
  char ch = wwiv::UpperCase<char>(*s);

  char yesChar = *(YesNoString(true));
  if (ch == yesChar || ch == 1 || ch == '1' || ch == 'T') {
    return 1;
  }
  return 0;
}


// end callback addition


#define OFFOF(x) ( reinterpret_cast<long>( &session()->user()->data.x ) - reinterpret_cast<long>( &session()->user()->data ) )


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


static eventinfo_t eventinfo[] = {
  {"TIMED",         EFLAG_NONE},
  {"NEWUSER",       0},
  {"BEGINDAY",      0},
  {"LOGON",         EFLAG_NONE},
  {"ULCHK",         EFLAG_NOHUP},
  {"FSED",          EFLAG_FOSSIL},  // UNUSED
  {"PROT_SINGLE",   0},
  {"PROT_BATCH",    EFLAG_NONE},
  {"CHAT",          0},
  {"ARCH_E",        EFLAG_NONE},
  {"ARCH_L",        EFLAG_NONE},
  {"ARCH_A",        EFLAG_NONE},
  {"ARCH_D",        EFLAG_NONE},
  {"ARCH_K",        EFLAG_NONE},
  {"ARCH_T",        EFLAG_NONE},
  {"NET_CMD1",      EFLAG_NONE},
  {"NET_CMD2",      EFLAG_NETPROG},
  {"LOGOFF",        EFLAG_NONE},
  {"V_SCAN",        EFLAG_NONE},
  {"NETWORK",       0},
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
{const char* ss; if ((ss=ini->GetValue(get_key_str(INI_STR_SIMPLE_ASV,s)))!=nullptr) session()->asv.f = func (ss); else session()->asv.f = d;}
#define INI_GET_CALLBACK(s, f, func, d) \
{const char* ss; if ((ss=ini->GetValue(get_key_str(INI_STR_CALLBACK,s)))!=nullptr) \
    session()->cbv.f = func (ss); \
    else session()->cbv.f = d;}

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

IniFile* WApplication::ReadINIFile() {
  // Setup default session()-> data
  for (int nTempColorNum = 0; nTempColorNum < 10; nTempColorNum++) {
    session()->newuser_colors[ nTempColorNum ] = nucol[ nTempColorNum ];
    session()->newuser_bwcolors[ nTempColorNum ] = nucolbw[ nTempColorNum ];
  }

  session()->SetTopScreenColor(31);
  session()->SetUserEditorColor(31);
  session()->SetEditLineColor(112);
  session()->SetChatNameSelectionColor(95);
  session()->SetMessageColor(2);
  session()->max_batch = 50;
  session()->max_extend_lines = 10;
  session()->max_chains = 50;
  session()->max_gfilesec = 32;
  session()->mail_who_field_len = 35;
  session()->SetBeginDayNodeNumber(1);
  session()->SetUseInternalZmodem(true);
  session()->SetExecUseWaitForInputIdle(true);
  session()->SetExecWaitForInputTimeout(2000);
  session()->SetExecChildProcessWaitTime(500);
  session()->SetExecLogSyncFoss(true);
  session()->SetNewScanAtLogin(false);

  for (size_t nTempEventNum = 0; nTempEventNum < NEL(eventinfo); nTempEventNum++) {
    application()->spawn_opts[ nTempEventNum ] = eventinfo[ nTempEventNum ].eflags;
  }

  // put in default WApplication::flags
  application()->SetConfigFlags(OP_FLAGS_FIDO_PROCESS);

  if (ok_modem_stuff) {
    application()->SetConfigFlag(OP_FLAGS_NET_CALLOUT);
  } else {
    application()->ClearConfigFlag(OP_FLAGS_NET_CALLOUT);
  }

  // initialize ini communication
  const string instance_name = StringPrintf("WWIV-%u", GetInstanceNumber());
  IniFile* ini(new IniFile(FilePath(application()->GetHomeDir(), WWIV_INI), instance_name, INI_TAG));
  if (ini->IsOpen()) {
    // found something
    // pull out event flags
    for (size_t nTempSpawnOptNum = 0; nTempSpawnOptNum < NEL(application()->spawn_opts); nTempSpawnOptNum++) {
      const string key_name = StringPrintf("%s[%s]", get_key_str(INI_STR_SPAWNOPT), eventinfo[nTempSpawnOptNum].name);
      const char *ss = ini->GetValue(key_name);
      if (ss != nullptr) {
        application()->spawn_opts[nTempSpawnOptNum] = str2spawnopt(ss);
      }
    }

    // pull out newuser colors
    for (int nTempColorNum = 0; nTempColorNum < 10; nTempColorNum++) {
      string key_name = StringPrintf("%s[%d]", get_key_str(INI_STR_NUCOLOR), nTempColorNum);
      const char *ss = ini->GetValue(key_name);
      if (ss != nullptr && atoi(ss) > 0) {
        session()->newuser_colors[nTempColorNum] = atoi(ss);
      }
      key_name = StringPrintf("%s[%d]", get_key_str(INI_STR_NUCOLOR), nTempColorNum);
      ss = ini->GetValue(key_name);
      if (ss != nullptr && atoi(ss) > 0) {
        session()->newuser_bwcolors[nTempColorNum] = atoi(ss);
      }
    }

    session()->SetMessageThreadingEnabled(ini->GetBooleanValue("THREAD_SUBS"));
    session()->SetCarbonCopyEnabled(ini->GetBooleanValue("ALLOW_CC_BCC"));

    // pull out sysop-side colors
    session()->SetTopScreenColor(ini->GetNumericValue(get_key_str(INI_STR_TOPCOLOR),
                                    session()->GetTopScreenColor()));
    session()->SetUserEditorColor(ini->GetNumericValue(get_key_str(INI_STR_F1COLOR),
                                     session()->GetUserEditorColor()));
    session()->SetEditLineColor(ini->GetNumericValue(get_key_str(INI_STR_EDITLINECOLOR),
                                   session()->GetEditLineColor()));
    session()->SetChatNameSelectionColor(ini->GetNumericValue(get_key_str(INI_STR_CHATSELCOLOR),
                                            session()->GetChatNameSelectionColor()));

    // pull out sizing options
    session()->max_batch = ini->GetNumericValue(get_key_str(INI_STR_MAX_BATCH), session()->max_batch);
    session()->max_extend_lines = ini->GetNumericValue(get_key_str(INI_STR_MAX_EXTEND_LINES),
                                     session()->max_extend_lines);
    session()->max_chains = ini->GetNumericValue(get_key_str(INI_STR_MAX_CHAINS), session()->max_chains);
    session()->max_gfilesec = ini->GetNumericValue(get_key_str(INI_STR_MAX_GFILESEC), session()->max_gfilesec);

    // pull out strings
    INI_INIT_STR(INI_STR_UPLOAD_CMD, upload_cmd);
    INI_INIT_STR(INI_STR_BEGINDAY_CMD, beginday_cmd);
    INI_INIT_STR(INI_STR_NEWUSER_CMD, newuser_cmd);
    INI_INIT_STR(INI_STR_LOGON_CMD, logon_cmd);

    session()->m_nForcedReadSubNumber = ini->GetNumericValue(get_key_str(INI_STR_FORCE_SCAN_SUBNUM),
                                           session()->m_nForcedReadSubNumber);
    session()->m_bInternalZmodem = ini->GetBooleanValue(get_key_str(INI_STR_INTERNALZMODEM),
                                      session()->m_bInternalZmodem);
    session()->m_bNewScanAtLogin = ini->GetBooleanValue(get_key_str(INI_STR_NEW_SCAN_AT_LOGIN),
                                      session()->m_bNewScanAtLogin);

    session()->m_bExecLogSyncFoss = ini->GetBooleanValue(get_key_str(INI_STR_EXEC_LOG_SYNCFOSS),
                                       session()->m_bExecLogSyncFoss);
    session()->m_bExecUseWaitForInputIdle = ini->GetBooleanValue(get_key_str(INI_STR_EXEC_USE_WAIT_FOR_IDLE),
        session()->m_bExecUseWaitForInputIdle);
    session()->m_nExecChildProcessWaitTime = ini->GetNumericValue(get_key_str(INI_STR_EXEC_CHILD_WAIT_TIME),
        session()->m_nExecChildProcessWaitTime);
    session()->m_nExecUseWaitForInputTimeout = ini->GetNumericValue(get_key_str(INI_STR_EXEC_WAIT_FOR_IDLE_TIME),
        session()->m_nExecUseWaitForInputTimeout);

    session()->SetBeginDayNodeNumber(ini->GetNumericValue(get_key_str(INI_STR_BEGINDAYNODENUMBER),
                                        session()->GetBeginDayNodeNumber()));

    // pull out sysinfo_flags
    application()->SetConfigFlags(GetFlagsFromIniFile(ini, sysinfo_flags, NEL(sysinfo_flags),
                                     application()->GetConfigFlags()));

    // allow override of WSession::m_nMessageColor
    session()->SetMessageColor(ini->GetNumericValue(get_key_str(INI_STR_MSG_COLOR), session()->GetMessageColor()));

    // get asv values
    if (application()->HasConfigFlag(OP_FLAGS_SIMPLE_ASV)) {
      INI_GET_ASV("SL", sl, atoi, syscfg.autoval[9].sl);
      INI_GET_ASV("DSL", dsl, atoi, syscfg.autoval[9].dsl);
      INI_GET_ASV("EXEMPT", exempt, atoi, 0);
      INI_GET_ASV("AR", ar, str_to_arword, syscfg.autoval[9].ar);
      INI_GET_ASV("DAR", dar, str_to_arword, syscfg.autoval[9].dar);
      INI_GET_ASV("RESTRICT", restrict, str2restrict, 0);
    }
    if (application()->HasConfigFlag(OP_FLAGS_ADV_ASV)) {
      session()->advasv.reg_wwiv = ini->GetNumericValue(get_key_str(INI_STR_ADVANCED_ASV, "REG_WWIV"), 1);
      session()->advasv.nonreg_wwiv = ini->GetNumericValue(get_key_str(INI_STR_ADVANCED_ASV, "NONREG_WWIV"), 1);
      session()->advasv.non_wwiv = ini->GetNumericValue(get_key_str(INI_STR_ADVANCED_ASV, "NON_WWIV"), 1);
      session()->advasv.cosysop = ini->GetNumericValue(get_key_str(INI_STR_ADVANCED_ASV, "COSYSOP"), 1);
    }

    // get callback values
    if (application()->HasConfigFlag(OP_FLAGS_CALLBACK)) {
      INI_GET_CALLBACK("SL", sl, atoi, syscfg.autoval[2].sl);
      INI_GET_CALLBACK("DSL", dsl, atoi, syscfg.autoval[2].dsl);
      INI_GET_CALLBACK("EXEMPT", exempt, atoi, 0);
      INI_GET_CALLBACK("AR", ar, str_to_arword, syscfg.autoval[2].ar);
      INI_GET_CALLBACK("DAR", dar, str_to_arword, syscfg.autoval[2].dar);
      INI_GET_CALLBACK("RESTRICT", restrict, str2restrict, 0);
      INI_GET_CALLBACK("FORCED", forced, stryn2tf, 0);
      INI_GET_CALLBACK("LONG_DISTANCE", longdistance, stryn2tf, 0);
      INI_GET_CALLBACK("REPEAT", repeat, atoi, 0);
    }

    // sysconfig flags
    syscfg.sysconfig = static_cast<unsigned short>(GetFlagsFromIniFile(ini, sysconfig_flags,
                       NEL(sysconfig_flags), syscfg.sysconfig));

    const char* ss;
    // misc stuff
    if (((ss = ini->GetValue(get_key_str(INI_STR_MAIL_WHO_LEN))) != nullptr) &&
        (atoi(ss) > 0 || ss[0] == '0')) {
      session()->mail_who_field_len = atoi(ss);
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

    session()->screen_saver_time = ini->GetNumericValue(get_key_str(INI_STR_SCREEN_SAVER_TIME),
                                      session()->screen_saver_time);
  }

  session()->max_extend_lines    = std::min<unsigned short>(session()->max_extend_lines, 99);
  session()->max_batch           = std::min<unsigned short>(session()->max_batch , 999);
  session()->max_chains          = std::min<unsigned short>(session()->max_chains, 999);
  session()->max_gfilesec        = std::min<unsigned short>(session()->max_gfilesec, 999);

  // can't allow user to change these on-the-fly
  unsigned short omb = session()->max_batch;
  if (omb) {
    session()->max_batch = omb;
  }
  unsigned short omc = session()->max_chains;
  if (omc) {
    session()->max_chains = omc;
  }
  unsigned short omg = session()->max_gfilesec;
  if (omg) {
    session()->max_gfilesec = omg;
  }

  session()->set_wwivmail_enabled(ini->GetBooleanValue("USE_WWIVMAIL", true));
  session()->set_internal_qwk_enabled(ini->GetBooleanValue("USE_INTERNAL_QWK", true));
  return ini;
}

bool WApplication::ReadConfigOverlayFile(int instance_number, configrec* full_syscfg, IniFile* ini) {
  const char* temp_directory_char = ini->GetValue("TEMP_DIRECTORY");
  if (temp_directory_char != nullptr) {
    string temp_directory(temp_directory_char);
    // TEMP_DIRECTORY is defined in wwiv.ini, therefore use it over config.ovr, also 
    // default the batch_directory to TEMP_DIRECTORY if BATCH_DIRECTORY does not exist.
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
      std::clog << "Not enough instances configured.\r\n";
      return false;
    }

    return true;
  }

  // Read the legacy config.ovr file.
  File configOvrFile(CONFIG_OVR);
  bool bIsConfigObvOpen = configOvrFile.Open(File::modeBinary | File::modeReadOnly);
  if (bIsConfigObvOpen &&
      configOvrFile.GetLength() < static_cast<long>(instance_number * sizeof(configoverrec))) {
    configOvrFile.Close();
    std::clog << "Not enough instances configured.\r\n";
    return false;
  }
  if (!bIsConfigObvOpen) {
    syscfgovr.primaryport = 1;
    strcpy(syscfgovr.tempdir, full_syscfg->tempdir);
    strcpy(syscfgovr.batchdir, full_syscfg->batchdir);
  } else {
    long lCurNodeOffset = (instance_number - 1) * sizeof(configoverrec);
    configOvrFile.Seek(lCurNodeOffset, File::seekBegin);
    configOvrFile.Read(&syscfgovr, sizeof(configoverrec));
    configOvrFile.Close();
  }
  return true;
}

static char* DuplicatePath(const char* path) {
  char* out = new char[MAX_PATH + 1];
  strncpy(out, path, MAX_PATH);
  return out;
}

bool WApplication::ReadConfig() {
  //configrec full_syscfg;
  wwiv::sdk::Config full_config;
  if (!full_config.IsInitialized()) {
    std::clog << CONFIG_DAT << " NOT FOUND.\r\n";
    return false;
  }

  // initialize the user manager
  users()->InitializeUserManager(full_config.config()->datadir, full_config.config()->userreclen, full_config.config()->maxusers);

  std::unique_ptr<IniFile> ini(ReadINIFile());
  if (!ini->IsOpen()) {
    std::clog << "Insufficient memory for system info structure.\r\n";
    AbortBBS();
  }

  bool config_ovr_read = ReadConfigOverlayFile(GetInstanceNumber(), full_config.config(), ini.get());
  if (!config_ovr_read) {
    return false;
  }

  // update user info data
  int userreclen = sizeof(userrec);
  int waitingoffset = OFFOF(waiting);
  int inactoffset = OFFOF(inact);
  int sysstatusoffset = OFFOF(sysstatus);
  int fuoffset = OFFOF(forwardusr);
  int fsoffset = OFFOF(forwardsys);
  int fnoffset = OFFOF(net_num);

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
  syscfg.batchdir         = DuplicatePath(full_config.config()->batchdir);
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
  syscfg.netlowtime       = full_config.config()->netlowtime;
  syscfg.nethightime      = full_config.config()->nethightime;
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

bool WApplication::SaveConfig() {
  File configFile(CONFIG_DAT);
  if (configFile.Open(File::modeBinary | File::modeReadWrite)) {
    configrec full_syscfg;
    configFile.Read(&full_syscfg, sizeof(configrec));
    // Should these move to wwiv.ini?
    // full_config.config()->post_call_ratio = syscfg.post_call_ratio;
    // full_syscfg.newusergold     = syscfg.newusergold;

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


void WApplication::read_nextern() {
  session()->SetNumberOfExternalProtocols(0);
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
    WWIV_ASSERT(externs != nullptr);
    session()->SetNumberOfExternalProtocols(externalFile.Read(externs, lFileSize) / sizeof(newexternalrec));
  }
}


void WApplication::read_arcs() {
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
    WWIV_ASSERT(arcs != nullptr);
  }
}


void WApplication::read_editors() {
  if (editors) {
    free(editors);
    editors = nullptr;
  }
  session()->SetNumberOfEditors(0);

  File file(syscfg.datadir, EDITORS_DAT);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    unsigned long lFileSize = file.GetLength();
    if (lFileSize > 10 * sizeof(editorrec)) {
      lFileSize = 10 * sizeof(editorrec);
    }
    editors = static_cast<editorrec *>(BbsAllocA(lFileSize + 10));
    WWIV_ASSERT(editors != nullptr);
    session()->SetNumberOfEditors(file.Read(editors, lFileSize) / sizeof(editorrec));
  }
}


void WApplication::read_nintern() {
  if (over_intern) {
    free(over_intern);
    over_intern = nullptr;
  }
  File file(syscfg.datadir, NINTERN_DAT);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    over_intern = static_cast<newexternalrec *>(BbsAllocA(3 * sizeof(newexternalrec)));
    WWIV_ASSERT(over_intern != nullptr);

    file.Read(over_intern, 3 * sizeof(newexternalrec));
  }
}


bool WApplication::read_subs() {
  if (subboards) {
    free(subboards);
  }
  subboards = nullptr;
  session()->SetMaxNumberMessageAreas(syscfg.max_subs);
  subboards = static_cast< subboardrec * >(BbsAllocA(session()->GetMaxNumberMessageAreas() * sizeof(subboardrec)));
  WWIV_ASSERT(subboards != nullptr);

  File file(syscfg.datadir, SUBS_DAT);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    std::clog << file.GetName() << " NOT FOUND.\r\n";
    return false;
  }
  session()->num_subs = (file.Read(subboards,
                                      (session()->GetMaxNumberMessageAreas() * sizeof(subboardrec)))) / sizeof(subboardrec);
  return (read_subs_xtr(session()->GetMaxNumberMessageAreas(), session()->num_subs, subboards));
}


void WApplication::read_networks() {
  session()->internetEmailName = "";
  session()->internetEmailDomain = "";
  session()->internetPopDomain = "";
  session()->SetInternetUseRealNames(false);

  // TODO: wire up for NetX
  TextFile fileNetIni("NET.INI", "rt");
  if (fileNetIni.IsOpen()) {
    while (!fileNetIni.IsEndOfFile()) {
      char szBuffer[ 255 ];
      fileNetIni.ReadLine(szBuffer, 80);
      szBuffer[sizeof(szBuffer) - 1] = 0;
      StringRemoveWhitespace(szBuffer);
      if (!strncasecmp(szBuffer, "DOMAIN=", 7) && session()->internetEmailDomain.empty()) {
        session()->internetEmailDomain = &(szBuffer[7]);
      } else if (!strncasecmp(szBuffer, "POPNAME=", 8) && session()->internetEmailName.empty()) {
        session()->internetEmailName = &(szBuffer[8]);
      } else if (!strncasecmp(szBuffer, "FWDDOM=", 7)) {
        session()->internetEmailDomain = &(szBuffer[7]);
      } else if (!strncasecmp(szBuffer, "FWDNAME=", 8)) {
        session()->internetEmailName = &(szBuffer[8]);
      } else if (!strncasecmp(szBuffer, "POPDOMAIN=", 10)) {
        session()->internetPopDomain = &(szBuffer[10]);
      } else if (!strncasecmp(szBuffer, "REALNAME=", 9) &&
                 (szBuffer[9] == 'Y' || szBuffer[9] == 'y')) {
        session()->SetInternetUseRealNames(true);
      }
    }
    fileNetIni.Close();
  }
  if (net_networks) {
    free(net_networks);
  }
  net_networks = nullptr;
  File file(syscfg.datadir, NETWORKS_DAT);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    session()->SetMaxNetworkNumber(file.GetLength() / sizeof(net_networks_rec));
    if (session()->GetMaxNetworkNumber()) {
      net_networks = static_cast<net_networks_rec *>(BbsAllocA(session()->GetMaxNetworkNumber() * sizeof(
                       net_networks_rec)));
      WWIV_ASSERT(net_networks != nullptr);

      file.Read(net_networks, session()->GetMaxNetworkNumber() * sizeof(net_networks_rec));
    }
    file.Close();
    for (int nTempNetNumber = 0; nTempNetNumber < session()->GetMaxNetworkNumber(); nTempNetNumber++) {
      char* ss = strchr(net_networks[nTempNetNumber].name, ' ');
      if (ss) {
        *ss = '\0';
      }
    }
  }
  if (!net_networks) {
    net_networks = static_cast<net_networks_rec *>(BbsAllocA(sizeof(net_networks_rec)));
    WWIV_ASSERT(net_networks != nullptr);
    session()->SetMaxNetworkNumber(1);
    strcpy(net_networks->name, "WWIVnet");
    strcpy(net_networks->dir, syscfg.datadir);
    net_networks->sysnum = syscfg.systemnumber;
  }
}


bool WApplication::read_names() {
  if (smallist) {
    free(smallist);
  }

  int maxNumberOfUsers = std::max<int>(statusMgr->GetUserCount(), syscfg.maxusers);

  smallist = static_cast<smalrec *>(BbsAllocA(static_cast<long>(maxNumberOfUsers) * static_cast<long>(sizeof(smalrec))));
  WWIV_ASSERT(smallist != nullptr);

  File file(syscfg.datadir, NAMES_LST);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    std::clog << file.GetName() << " NOT FOUND.\r\n";
    return false;
  }
  file.Read(smallist, maxNumberOfUsers * sizeof(smalrec));
  file.Close();
  return true;
}


void WApplication::read_voting() {
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


bool WApplication::read_dirs() {
  if (directories) {
    free(directories);
  }
  directories = nullptr;
  session()->SetMaxNumberFileAreas(syscfg.max_dirs);
  directories = static_cast<directoryrec *>(BbsAllocA(static_cast<long>(session()->GetMaxNumberFileAreas()) *
                static_cast<long>(sizeof(directoryrec))));
  WWIV_ASSERT(directories != nullptr);

  File file(syscfg.datadir, DIRS_DAT);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    std::clog << file.GetName() << " NOT FOUND.\r\n";
    return false;
  }
  session()->num_dirs = file.Read(directories,
                                     (sizeof(directoryrec) * session()->GetMaxNumberFileAreas())) / sizeof(directoryrec);
  return true;
}


void WApplication::read_chains() {
  if (chains) {
    free(chains);
  }
  chains = nullptr;
  chains = static_cast<chainfilerec *>(BbsAllocA(session()->max_chains * sizeof(chainfilerec)));
  WWIV_ASSERT(chains != nullptr);
  File file(syscfg.datadir, CHAINS_DAT);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    session()->SetNumberOfChains(file.Read(chains,
                                    session()->max_chains * sizeof(chainfilerec)) / sizeof(chainfilerec));
  }
  file.Close();
  if (application()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
    if (chains_reg) {
      free(chains_reg);
    }
    chains_reg = nullptr;
    chains_reg = static_cast<chainregrec *>(BbsAllocA(session()->max_chains * sizeof(chainregrec)));
    WWIV_ASSERT(chains_reg != nullptr);

    File regFile(syscfg.datadir, CHAINS_REG);
    if (regFile.Open(File::modeBinary | File::modeReadOnly)) {
      regFile.Read(chains_reg, session()->max_chains * sizeof(chainregrec));
    } else {
      for (int nTempChainNum = 0; nTempChainNum < session()->GetNumberOfChains(); nTempChainNum++) {
        for (size_t nTempRegByNum = 0;
             nTempRegByNum < sizeof(chains_reg[ nTempChainNum ].regby) / sizeof(chains_reg[ nTempChainNum ].regby[0]);
             nTempRegByNum++) {
          chains_reg[ nTempChainNum ].regby[ nTempRegByNum ] = 0;
        }
        chains_reg[ nTempChainNum ].usage   = 0;
        chains_reg[ nTempChainNum ].minage  = 0;
        chains_reg[ nTempChainNum ].maxage  = 255;
      }
      regFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
      regFile.Write(chains_reg , sizeof(chainregrec) * session()->GetNumberOfChains());
    }
    regFile.Close();
  }
}


bool WApplication::read_language() {
  if (languages) {
    free(languages);
  }
  languages = nullptr;
  File file(syscfg.datadir, LANGUAGE_DAT);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    session()->num_languages = file.GetLength() / sizeof(languagerec);
    if (session()->num_languages) {
      languages = static_cast<languagerec *>(BbsAllocA(session()->num_languages * sizeof(languagerec)));
      WWIV_ASSERT(languages != nullptr);

      file.Read(languages, session()->num_languages * sizeof(languagerec));
    }
    file.Close();
  }
  if (!session()->num_languages) {
    languages = static_cast<languagerec *>(BbsAllocA(sizeof(languagerec)));
    WWIV_ASSERT(languages != nullptr);
    session()->num_languages = 1;
    strcpy(languages->name, "English");
    strncpy(languages->dir, syscfg.gfilesdir, sizeof(languages->dir) - 1);
    strncpy(languages->mdir, syscfg.menudir, sizeof(languages->mdir) - 1);
  }
  session()->SetCurrentLanguageNumber(-1);
  if (!set_language(0)) {
    std::clog << "You need the default language installed to run the BBS.\r\n";
    return false;
  }
  return true;
}


void WApplication::read_gfile() {
  if (gfilesec != nullptr) {
    free(gfilesec);
    gfilesec = nullptr;
  }
  gfilesec = static_cast<gfiledirrec *>(BbsAllocA(static_cast<long>(session()->max_gfilesec * sizeof(gfiledirrec))));
  WWIV_ASSERT(gfilesec != nullptr);
  File file(syscfg.datadir, GFILE_DAT);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    session()->num_sec = 0;
  } else {
    session()->num_sec = file.Read(gfilesec, session()->max_gfilesec * sizeof(gfiledirrec)) / sizeof(gfiledirrec);
  }
}

/**
 * Makes a path into an absolute path, returns true if original path altered,
 * else returns false
 */
void WApplication::make_abs_path(char *pszDirectory) {
  string base(GetHomeDir());
  string dir(pszDirectory);
  File::MakeAbsolutePath(base, &dir);
  strcpy(pszDirectory, dir.c_str());
}

void WApplication::InitializeBBS() {
  std::string newprompt;

  session()->localIO()->SetScreenBottom(session()->localIO()->GetDefaultScreenBottom());
  defscreenbottom = session()->localIO()->GetDefaultScreenBottom();

  session()->localIO()->LocalCls();
#if !defined( __unix__ )
  std::clog << std::endl << wwiv_version << beta_version << ", Copyright (c) 1998-2014, WWIV Software Services.\r\n\n";
  std::clog << "\r\nInitializing BBS...\r\n";
#endif // __unix__
  session()->SetCurrentReadMessageArea(-1);
  use_workspace = false;
  chat_file = false;
  session()->localIO()->SetSysopAlert(false);
  nsp = 0;
  session()->localIO()->set_global_handle(false, true);
  bquote = 0;
  equote = 0;
  session()->SetQuoting(false);
  session()->tagptr = 0;

  time_t t = time(nullptr);
  // Struct tm_year is -= 1900
  struct tm * pTm = localtime(&t);
  if (pTm->tm_year < 88) {
    std::clog << "You need to set the date [" << pTm->tm_year << "] & time before running the BBS.\r\n";
    AbortBBS();
  }

  if (!ReadConfig()) {
    AbortBBS(true);
  }

  XINIT_PRINTF("* Processing configuration file: WWIV.INI.\r\n");

  if (syscfgovr.tempdir[0] == '\0') {
    std::clog << "\r\nYour temp dir isn't valid.\r\n";
    std::clog << "It is empty\r\n\n";
    AbortBBS();
  }

  if (syscfgovr.batchdir[0] == '\0') {
    std::clog << "\r\nYour batch dir isn't valid.\r\n";
    std::clog << "It is empty\r\n\n";
    AbortBBS();
  }

  if (!File::Exists(syscfgovr.tempdir)) {
    if (!File::mkdirs(syscfgovr.tempdir)) {
      std::clog << "\r\nYour temp dir isn't valid.\r\n";
      std::clog << "It is now set to: '" << syscfgovr.tempdir << "'\r\n\n";
      AbortBBS();
    }
  }

  if (!File::Exists(syscfgovr.batchdir)) {
    if (!File::mkdirs(syscfgovr.batchdir)) {
      std::clog << "\r\nYour batch dir isn't valid.\r\n";
      std::clog << "It is now set to: '" << syscfgovr.batchdir << "'\r\n\n";
      AbortBBS();
    }
  }

  write_inst(INST_LOC_INIT, 0, INST_FLAGS_NONE);

  // make sure it is the new USERREC structure
  XINIT_PRINTF("* Reading user scan pointers.\r\n");
  File fileQScan(syscfg.datadir, USER_QSC);
  if (!fileQScan.Exists()) {
    std::clog << "Could not open file '" << fileQScan.full_pathname() << "'\r\n";
    std::clog << "You must go into INIT and convert your userlist before running the BBS.\r\n";
    AbortBBS();
  }

#if !defined( __unix__ )
  if (!syscfgovr.primaryport) {
    syscfgovr.primaryport = 1;
  }
#endif // __unix__

  languages = nullptr;
  if (!read_language()) {
    AbortBBS();
  }

  net_networks = nullptr;
  session()->SetNetworkNumber(0);
  read_networks();
  set_net_num(0);

  XINIT_PRINTF("* Reading status information.\r\n");
  WStatus* pStatus = statusMgr->BeginTransaction();
  if (!pStatus) {
    std::clog << "Unable to return status.dat.\r\n";
    AbortBBS();
  }

  XINIT_PRINTF("* Reading color information.\r\n");
  File fileColor(syscfg.datadir, COLOR_DAT);
  if (!fileColor.Exists()) {
    buildcolorfile();
  }
  get_colordata();

  pStatus->SetWWIVVersion(wwiv_num_version);
  pStatus->EnsureCallerNumberIsValid();
  statusMgr->CommitTransaction(pStatus);

  gat = static_cast<unsigned short *>(BbsAllocA(2048 * sizeof(short)));
  WWIV_ASSERT(gat != nullptr);

  XINIT_PRINTF("* Reading Gfiles.\r\n");
  gfilesec = nullptr;
  read_gfile();

  smallist = nullptr;

  XINIT_PRINTF("* Reading user names.\r\n");
  if (!read_names()) {
    AbortBBS();
  }

  XINIT_PRINTF("* Reading Message Areas.\r\n");
  subboards = nullptr;
  if (!read_subs()) {
    AbortBBS();
  }

  XINIT_PRINTF("* Reading File Areas.\r\n");
  directories = nullptr;
  if (!read_dirs()) {
    AbortBBS();
  }

  XINIT_PRINTF("* Reading Chains.\r\n");
  session()->SetNumberOfChains(0);
  chains = nullptr;
  read_chains();

  XINIT_PRINTF("* Reading File Transfer Protocols.\r\n");
  externs = nullptr;
  read_nextern();

  over_intern = nullptr;
  read_nintern();

  XINIT_PRINTF("* Reading File Archivers.\r\n");
  read_arcs();
  SaveConfig();

  XINIT_PRINTF(" * Reading Full Screen Message Editors.\r\n");
  read_editors();

  if (!File::mkdirs(m_attachmentDirectory)) {
    std::clog << "\r\nYour file attachment directory is invalid.\r\n";
    std::clog << "It is now set to: " << m_attachmentDirectory << "'\r\n\n";
    AbortBBS();
  }
  CdHome();

  check_phonenum();                         // dupphone addition

  // allocate sub cache
  iscan1(-1, false);

  batch = static_cast<batchrec *>(BbsAllocA(session()->max_batch * sizeof(batchrec)));
  WWIV_ASSERT(batch != nullptr);

  XINIT_PRINTF("* Reading User Information.\r\n");
  session()->ReadCurrentUser(1, false);
  fwaiting = (session()->user()->IsUserDeleted()) ? 0 : session()->user()->GetNumMailWaiting();

  statusMgr->RefreshStatusCache();

  if (syscfg.sysconfig & sysconfig_no_local) {
    session()->topdata = WLocalIO::topdataNone;
  } else {
    session()->topdata = WLocalIO::topdataUser;
  }
  snprintf(g_szDSZLogFileName, sizeof(g_szDSZLogFileName), "%sWWIVDSZ.%3.3u", GetHomeDir().c_str(), GetInstanceNumber());
  char *ss = getenv("PROMPT");
#if !defined ( __unix__ ) && !defined ( __APPLE__ )
  newprompt = "PROMPT=WWIV: ";

  if (ss) {
    newprompt += ss;
  } else {
    newprompt += "$P$G";
  }
  // put in our environment since passing the xenviron wasn't working
  // with sync emulated fossil
  putenv(newprompt.c_str());

  std::string dszLogEnvironmentVariable("DSZLOG=");
  dszLogEnvironmentVariable.append(g_szDSZLogFileName);
  int pk = 0;
  ss = getenv("DSZLOG");

  if (!ss) {
    putenv(dszLogEnvironmentVariable.c_str());
  }
  if (!pk) {
    putenv("PKNOFASTCHAR=Y");
  }

  wwivVerEnvVar = "BBS=";
  wwivVerEnvVar += wwiv_version;
  putenv(wwivVerEnvVar.c_str());
  putenv(networkNumEnvVar.c_str());
#endif // defined ( __unix__ )

  XINIT_PRINTF("* Reading Voting Booth Configuration.\r\n");
  read_voting();

  XINIT_PRINTF("* Reading External Events.\r\n");
  init_events();
  last_time = time_event - timer();
  if (last_time < 0.0) {
    last_time += HOURS_PER_DAY_FLOAT * SECONDS_PER_HOUR_FLOAT;
  }

  XINIT_PRINTF("* Allocating Memory for Message/File Areas.\r\n");
  do_event = 0;
  usub = static_cast<usersubrec *>(BbsAllocA(session()->GetMaxNumberMessageAreas() * sizeof(usersubrec)));
  WWIV_ASSERT(usub != nullptr);
  session()->m_SubDateCache = static_cast<unsigned int*>(BbsAllocA(session()->GetMaxNumberMessageAreas() * sizeof(
                                   long)));
  WWIV_ASSERT(session()->m_SubDateCache != nullptr);

  udir = static_cast<usersubrec *>(BbsAllocA(session()->GetMaxNumberFileAreas() * sizeof(usersubrec)));
  WWIV_ASSERT(udir != nullptr);
  session()->m_DirectoryDateCache = static_cast<unsigned int*>(BbsAllocA(session()->GetMaxNumberFileAreas() *
                                       sizeof(long)));
  WWIV_ASSERT(session()->m_DirectoryDateCache != nullptr);

  uconfsub = static_cast<userconfrec *>(BbsAllocA(MAX_CONFERENCES * sizeof(userconfrec)));
  WWIV_ASSERT(uconfsub != nullptr);
  uconfdir = static_cast<userconfrec *>(BbsAllocA(MAX_CONFERENCES * sizeof(userconfrec)));
  WWIV_ASSERT(uconfdir != nullptr);

  qsc = static_cast<uint32_t *>(BbsAllocA(syscfg.qscn_len));
  WWIV_ASSERT(qsc != nullptr);
  qsc_n = qsc + 1;
  qsc_q = qsc_n + (session()->GetMaxNumberFileAreas() + 31) / 32;
  qsc_p = qsc_q + (session()->GetMaxNumberMessageAreas() + 31) / 32;

  network_extension = ".NET";
  ss = getenv("WWIV_INSTANCE");
  if (ss) {
    int nTempInstanceNumber = atoi(ss);
    if (nTempInstanceNumber > 0) {
      network_extension = StringPrintf(".%3.3d", nTempInstanceNumber);
      // Fix... Set the global instance variable to match this.  When you run WWIV with the -n<instance> parameter
      // it sets the WWIV_INSTANCE environment variable, however it wasn't doing the reverse.
      instance_number = nTempInstanceNumber;
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

  XINIT_PRINTF("* Reading Conferences.\r\n");
  read_all_conferences();

  if (!m_bUserAlreadyOn) {
    sysoplog("", false);
    sysoplogfi(false, "WWIV %s, inst %ld, brought up at %s on %s.", wwiv_version, GetInstanceNumber(), times(), fulldate());
    sysoplog("", false);
  }
  if (GetInstanceNumber() > 1) {
    char szFileName[MAX_PATH];
    snprintf(szFileName, sizeof(szFileName), "%s.%3.3u", WWIV_NET_NOEXT, GetInstanceNumber());
    File::Remove(szFileName);
  } else {
    File::Remove(WWIV_NET_DAT);
  }

  srand(time(nullptr));
  catsl();

  XINIT_PRINTF("* Saving Instance information.\r\n");
  write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
}



// begin dupphone additions

void WApplication::check_phonenum() {
  File phoneFile(syscfg.datadir, PHONENUM_DAT);
  if (!phoneFile.Exists()) {
    create_phone_file();
  }
}


void WApplication::create_phone_file() {
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

  for (int nTempUserNumber = 1; nTempUserNumber <= nNumberOfRecords; nTempUserNumber++) {
    WUser user;
    application()->users()->ReadUser(&user, nTempUserNumber);
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
          !wwiv::strings::IsEquals(szTempVoiceNumber, szTempDataNumber) &&
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


#if defined( _MSC_VER )
#pragma warning( pop )
#endif // _MSC_VER

// end dupphone additions
