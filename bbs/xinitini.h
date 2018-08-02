/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
constexpr char* INI_STR_SPAWNOPT = "SPAWNOPT";
constexpr char* INI_STR_NUCOLOR = "NUCOLOR";
constexpr char* INI_STR_NUCOLORBW = "NUCOLORBW";
constexpr char* INI_STR_TOPCOLOR = "TOPCOLOR";
constexpr char* INI_STR_F1COLOR = "F1COLOR";
constexpr char* INI_STR_EDITLINECOLOR = "EDITLINECOLOR";
constexpr char* INI_STR_CHATSELCOLOR = "CHATSELCOLOR";
constexpr char* INI_STR_MSG_COLOR = "MSG_COLOR";

//---SYSTEM COMMANDS--
// 14
// 15
constexpr char* INI_STR_UPLOAD_CMD = "UPLOAD_CMD";
constexpr char* INI_STR_BEGINDAY_CMD = "BEGINDAY_CMD";
constexpr char* INI_STR_NEWUSER_CMD = "NEWUSER_CMD";
constexpr char* INI_STR_LOGON_CMD = "LOGON_CMD";
constexpr char* INI_STR_LOGOFF_CMD = "LOGOFF_CMD"; // not used?
constexpr char* INI_STR_V_SCAN_CMD = "VSCAN_CMD";
constexpr char* INI_STR_TERMINAL_CMD = "TERMINAL_CMD";

//---NETWORK SETTINGS--
//constexpr char* INI_STR_NET_CLEANUP_CMD1 = "NET_CLEANUP_CMD1";
//constexpr char* INI_STR_NET_CLEANUP_CMD2 = "NET_CLEANUP_CMD2";
constexpr char* INI_STR_NET_CALLOUT = "NET_CALLOUT";
//constexpr char* INI_STR_FIDO_PROCESS = "FIDO_PROCESS";
constexpr char* INI_STR_NET_PROCESS = "NET_CALLOUT";

//---SYSTEM SETTINGS--
constexpr char* INI_STR_FORCE_FBACK = "FORCE_FBACK";
constexpr char* INI_STR_FORCE_NEWUSER = "FORCE_NEWUSER";
constexpr char* INI_STR_USE_FORCE_SCAN = "FORCE_SCAN";
constexpr char* INI_STR_FORCE_SCAN_SUBNUM = "FORCE_SCAN_SUBNUM";
constexpr char* INI_STR_CHECK_DUP_PHONES = "CHECK_HANGUP";
constexpr char* INI_STR_HANGUP_DUP_PHONES = "HANGUP_DUP_PHONES";
constexpr char* INI_STR_POSTTIME_COMPENS = "POSTTIME_COMPENS";
constexpr char* INI_STR_SHOW_HIER = "SHOW_HIER";
constexpr char* INI_STR_IDZ_DESC = "IDZ_DESC";
constexpr char* INI_STR_SETLDATE = "SETLDATE";
//constexpr char* INI_STR_NEW_CHATSOUND = "NEW_CHATSOUND";
//constexpr char* INI_STR_SLASH_SZ = "SLASH_SZ";
constexpr char* INI_STR_READ_CD_IDZ = "READ_CD_IDZ";
constexpr char* INI_STR_FSED_EXT_DESC = "FSED_EXT_DESC";
constexpr char* INI_STR_FAST_TAG_RELIST = "FAST_TAG_RELIST";
constexpr char* INI_STR_MAIL_PROMPT = "MAIL_PROMPT";
constexpr char* INI_STR_SHOW_CITY_ST = "SHOW_CITY_ST";
//constexpr char* INI_STR_LOCAL_SYSOP = "LOCAL_SYSOP";
constexpr char* INI_STR_2WAY_CHAT = "2WAY_CHAT";
constexpr char* INI_STR_NO_NEWUSER_FEEDBACK = "NO_NEWUSER_FEEDBACK";
constexpr char* INI_STR_TITLEBAR = "TITLEBAR";
constexpr char* INI_STR_LOG_DOWNLOADS = "LOG_DOWNLOADS";
constexpr char* INI_STR_CLOSE_XFER = "CLOSE_XFER";
constexpr char* INI_STR_ALL_UL_TO_SYSOP = "ALL_UL_TO_SYSOP";
//constexpr char* INI_STR_NO_EASY_DL = "NO_EASY_DL";
//constexpr char* INI_STR_NEW_EXTRACT = "NEW_EXTRACT";
constexpr char* INI_STR_FAST_SEARCH = "FAST_SEARCH";
//constexpr char* INI_STR_USER_REGISTRATION = "USER_REGISTRATION";
constexpr char* INI_STR_MSG_TAG = "MSG_TAG";
constexpr char* INI_STR_CHAIN_REG = "CHAIN_REG";
constexpr char* INI_STR_CAN_SAVE_SSM = "CAN_SAVE_SSM";
constexpr char* INI_STR_EXTRA_COLOR = "EXTRA_COLOR";
constexpr char* INI_STR_BEEP_CHAT = "BEEP_CHAT";
constexpr char* INI_STR_TWO_COLOR_CHAT = "TWO_COLOR_CHAT";
constexpr char* INI_STR_ALLOW_ALIASES = "ALLOW_ALIASES";
constexpr char* INI_STR_USE_LIST = "USE_LIST";
constexpr char* INI_STR_FREE_PHONE = "FREE_PHONE";
constexpr char* INI_STR_EXTENDED_USERINFO = "EXTENDED_USERINFO";
constexpr char* INI_STR_NEWUSER_MIN = "NEWUSER_MIN";
constexpr char* INI_STR_ALLOW_CC_BCC = "ALLOW_CC_BCC";
//constexpr char* UNUSED_INI_STR_THREAD_SUBS = "THREAD_SUBS";
//constexpr char* UNUSED_INI_STR_DISABLE_PD = "DISABLE_PD";
constexpr char* INI_STR_MAIL_WHO_LEN = "MAIL_WHO_LEN";
constexpr char* INI_STR_ATTACH_DIR = "ATTACH_DIR";
//constexpr char* INI_STR_ENABLE_PIPES = "ENABLE_PIPES";
//constexpr char* UNUSED_INI_STR_ENABLE_MCI = "ENABLE_MCI";
constexpr char* INI_STR_24HR_CLOCK = "24HR_CLOCK";         //added Build2


//---SYSTEM MAXIMUMS--
constexpr char* INI_STR_MAX_BATCH = "MAX_BATCH";
constexpr char* INI_STR_MAX_EXTEND_LINES = "MAX_EXTEND_LINES";
constexpr char* INI_STR_MAX_CHAINS = "MAX_CHAINS";
constexpr char* INI_STR_MAX_GFILESEC = "MAX_GFILESEC";

//---CALLOUT/WFC--
constexpr char* INI_STR_CALLOUT_ANSI = "CALLOUT_ANSI";
constexpr char* INI_STR_CALLOUT_COLOR = "CALLOUT_COLOR";
constexpr char* INI_STR_CALLOUT_HIGHLIGHT = "CALLOUT_HIGHLIGHT";
constexpr char* INI_STR_CALLOUT_NORMAL = "CALLOUT_NORMAL";
constexpr char* INI_STR_CALLOUT_COLOR_TEXT = "CALLOUT_COLOR_TEXT";
constexpr char* INI_STR_WFC_SCREEN = "WFC_SCREEN";
constexpr char* INI_STR_SCREEN_SAVER_TIME = "SCREEN_SAVER_TIME";
constexpr char* INI_STR_WFC_DRIVE = "WFC_DRIVES";

//---ASV SETTINGS---
constexpr char* INI_STR_USE_SIMPLE_ASV = "USE_SIMPLE_ASV";
constexpr char* INI_STR_SIMPLE_ASV = "SIMPLE_ASV";
//constexpr char* INI_STR_USE_ADVANCED_ASV = "USE_ADVANCED_ASV";
constexpr char* INI_STR_ADVANCED_ASV = "ADVANCED_ASV";
//constexpr char* UNUSED_INI_STR_USE_CALLBACK = "USE_CALLBACK";
//constexpr char* UNUSED_INI_STR_CALLBACK = "CALLBACK";
//constexpr char* UNUSED_INI_STR_USE_VOICE_VAL = "USE_VOICE_VAL";
constexpr char* INI_STR_AUTO_USER_PURGE = "AUTO_USER_PURGE";
constexpr char* INI_STR_NO_PURGE_SL = "NO_PURGE_SL";

// --- New WWIV 5 Settings ---
constexpr char* INI_STR_BEGINDAYNODENUMBER = "BEGINDAYNODENUMBER";
constexpr char* INI_STR_INTERNALZMODEM  = "INTERNALZMODEM";
constexpr char* INI_STR_EXEC_LOG_SYNCFOSS = "EXEC_LOG_SYNCFOSS";
constexpr char* INI_STR_EXEC_CHILD_WAIT_TIME =   "EXEC_CHILD_WAIT_TIME";
constexpr char* INI_STR_NEW_SCAN_AT_LOGIN  = "NEW_SCAN_AT_LOGIN";

#endif  // __INCLUDED_XINITINI_H__
