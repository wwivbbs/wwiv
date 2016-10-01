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
#if !defined ( __INCLUDED_BBS_WSESSION_H__ )
#define __INCLUDED_BBS_WSESSION_H__

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "bbs/batch.h"
#include "bbs/runnable.h"
#include "bbs/remote_io.h"
#include "sdk/status.h"
#include "bbs/output.h"
#include "sdk/subxtr.h"
#include "bbs/local_io.h"
#include "core/inifile.h"
#include "core/file.h"
#include "sdk/config.h"
#include "sdk/names.h"
#include "sdk/net.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
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

// Holds information about tagged files.
struct tagrec_t {
  // file information
  uploadsrec u;
  // directory number
  int16_t directory;
  // directory mask
  uint16_t dir_mask;
};

extern Output bout;

///////////////////////////////////////////////////////////////////////////////
// Per-user session data
//
class WSession : public Runnable {
  friend class BbsHelper;
public:
  // Constants
  static const int exitLevelOK = 0;
  static const int exitLevelNotOK = 1;
  static const int exitLevelQuit = 2;

  static const int shutdownNone = 0;
  static const int shutdownThreeMinutes = 1;
  static const int shutdownTwoMinutes = 2;
  static const int shutdownOneMinute = 3;
  static const int shutdownImmediate = 4;

public:
  WSession(WApplication* app, LocalIO* localIO);
  virtual ~WSession();

  wwiv::sdk::User* user() { return &thisuser_; }

  void handle_sysop_key(uint8_t key);
  void tleft(bool check_for_timeout);
  void DisplaySysopWorkingIndicator(bool displayWait);
  RemoteIO* remoteIO() { return comm_.get(); }
  LocalIO* localIO() { return local_io_.get(); }
  bool reset_local_io(LocalIO* wlocal_io);
  const std::string& GetAttachmentDirectory() { return m_attachmentDirectory; }
  int  instance_number() const { return instance_number_; }
  const std::string& network_extension() const { return network_extension_; }

  void UpdateTopScreen();
  void ClearTopScreenProtection();

  /*! @function CreateComm Creates up the communications subsystem */
  void CreateComm(unsigned int nHandle, CommunicationType type);

  bool IsLastKeyLocal() const { return last_key_local_; }
  void SetLastKeyLocal(bool b) { last_key_local_ = b; }

  bool ReadCurrentUser() { return ReadCurrentUser(usernum); }
  bool ReadCurrentUser(int user_number);
  bool WriteCurrentUser() { return WriteCurrentUser(usernum); }
  bool WriteCurrentUser(int user_number);

  void ResetEffectiveSl() { effective_sl_ = user()->GetSl(); }
  void SetEffectiveSl(int nSl) { effective_sl_ = nSl; }
  unsigned int GetEffectiveSl() const { return effective_sl_; }

  int  GetChatNameSelectionColor() const { return chatname_color_; }
  void SetChatNameSelectionColor(int n) { chatname_color_ = n; }

  int  GetMessageColor() const { return message_color_; }
  void SetMessageColor(int n) { message_color_ = n; }

  int  GetForcedReadSubNumber() const { return m_nForcedReadSubNumber; }
  void SetForcedReadSubNumber(int n) { m_nForcedReadSubNumber = n; }

  const std::string GetCurrentSpeed() const { return current_speed_; }
  void SetCurrentSpeed(const std::string& s) { current_speed_ = s; }

  // This is used in sprintf in many places, so we return a char*
  // instead of a string.
  const char* network_name() const;
  const std::string network_directory() const;

  bool IsCarbonCopyEnabled() const { return m_bAllowCC; }
  void SetCarbonCopyEnabled(bool b) { m_bAllowCC = b; }

  bool IsUserOnline() const { return m_bUserOnline; }
  void SetUserOnline(bool b) { m_bUserOnline = b; }

  int  language_number() const { return m_nCurrentLanguageNumber; }
  void set_language_number(int n) { 
    m_nCurrentLanguageNumber = n; 
    if (n >= 0 && n <= static_cast<int>(languages.size())) {
      cur_lang_name = languages[n].name;
      language_dir = languages[n].dir;
    }
  }

  bool IsInternetUseRealNames() const { return m_bInternetUseRealNames; }
  void SetInternetUseRealNames(bool b) { m_bInternetUseRealNames = b; }

  int  GetNumMessagesReadThisLogon() const { return m_nNumMessagesReadThisLogon; }
  void SetNumMessagesReadThisLogon(int n) { m_nNumMessagesReadThisLogon = n; }

  bool IsNewScanAtLogin() const {return m_bNewScanAtLogin; } 
  void SetNewScanAtLogin(bool b) { m_bNewScanAtLogin =  b; }

  // This is the current user's dir number they are sitting on.
  // This is a user dir number (session()->udir[b], not directories[b]).
  int  current_user_dir_num() const { return m_nCurrentFileArea; }
  void set_current_user_dir_num(int n) { m_nCurrentFileArea = n; }

  // This is the current user's sub number they are sitting on.
  // This is a user sub number (usub[b], not subboards[b]).
  size_t current_user_sub_num() const { return current_sub_num_; }
  void set_current_user_sub_num(size_t n) { current_sub_num_ = n; }

  const usersubrec& current_user_sub() const { return usub[current_user_sub_num()]; }
  const usersubrec& current_user_dir() const { return udir[current_user_dir_num()]; }

  // This is set by iscan1 (for the most part) and is the sub number the user is
  // currently scanning/reading.  Note. this is the subnumber from subboards
  // not usub.
  // The most common usage pattern is:
  // iscan(session()->current_user_sub_num());
  // if (session()->GetCurrentReadMessageArea() < 0) { ... }

  // Note: This may be set to -1 to mean no area.
  int  GetCurrentReadMessageArea() const { return m_nCurrentReadMessageArea; }
  void SetCurrentReadMessageArea(int n) { m_nCurrentReadMessageArea = n; }

  const wwiv::sdk::subboard_t& current_sub() { return subs().sub(GetCurrentReadMessageArea()); }
  net_networks_rec& current_net() { return net_networks[net_num()]; }

  size_t GetCurrentConferenceMessageArea() const { return m_nCurrentConferenceMessageArea; }
  void SetCurrentConferenceMessageArea(size_t n) { m_nCurrentConferenceMessageArea = n; }

  size_t GetCurrentConferenceFileArea() const { return m_nCurrentConferenceFileArea; }
  void SetCurrentConferenceFileArea(size_t n) { m_nCurrentConferenceFileArea = n; }

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

  bool IsTimeOnlineLimited() const { return m_bTimeOnlineLimited; }
  void SetTimeOnlineLimited(bool b) { m_bTimeOnlineLimited = b; }

  int  net_type() const { return m_nCurrentNetworkType; }
  void set_net_type(int n) { m_nCurrentNetworkType = n; }
   
  int  net_num() const { return m_nNetworkNumber; }
  void set_net_num(int n) { m_nNetworkNumber = n; }

  int  max_net_num() const { return net_networks.size(); }

  wwiv::sdk::StatusMgr* status_manager() { return statusMgr.get(); }
  wwiv::sdk::UserManager* users() { return user_manager_.get(); }

  const std::string& temp_directory() const { return temp_directory_; }
  const std::string& batch_directory() const { return batch_directory_; }
  const uint8_t primary_port() const { return primary_port_; }

  /*!
  * @function GetHomeDir Returns the current home directory
  */
  const std::string GetHomeDir();

  /*! @function CdHome Changes directories back to the WWIV Home directory */
  void CdHome();

  /*! @function AbortBBS - Shuts down the bbs at the not-ok error level */
  void AbortBBS(bool bSkipShutdown = false);

  /*! @function QuitBBS - Shuts down the bbs at the "QUIT" error level */
  void QuitBBS();

  void SetConfigFlag(int nFlag) { flags |= nFlag; }
  void ToggleConfigFlag(int nFlag) { flags ^= nFlag; }
  void ClearConfigFlag(int nFlag) { flags &= ~nFlag; }
  bool HasConfigFlag(int nFlag) const { return (flags & nFlag) != 0; }
  void SetConfigFlags(int nFlags) { flags = nFlags; }
  uint32_t GetConfigFlags() const { return flags; }

  uint16_t GetSpawnOptions(int nCmdID) { return spawn_opts[nCmdID]; }

  bool IsCleanNetNeeded() const { return need_to_clean_net_; }
  void SetCleanNetNeeded(bool b) { need_to_clean_net_ = b; }

  void SetWfcStatus(int nStatus) { wfc_status_ = nStatus; }
  int  GetWfcStatus() { return wfc_status_; }

  void SetChatReason(const std::string& chat_reason) { chat_reason_ = chat_reason; }

  /** Returns the WWIV SDK Config Object. */
  wwiv::sdk::Config* config() const { return config_.get(); }
  void set_config_for_test(std::unique_ptr<wwiv::sdk::Config> config) { config_ = std::move(config); }
  /** Returns the WWIV Names.LST Config Object. */
  wwiv::sdk::Names* names() const { return names_.get(); }

  bool read_subs();
  
  // former global variables and system_operation_rec members
  // to be moved
  unsigned long flags;
  unsigned short spawn_opts[20];

  /*!
  * @function ShowUsage - Shows the help screen to the user listing
  *           all of the command line arguments for WWIV
  */
  void ShowUsage();

  /*!
  * @function Run main bbs loop - Invoked from the application
  *           main method.
  * @param argc The number of arguments
  * @param argv arguments
  */
  int Run(int argc, char *argv[]);

   void ExitBBSImpl(int exit_level, bool perform_shutdown);

   void InitializeBBS(); // old init() method
   void ReadINIFile(wwiv::core::IniFile& ini); // from xinit.cpp
   bool ReadInstanceSettings(int instance_number, wwiv::core::IniFile& ini);
   bool ReadConfig();

   int LocalLogon();

 private:
   uint16_t str2spawnopt(const char *s);
   uint16_t str2restrict(const char *s);
   void read_nextern();
   void read_arcs();
   void read_editors();
   void read_nintern();
   void read_networks();
   bool read_names();
   bool read_dirs();
   void read_chains();
   bool read_language();
   void read_gfile();
   void make_abs_path(char *dir);
   void check_phonenum();
   void create_phone_file();

 protected:
   /*!
   * @function GetCaller WFC Screen loop
   */
   void GetCaller();

   int doWFCEvents();

   /*!
   * @function GotCaller login routines
   * @param ms Modem Speed (may be a locked speed)
   * @param cs Connect Speed (real speed)
   */
   void GotCaller(unsigned int ms, unsigned long cs);

private:
  uint16_t  unx_;
  /*! The current working directory.*/
  std::string current_dir_;
  int             oklevel_;
  int             errorlevel_;
  int             instance_number_ = -1;
  std::string     network_extension_;
  double          last_time = 0;
  bool            user_already_on_ = false;
  bool            need_to_clean_net_ = false;
  int             shutdown_status_ = shutdownNone;
  double          shutdown_time_ = 0;
  int             wfc_status_ = 0;

  std::unique_ptr<wwiv::sdk::StatusMgr> statusMgr;
  std::unique_ptr<wwiv::sdk::UserManager> user_manager_;
  std::string m_attachmentDirectory;
  WApplication* application_;
  wwiv::sdk::User thisuser_;
  bool last_key_local_ = true;
  int effective_sl_ = 0;
  std::unique_ptr<RemoteIO> comm_;
  std::unique_ptr<LocalIO> local_io_;
  std::string current_speed_;
  std::unique_ptr<wwiv::sdk::Config> config_;
  std::unique_ptr<wwiv::sdk::Names> names_;


  // Data from system_operation_rec, make it public for now, and add
  // accessors later on.
  public:
    int chatname_color_ = 0;
    int message_color_ = 0;

    int         m_nForcedReadSubNumber = 0;
    bool        m_bAllowCC = false;
    bool        m_bUserOnline = false;
    bool        m_bQuoting = false;
    bool        m_bTimeOnlineLimited = false;

  bool        m_bNewScanAtLogin = false,
              m_bInternalZmodem = true,
              m_bExecLogSyncFoss = true;
  int         m_nNumMessagesReadThisLogon = 0,
              m_nCurrentLanguageNumber = 0,
              m_nCurrentFileArea = 0,
              current_sub_num_ = 0,
              m_nCurrentReadMessageArea = 0,
              m_nCurrentConferenceMessageArea = 0,
              m_nCurrentConferenceFileArea = 0,
              m_nNumMsgsInCurrentSub = 0,
              m_nBeginDayNodeNumber = 0,
              m_nExecChildProcessWaitTime = 0,
              m_nMaxNumberMessageAreas = 0,
              m_nMaxNumberFileAreas = 0,
              m_nCurrentNetworkType = 0,
              m_nNetworkNumber = 0,
              m_nMaxNetworkNumber = 0,
              numf = 0,
              subchg = 0,
              topdata = 0,
              using_modem = 0;
  unsigned int screenlinest = 0;

  std::string internetPopDomain;
  std::string internetEmailDomain;
  std::string internetEmailName;
  std::string internetFullEmailAddress;
  std::string usenetReferencesLine;
  bool m_bInternetUseRealNames;
  std::string language_dir;
  std::string cur_lang_name;
  std::string chat_reason_;
  std::string net_email_name;
  std::string temp_directory_;
  std::string batch_directory_;
  uint8_t primary_port_ = 1;
  std::string extended_description_filename_;
  std::string dsz_logfile_name_;
  std::string download_filename_;


  int wfc_status;
  int usernum;

  asv_rec asv;
  adv_asv_rec advasv;

  uint16_t
  mail_who_field_len = 0,
  max_batch = 0,
  max_extend_lines = 0,
  max_chains = 0,
  max_gfilesec = 0,
  screen_saver_time = 0;

  unsigned char
  newuser_colors[10],         // skip for now
  newuser_bwcolors[10];       // skip for now

public:
  // Public subsystems
  Batch batch_;
  Batch& batch() { return batch_; }
  std::unique_ptr<wwiv::sdk::Subs> subs_;
  wwiv::sdk::Subs& subs() { return *subs_.get(); }

  // public data structures
  std::vector<editorrec> editors;
  std::vector<chainfilerec> chains;
  std::vector<chainregrec> chains_reg;

  std::vector<newexternalrec> externs;
  std::vector<newexternalrec> over_intern;
  std::vector<languagerec> languages;
  std::vector<net_networks_rec> net_networks;
  std::vector<gfiledirrec> gfilesec;
  std::vector<arcrec> arcs;
  std::vector<directoryrec> directories;
  std::vector<usersubrec> usub;
  std::vector<usersubrec> udir;
  std::vector<eventsrec> events;
  std::vector<net_system_list_rec> csn;
  std::vector<uint16_t> csn_index;
  std::vector<tagrec_t> filelist;
};

#endif  // #if !defined (__INCLUDED_BBS_WSESSION_H__)

