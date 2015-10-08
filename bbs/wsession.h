/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
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
#if !defined ( __INCLUDED_BBS_WSESSION_H__ )
#define __INCLUDED_BBS_WSESSION_H__

#include <iostream>
#include <memory>
#include <string>

#include "bbs/capture.h"
#include "bbs/wcomm.h"
#include "bbs/wuser.h"
#include "bbs/woutstreambuffer.h"
#include "bbs/local_io.h"
#include "sdk/vardec.h"

//
// WSession - Holds information and status data about the current user
// session on the BBS. (i.e. user record, and all data/variables
// associated with the current logon)
//
// This is different from the BbsApp which holds global BBS information
// associated with this instance of WWIV globally (not tied to a user)
//

class WApplication;
///////////////////////////////////////////////////////////////////////////////
// ASV Settings (populated by INI file
//
struct asv_rec {
  uint8_t
  sl, dsl, exempt;

  uint16_t
  ar, dar, restrict;
};

struct adv_asv_rec {
  uint8_t reg_wwiv, nonreg_wwiv, non_wwiv, cosysop;
};

///////////////////////////////////////////////////////////////////////////////
// begin callback additions

struct cbv_rec {
  uint8_t
  sl, dsl, exempt, longdistance, forced, repeat;

  uint16_t
  ar, dar, restrict;
};

extern WOutStream bout;

///////////////////////////////////////////////////////////////////////////////
// Per-user session data
//
class WSession {
 public:
  WSession(WApplication* app, LocalIO* localIO);
  virtual ~WSession();

 public:
  static const int mmkeyNoArea = 0;
  static const int mmkeyMessageAreas = 1;
  static const int mmkeyFileAreas = 2;

  WUser* user() { return &m_thisuser; }

  void DisplaySysopWorkingIndicator(bool displayWait);
  WComm* remoteIO() { return comm_.get(); }
  LocalIO* localIO() { return local_io_.get(); }
  bool reset_local_io(LocalIO* wlocal_io);
  wwiv::bbs::Capture* capture() { return capture_.get(); }

  /*! @function CreateComm Creates up the communications subsystem */
  void CreateComm(unsigned int nHandle);

  bool IsLastKeyLocal() const { return last_key_local_; }
  void SetLastKeyLocal(bool b) { last_key_local_ = b; }

  bool ReadCurrentUser() { return ReadCurrentUser(usernum, false); }
  bool ReadCurrentUser(int nUserNumber, bool bForceRead = false);
  bool WriteCurrentUser() { return WriteCurrentUser(usernum); }
  bool WriteCurrentUser(int nUserNumber);

  void ResetEffectiveSl() { effective_sl_ = user()->GetSl(); }
  void SetEffectiveSl(int nSl) { effective_sl_ = nSl; }
  int  GetEffectiveSl() const { return effective_sl_; }

  int  GetTopScreenColor() const { return m_nTopScreenColor; }
  void SetTopScreenColor(int n) { m_nTopScreenColor = n; }

  int  GetUserEditorColor() const { return m_nUserEditorColor; }
  void SetUserEditorColor(int n) { m_nUserEditorColor = n; }

  int  GetEditLineColor() const { return m_nEditLineColor; }
  void SetEditLineColor(int n) { m_nEditLineColor = n; }

  int  GetChatNameSelectionColor() const { return m_nChatNameSelectionColor; }
  void SetChatNameSelectionColor(int n) { m_nChatNameSelectionColor = n; }

  int  GetMessageColor() const { return m_nMessageColor; }
  void SetMessageColor(int n) { m_nMessageColor = n; }

  int  GetForcedReadSubNumber() const { return m_nForcedReadSubNumber; }
  void SetForcedReadSubNumber(int n) { m_nForcedReadSubNumber = n; }

  const std::string GetCurrentSpeed() const { return current_speed_; }
  void SetCurrentSpeed(const std::string& s) { current_speed_ = s; }

  // This is used in sprintf in many places, so we return a char*
  // instead of a string.
  const char* GetNetworkName() const;
  const std::string GetNetworkDataDirectory() const;

  bool IsMessageThreadingEnabled() const { return m_bThreadSubs; }
  void SetMessageThreadingEnabled(bool b) { m_bThreadSubs = b; }

  bool IsCarbonCopyEnabled() const { return m_bAllowCC; }
  void SetCarbonCopyEnabled(bool b) { m_bAllowCC = b; }

  bool IsUserOnline() const { return m_bUserOnline; }
  void SetUserOnline(bool b) { m_bUserOnline = b; }

  int  GetFileAreaCacheNumber() const { return m_nFileAreaCache; }
  void SetFileAreaCacheNumber(int n) { m_nFileAreaCache = n; }

  int  GetMessageAreaCacheNumber() const { return m_nMessageAreaCache; }
  void SetMessageAreaCacheNumber(int n) { m_nMessageAreaCache = n; }

  int  GetCurrentLanguageNumber() const { return m_nCurrentLanguageNumber; }
  void SetCurrentLanguageNumber(int n) { m_nCurrentLanguageNumber = n; }

  bool IsInternetUseRealNames() const { return m_bInternetUseRealNames; }
  void SetInternetUseRealNames(bool b) { m_bInternetUseRealNames = b; }

  int  GetNumMessagesReadThisLogon() const { return m_nNumMessagesReadThisLogon; }
  void SetNumMessagesReadThisLogon(int n) { m_nNumMessagesReadThisLogon = n; }

  bool IsQuoting() const { return m_bQuoting; }
  void SetQuoting(bool b) { m_bQuoting = b; }

  bool IsNewScanAtLogin() const {return m_bNewScanAtLogin; } 
  void SetNewScanAtLogin(bool b) { m_bNewScanAtLogin =  b; }

  int  GetCurrentFileArea() const { return m_nCurrentFileArea; }
  void SetCurrentFileArea(int n) { m_nCurrentFileArea = n; }

  int  GetCurrentMessageArea() const { return m_nCurrentMessageArea; }
  void SetCurrentMessageArea(int n) { m_nCurrentMessageArea = n; }

  int  GetCurrentReadMessageArea() const { return m_nCurrentReadMessageArea; }
  void SetCurrentReadMessageArea(int n) { m_nCurrentReadMessageArea = n; }

  int  GetCurrentConferenceMessageArea() const { return m_nCurrentConferenceMessageArea; }
  void SetCurrentConferenceMessageArea(int n) { m_nCurrentConferenceMessageArea = n; }

  int  GetCurrentConferenceFileArea() const { return m_nCurrentConferenceFileArea; }
  void SetCurrentConferenceFileArea(int n) { m_nCurrentConferenceFileArea = n; }

  bool IsUseInternalZmodem() const { return m_bInternalZmodem; }
  void SetUseInternalZmodem(bool b) { m_bInternalZmodem = b; }

  int  GetNumMessagesInCurrentMessageArea() const { return m_nNumMsgsInCurrentSub; }
  void SetNumMessagesInCurrentMessageArea(int n) { m_nNumMsgsInCurrentSub = n; }

  int  GetBeginDayNodeNumber() const { return m_nBeginDayNodeNumber; }
  void SetBeginDayNodeNumber(int n) { m_nBeginDayNodeNumber = n; }
  
  int  GetExecChildProcessWaitTime() const { return m_nExecChildProcessWaitTime; }
  void SetExecChildProcessWaitTime(int n) { m_nExecChildProcessWaitTime = n; }

  bool IsExecLogSyncFoss() const { return m_bExecLogSyncFoss; }
  void SetExecLogSyncFoss(bool b) { m_bExecLogSyncFoss = b; }

  int  GetMaxNumberMessageAreas() const { return m_nMaxNumberMessageAreas; }
  void SetMaxNumberMessageAreas(int n) { m_nMaxNumberMessageAreas = n; }

  int  GetMaxNumberFileAreas() const { return m_nMaxNumberFileAreas; }
  void SetMaxNumberFileAreas(int n) { m_nMaxNumberFileAreas = n; }

  bool IsNewMailWatiting() const { return m_bNewMailWaiting; }
  void SetNewMailWaiting(bool b) { m_bNewMailWaiting = b; }

  bool IsTimeOnlineLimited() const { return m_bTimeOnlineLimited; }
  void SetTimeOnlineLimited(bool b) { m_bTimeOnlineLimited = b; }

  int  GetCurrentNetworkType() const { return m_nCurrentNetworkType; }
  void SetCurrentNetworkType(int n) { m_nCurrentNetworkType = n; }
   
  int  GetNetworkNumber() const { return m_nNetworkNumber; }
  void SetNetworkNumber(int n) { m_nNetworkNumber = n; }

  int  GetMaxNetworkNumber() const { return m_nMaxNetworkNumber; }
  void SetMaxNetworkNumber(int n) { m_nMaxNetworkNumber = n; }

  int  GetNumberOfChains() const { return m_nNumberOfChains; }
  void SetNumberOfChains(int n) { m_nNumberOfChains = n; }

  int  GetNumberOfEditors() const { return m_nNumberOfEditors; }
  void SetNumberOfEditors(int n) { m_nNumberOfEditors = n; }

  int  GetNumberOfExternalProtocols() const { return m_nNumberOfExternalProtocols; }
  void SetNumberOfExternalProtocols(int n) { m_nNumberOfExternalProtocols = n; }

  bool wwivmail_enabled() const { return wwivmail_enabled_; }
  void set_wwivmail_enabled(bool wwivmail_enabled) { wwivmail_enabled_ = wwivmail_enabled; }

  bool internal_qwk_enabled() const { return internal_qwk_enabled_; }
  void set_internal_qwk_enabled(bool internal_qwk_enabled) { internal_qwk_enabled_ = internal_qwk_enabled; }

 private:
  WApplication* application_;
  WUser m_thisuser;
  bool  last_key_local_;
  int effective_sl_;
  bool wwivmail_enabled_;
  bool internal_qwk_enabled_;
  std::unique_ptr<WComm> comm_;
  std::unique_ptr<LocalIO> local_io_;
  std::unique_ptr<wwiv::bbs::Capture> capture_;
  std::string current_speed_;

 public:
  // Data from system_operation_rec, make it public for now, and add
  // accessors later on.
  int
  m_nTopScreenColor,
  m_nUserEditorColor,
  m_nEditLineColor,
  m_nChatNameSelectionColor,
  m_nMessageColor;

  int         m_nForcedReadSubNumber;
  bool        m_bThreadSubs;
  bool        m_bAllowCC;
  bool        m_bUserOnline;
  bool        m_bQuoting;
  bool        m_bNewMailWaiting;
  bool        m_bTimeOnlineLimited;

  bool        m_bNewScanAtLogin,
              m_bInternalZmodem,
              m_bExecLogSyncFoss;

  int         m_nNumMessagesReadThisLogon,
              m_nFileAreaCache,
              m_nMessageAreaCache,
              m_nCurrentLanguageNumber,
              m_nCurrentFileArea,
              m_nCurrentMessageArea,
              m_nCurrentReadMessageArea,
              m_nCurrentConferenceMessageArea,
              m_nCurrentConferenceFileArea,
              m_nNumMsgsInCurrentSub,
              m_nBeginDayNodeNumber,
              m_nExecChildProcessWaitTime,
              m_nMaxNumberMessageAreas,
              m_nMaxNumberFileAreas,
              m_nCurrentNetworkType,
              m_nNetworkNumber,
              m_nMaxNetworkNumber,
              m_nNumberOfChains,
              m_nNumberOfEditors,
              m_nNumberOfExternalProtocols,
              numf,
              num_dirs,
              num_languages,
              num_sec,
              num_subs,
              num_events,
              num_sys_list,
              screenlinest,
              subchg,
              tagging,
              tagptr,
              titled,
              topdata,
              using_modem,
              numbatch,
              numbatchdl;

  std::string internetPopDomain;
  std::string internetEmailDomain;
  std::string internetEmailName;
  std::string internetFullEmailAddress;
  std::string usenetReferencesLine;
  std::string threadID;
  bool m_bInternetUseRealNames;

  uint32_t *m_DirectoryDateCache,
           *m_SubDateCache;

  std::string language_dir;
  char *cur_lang_name;

  int wfc_status;
  int usernum;

  asv_rec asv;
  adv_asv_rec advasv;
  cbv_rec cbv;

  uint16_t
  mail_who_field_len,
  max_batch,
  max_extend_lines,
  max_chains,
  max_gfilesec,
  screen_saver_time;

  unsigned char
  newuser_colors[10],         // skip for now
  newuser_bwcolors[10];       // skip for now
};

#endif  // #if !defined (__INCLUDED_BBS_WSESSION_H__)

