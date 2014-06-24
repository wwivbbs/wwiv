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

#ifndef __INCLUDED_XINITINI_H__
#define __INCLUDED_XINITINI_H__


//---SYSTEM OPTIONS--
#define INI_STR_SPAWNOPT                 2
#define INI_STR_NUCOLOR                  3
#define INI_STR_NUCOLORBW                4
#define INI_STR_TOPCOLOR                 5
#define INI_STR_F1COLOR                  6
#define INI_STR_EDITLINECOLOR            7
#define INI_STR_CHATSELCOLOR             8
#define INI_STR_MSG_COLOR                9

//---SYSTEM COMMANDS--
#define INI_STR_TERMINAL_CMD            14
#define INI_STR_EXECUTE_CMD             15
#define INI_STR_UPLOAD_CMD              16
#define INI_STR_BEGINDAY_CMD            17
#define INI_STR_NEWUSER_CMD             18
#define INI_STR_LOGON_CMD               19
#define INI_STR_LOGOFF_CMD              20
#define INI_STR_V_SCAN_CMD              21

//---NETWORK SETTINGS--
#define INI_STR_NET_CLEANUP_CMD1        25
#define INI_STR_NET_CLEANUP_CMD2        26
#define INI_STR_NET_CALLOUT             27
#define INI_STR_FIDO_PROCESS            28

//---SYSTEM SETTINGS--
#define INI_STR_FORCE_FBACK             33
#define INI_STR_FORCE_NEWUSER           34
#define INI_STR_USE_FORCE_SCAN          35
#define INI_STR_FORCE_SCAN_SUBNUM       36
#define INI_STR_CHECK_DUP_PHONES        37
#define INI_STR_HANGUP_DUP_PHONES       38
#define INI_STR_POSTTIME_COMPENS        39
#define INI_STR_SHOW_HIER               40
#define INI_STR_IDZ_DESC                41
#define INI_STR_SETLDATE                42
//#define INI_STR_NEW_CHATSOUND           43
#define INI_STR_SLASH_SZ                44
#define INI_STR_READ_CD_IDZ             45
#define INI_STR_FSED_EXT_DESC           46
#define INI_STR_FAST_TAG_RELIST         47
#define INI_STR_MAIL_PROMPT             48
#define INI_STR_SHOW_CITY_ST            49
#define INI_STR_LOCAL_SYSOP             50
#define INI_STR_2WAY_CHAT               51
#define INI_STR_NO_NEWUSER_FEEDBACK      52
#define INI_STR_TITLEBAR                53
#define INI_STR_LOG_DOWNLOADS           54
#define INI_STR_CLOSE_XFER              55
#define INI_STR_ALL_UL_TO_SYSOP         56
#define INI_STR_NO_EASY_DL              57
#define INI_STR_NEW_EXTRACT             58
#define INI_STR_FAST_SEARCH             59
#define INI_STR_USER_REGISTRATION       60
#define INI_STR_MSG_TAG                 61
#define INI_STR_CHAIN_REG               62
#define INI_STR_CAN_SAVE_SSM            63
#define INI_STR_EXTRA_COLOR             64
#define INI_STR_BEEP_CHAT               65
#define INI_STR_TWO_COLOR_CHAT          66
#define INI_STR_ALLOW_ALIASES           67
#define INI_STR_USE_LIST                68
#define INI_STR_FREE_PHONE              69
#define INI_STR_EXTENDED_USERINFO       70
#define INI_STR_NEWUSER_MIN             71
#define INI_STR_ALLOW_CC_BCC            72
#define INI_STR_THREAD_SUBS             73
#define INI_STR_DISABLE_PD              74
#define INI_STR_MAIL_WHO_LEN            75
#define INI_STR_ATTACH_DIR              76
#define INI_STR_ENABLE_PIPES            77
#define INI_STR_ENABLE_MCI              78
#define INI_STR_24HR_CLOCK              79          //added Build2


//---SYSTEM MAXIMUMS--
#define INI_STR_MAX_BATCH                84
#define INI_STR_MAX_EXTEND_LINES         85
#define INI_STR_MAX_CHAINS               86
#define INI_STR_MAX_GFILESEC             87
#define INI_STR_RATIO                    88

//---CALLOUT/WFC--
#define INI_STR_CALLOUT_ANSI             92
#define INI_STR_CALLOUT_COLOR            93
#define INI_STR_CALLOUT_HIGHLIGHT        94
#define INI_STR_CALLOUT_NORMAL           95
#define INI_STR_CALLOUT_COLOR_TEXT       96
#define INI_STR_WFC_SCREEN               97
#define INI_STR_SCREEN_SAVER_TIME        98
#define INI_STR_WFC_DRIVE                99

//---ASV/CALLBACK SETTINGS---
#define INI_STR_USE_SIMPLE_ASV          103
#define INI_STR_SIMPLE_ASV              104
#define INI_STR_USE_ADVANCED_ASV        105
#define INI_STR_ADVANCED_ASV            106
#define INI_STR_USE_CALLBACK            107
#define INI_STR_CALLBACK                108
#define INI_STR_USE_VOICE_VAL           109
#define INI_STR_AUTO_USER_PURGE         110
#define INI_STR_NO_PURGE_SL             111

// --- New WWIV 5.0 Settings ---
#define INI_STR_BEGINDAYNODENUMBER      126
#define INI_STR_INTERNALZMODEM          127
#define INI_STR_EXEC_LOG_SYNCFOSS       128
#define INI_STR_EXEC_USE_WAIT_FOR_IDLE  129
#define INI_STR_EXEC_CHILD_WAIT_TIME    130
#define INI_STR_EXEC_WAIT_FOR_IDLE_TIME 131
#define INI_STR_NEW_SCAN_AT_LOGIN   132


static const char* INI_OPTIONS_ARRAY[] = {
  "// entry #0.. ignore *",   // 0
  "//---SYSTEM OPTIONS---*",
  "SPAWNOPT",
  "NUCOLOR",
  "NUCOLORBW",
  "TOPCOLOR",                 // 5
  "F1COLOR",
  "EDITLINECOLOR",
  "CHATSELCOLOR",
  "MSG_COLOR",
  "",                         // 10
  "",
  "",
  "//---SYSTEM COMMANDS---*",
  "TERMINAL_CMD",
  "EXECUTE_CMD",              // 15
  "UPLOAD_CMD",
  "BEGINDAY_CMD",
  "NEWUSER_CMD",
  "LOGON_CMD",
  "LOGOFF_CMD",               // 20
  "VSCAN_CMD",
  "",
  "",
  "//---NETWORK SETTINGS---*",
  "NET_CLEANUP_CMD1",         // 25
  "NET_CLEANUP_CMD2",
  "NET_CALLOUT",
  "FIDO_PROCESS",
  "",
  "",                         // 30
  "",
  "//---SYSTEM SETTINGS---*",
  "FORCE_FBACK",
  "FORCE_NEWUSER",
  "FORCE_SCAN",               // 35
  "FORCE_SCAN_SUBNUM",
  "CHECK_HANGUP",
  "HANGUP_DUP_PHONES",
  "POSTTIME_COMPENS",
  "SHOW_HIER",                // 40
  "IDZ_DESC",
  "SETLDATE",
  "NEW_CHATSOUND",
  "SLASH_SZ",
  "READ_CD_IDZ",              // 45
  "FSED_EXT_DESC",
  "FAST_TAG_RELIST",
  "MAIL_PROMPT",
  "SHOW_CITY_ST",
  "LOCAL_SYSOP",              // 50
  "2WAY_CHAT",
  "NO_NEWUSER_FEEDBACK",
  "TITLEBAR",
  "LOG_DOWNLOADS",
  "CLOSE_XFER",               // 55
  "ALL_UL_TO_SYSOP",
  "NO_EASY_DL",
  "NEW_EXTRACT",
  "FAST_SEARCH",
  "USER_REGISTRATION",        // 60
  "MSG_TAG",
  "CHAIN_REG",
  "CAN_SAVE_SSM",
  "EXTRA_COLOR",
  "BEEP_CHAT",                // 65
  "TWO_COLOR_CHAT",
  "ALLOW_ALIASES",
  "USE_LIST",
  "FREE_PHONE",
  "EXTENDED_USERINFO",        // 70
  "NEWUSER_MIN",
  "ALLOW_CC_BCC",
  "THREAD_SUBS",
  "DISABLE_PD",
  "MAIL_WHO_LEN",             // 75
  "ATTACH_DIR",
  "ENABLE_PIPES",
  "ENABLE_MCI",
  "24HR_CLOCK",
  "",                         // 80
  "",
  "",
  "//---SYSTEM MAXIMUMS---*",
  "MAX_BATCH",
  "MAX_EXTEND_LINES",         // 85
  "MAX_CHAINS",
  "MAX_GFILESEC",
  "RATIO",
  "",
  "",                         // 90
  "//---CALLOUT/WFC---*",
  "CALLOUT_ANSI",
  "CALLOUT_COLOR",
  "CALLOUT_HIGHLIGHT",
  "CALLOUT_NORMAL",           // 95
  "CALLOUT_COLOR_TEXT",
  "WFC_SCREEN",
  "SCREEN_SAVER_TIME",
  "WFC_DRIVES",
  "",                         // 100
  "",
  "//---ASV/CALLBACK SETTINGS---*",
  "USE_SIMPLE_ASV",
  "SIMPLE_ASV",
  "USE_ADVANCED_ASV",         // 100
  "ADVANCED_ASV",
  "USE_CALLBACK",
  "CALLBACK",
  "USE_VOICE_VAL",
  "AUTO_USER_PURGE",          // 110
  "NO_PURGE_SL",
  "NO_PURGE_SL",
  "",
  "",
  "",             // 115
  "",
  "",
  "",
  "",
  "",             // 120
  "",
  "",
  "",
  "",
  "//--- New WWIV 5.0 Settings ---", // 125
  "BEGINDAYNODENUMBER",       // 126
  "INTERNALZMODEM",     // 127
  "EXEC_LOGSYNCFOSS",     // 128
  "EXEC_USECWAITFORIDLE",   // 129
  "EXEC_CHILDWAITTIME",   // 130
  "EXEC_WAITFORIDLETIME",   // 131
  "NEW_SCAN_AT_LOGIN",    // 132
  NULL,
  NULL
};


#endif  // __INCLUDED_XINITINI_H__
