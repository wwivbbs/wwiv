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

#ifndef __INCLUDED_XINITINI_H__
#define __INCLUDED_XINITINI_H__

//---SYSTEM OPTIONS--
constexpr const char* INI_STR_SPAWNOPT = "SPAWNOPT";
constexpr const char* INI_STR_NUCOLOR = "NUCOLOR";
constexpr const char* INI_STR_NUCOLORBW = "NUCOLORBW";
constexpr const char* INI_STR_TOPCOLOR = "TOPCOLOR";
constexpr const char* INI_STR_F1COLOR = "F1COLOR";
constexpr const char* INI_STR_EDITLINECOLOR = "EDITLINECOLOR";
constexpr const char* INI_STR_CHATSELCOLOR = "CHATSELCOLOR";
constexpr const char* INI_STR_MSG_COLOR = "MSG_COLOR";

//---SYSTEM COMMANDS--
constexpr const char* INI_STR_UPLOAD_CMD = "UPLOAD_CMD";
constexpr const char* INI_STR_BEGINDAY_CMD = "BEGINDAY_CMD";
constexpr const char* INI_STR_NEWUSER_CMD = "NEWUSER_CMD";
constexpr const char* INI_STR_LOGON_CMD = "LOGON_CMD";
constexpr const char* INI_STR_LOGOFF_CMD = "LOGOFF_CMD";
constexpr const char* INI_STR_CLEANUP_CMD = "CLEANUP_CMD";
constexpr const char* INI_STR_V_SCAN_CMD = "VSCAN_CMD";
constexpr const char* INI_STR_TERMINAL_CMD = "TERMINAL_CMD";

//---SYSTEM SETTINGS--
constexpr const char* INI_STR_FORCE_FBACK = "FORCE_FBACK";
constexpr const char* INI_STR_FORCE_NEWUSER = "FORCE_NEWUSER";
constexpr const char* INI_STR_USE_FORCE_SCAN = "FORCE_SCAN";
constexpr const char* INI_STR_FORCE_SCAN_SUBNUM = "FORCE_SCAN_SUBNUM";
constexpr const char* INI_STR_CHECK_DUP_PHONES = "CHECK_HANGUP";
constexpr const char* INI_STR_HANGUP_DUP_PHONES = "HANGUP_DUP_PHONES";
constexpr const char* INI_STR_POSTTIME_COMPENS = "POSTTIME_COMPENS";
constexpr const char* INI_STR_SHOW_HIER = "SHOW_HIER";
constexpr const char* INI_STR_IDZ_DESC = "IDZ_DESC";
constexpr const char* INI_STR_SETLDATE = "SETLDATE";
constexpr const char* INI_STR_READ_CD_IDZ = "READ_CD_IDZ";
constexpr const char* INI_STR_FSED_EXT_DESC = "FSED_EXT_DESC";
constexpr const char* INI_STR_FAST_TAG_RELIST = "FAST_TAG_RELIST";
constexpr const char* INI_STR_MAIL_PROMPT = "MAIL_PROMPT";
constexpr const char* INI_STR_SHOW_CITY_ST = "SHOW_CITY_ST";
constexpr const char* INI_STR_2WAY_CHAT = "2WAY_CHAT";
constexpr const char* INI_STR_NO_NEWUSER_FEEDBACK = "NO_NEWUSER_FEEDBACK";
constexpr const char* INI_STR_TITLEBAR = "TITLEBAR";
constexpr const char* INI_STR_LOG_DOWNLOADS = "LOG_DOWNLOADS";
constexpr const char* INI_STR_CLOSE_XFER = "CLOSE_XFER";
constexpr const char* INI_STR_ALL_UL_TO_SYSOP = "ALL_UL_TO_SYSOP";
constexpr const char* INI_STR_MSG_TAG = "MSG_TAG";
constexpr const char* INI_STR_CHAIN_REG = "CHAIN_REG";
constexpr const char* INI_STR_CAN_SAVE_SSM = "CAN_SAVE_SSM";
constexpr const char* INI_STR_TWO_COLOR_CHAT = "TWO_COLOR_CHAT";
constexpr const char* INI_STR_ALLOW_ALIASES = "ALLOW_ALIASES";
constexpr const char* INI_STR_FREE_PHONE = "FREE_PHONE";
constexpr const char* INI_STR_EXTENDED_USERINFO = "EXTENDED_USERINFO";
constexpr const char* INI_STR_NEWUSER_MIN = "NEWUSER_MIN";
constexpr const char* INI_STR_ALLOW_CC_BCC = "ALLOW_CC_BCC";
constexpr const char* INI_STR_MAIL_WHO_LEN = "MAIL_WHO_LEN";
constexpr const char* INI_STR_ATTACH_DIR = "ATTACH_DIR";
constexpr const char* INI_STR_NETFOSS_DIR = "NETFOSS_DIR";


//---SYSTEM MAXIMUMS--
constexpr const char* INI_STR_MAX_BATCH = "MAX_BATCH";
constexpr const char* INI_STR_MAX_EXTEND_LINES = "MAX_EXTEND_LINES";
constexpr const char* INI_STR_MAX_CHAINS = "MAX_CHAINS";
constexpr const char* INI_STR_MAX_GFILESEC = "MAX_GFILESEC";

//---CALLOUT/WFC--
constexpr const char* INI_STR_WFC_SCREEN = "WFC_SCREEN";
constexpr const char* INI_STR_SCREEN_SAVER_TIME = "SCREEN_SAVER_TIME";

//---ASV SETTINGS---
constexpr const char* INI_STR_USE_SIMPLE_ASV = "USE_SIMPLE_ASV";
constexpr const char* INI_STR_SIMPLE_ASV = "SIMPLE_ASV";
constexpr const char* INI_STR_ADVANCED_ASV = "ADVANCED_ASV";
constexpr const char* INI_STR_AUTO_USER_PURGE = "AUTO_USER_PURGE";
constexpr const char* INI_STR_NO_PURGE_SL = "NO_PURGE_SL";

// --- New WWIV 5 Settings ---
constexpr const char* INI_STR_BEGINDAYNODENUMBER = "BEGINDAYNODENUMBER";
constexpr const char* INI_STR_INTERNALZMODEM = "INTERNALZMODEM";
constexpr const char* INI_STR_INTERNAL_FSED = "INTERNAL_FSED";
constexpr const char* INI_STR_EXEC_LOG_SYNCFOSS = "EXEC_LOG_SYNCFOSS";
constexpr const char* INI_STR_EXEC_CHILD_WAIT_TIME =   "EXEC_CHILD_WAIT_TIME";
constexpr const char* INI_STR_NEW_SCAN_AT_LOGIN  = "NEW_SCAN_AT_LOGIN";

#endif  // __INCLUDED_XINITINI_H__
