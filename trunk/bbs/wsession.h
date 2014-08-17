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
#if !defined ( __INCLUDED_WSESSION_H__ )
#define __INCLUDED_WSESSION_H__

#include <iostream>
#include <string>

#include "vardec.h"
#include "wuser.h"

//
// WSession - Holds information and status data about the current user
// session on the BBS. (i.e. user record, and all data/variables
// associated with the current logon)
//
// This is different from the BbsApp which holds global BBS information
// associated with this instance of WWIV globally (not tied to a user)
//

#include "woutstreambuffer.h"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable: 4511 4512 )
#endif // _MSC_VER

class WApplication;
class WLocalIO;
class WComm;
class WUser;

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

///////////////////////////////////////////////////////////////////////////////
// Per-user session data
//
class WSession {
 public:
  WSession(WApplication* app);
  WSession(WApplication* app, WLocalIO* localIO);
  virtual ~WSession();

 public:
  WOutStream bout;
  static const int mmkeyMessageAreas = 0;
  static const int mmkeyFileAreas = 1;
  static const int mmkeyChains = 2;

  WUser* GetCurrentUser() { return &m_thisuser; }

  void DisplaySysopWorkingIndicator(bool displayWait);
  WComm* remoteIO();
  WLocalIO* localIO();
  /*! @function CreateComm Creates up the communications subsystem */
  void CreateComm(bool bUseSockets, unsigned int nHandle);

  bool IsLastKeyLocal() const { return m_bLastKeyLocal; }
  void SetLastKeyLocal(bool b) { m_bLastKeyLocal = b; }

  bool ReadCurrentUser();
  bool ReadCurrentUser(int nUserNumber, bool bForceRead = false);
  bool WriteCurrentUser();
  bool WriteCurrentUser(int nUserNumber);

  void ResetEffectiveSl() { m_nEffectiveSl = GetCurrentUser()->GetSl(); }
  void SetEffectiveSl(int nSl) { m_nEffectiveSl = nSl; }
  int  GetEffectiveSl() const { return m_nEffectiveSl; }

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

  const std::string& GetCurrentSpeed() const { return m_currentSpeed; }
  void SetCurrentSpeed(std::string s) { m_currentSpeed = s; }
  void SetCurrentSpeed(const char *s) { m_currentSpeed = s; }

  const char* GetNetworkName() const;
  const char* GetNetworkDataDirectory() const;

  bool IsMessageThreadingEnabled() const { return m_bThreadSubs; }
  void SetMessageThreadingEnabled(bool b) { m_bThreadSubs = b; }

  bool IsCarbonCopyEnabled() const { return m_bAllowCC; }
  void SetCarbonCopyEnabled(bool b) { m_bAllowCC = b; }

  bool IsUserOnline() const { return m_bUserOnline; }
  void SetUserOnline(bool b) { m_bUserOnline = b; }

  int  GetMMKeyArea() const { return m_nMMKeyArea; }
  void SetMMKeyArea(int n) { m_nMMKeyArea = n; }

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
  
  bool IsExecUseWaitForInputIdle() const { return m_bExecUseWaitForInputIdle; }
  void SetExecUseWaitForInputIdle(bool b) { m_bExecUseWaitForInputIdle = b; }

  int  GetExecWaitForInputTimeout() const { return m_nExecUseWaitForInputTimeout; }
  void SetExecWaitForInputTimeout(int n) { m_nExecUseWaitForInputTimeout = n; }

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
  bool            m_bLastKeyLocal;
  int             m_nEffectiveSl;
  WApplication*   m_pApplication;
  WUser           m_thisuser;
  WComm*          m_pComm;
  WLocalIO*       m_pLocalIO;
  bool wwivmail_enabled_;
  bool internal_qwk_enabled_;

 public:
  //
  // Data from system_operation_rec, make it public for now, and add
  // accessors later on.
  //
  int
  m_nTopScreenColor,
  m_nUserEditorColor,
  m_nEditLineColor,
  m_nChatNameSelectionColor,
  m_nMessageColor;

  std::string m_currentSpeed;

  int         m_nForcedReadSubNumber;
  bool        m_bThreadSubs;
  bool        m_bAllowCC;
  bool        m_bUserOnline;
  bool        m_bQuoting;
  bool        m_bNewMailWaiting;
  bool        m_bTimeOnlineLimited;

  bool        m_bNewScanAtLogin,
              m_bInternalZmodem,
              m_bExecUseWaitForInputIdle,
              m_bExecLogSyncFoss;

  int         m_nMMKeyArea,
              m_nNumMessagesReadThisLogon,
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
              m_nExecUseWaitForInputTimeout,
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
  bool        m_bInternetUseRealNames;


  unsigned int *m_DirectoryDateCache,
           *m_SubDateCache;

  std::string language_dir;
  char    *cur_lang_name;

  int         wfcdrvs[5],
              wfc_status;

  int         usernum;

  asv_rec   asv;
  adv_asv_rec advasv;
  cbv_rec   cbv;

  unsigned short
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

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif  // #if !defined (__INCLUDED_WSESSION_H__)

