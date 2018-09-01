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
#if !defined(__INCLUDED_BBS_APPLICATION_H__)
#define __INCLUDED_BBS_APPLICATION_H__

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "bbs/batch.h"
#include "bbs/conf.h"
#include "bbs/context.h"
#include "local_io/local_io.h"
#include "bbs/output.h"
#include "bbs/remote_io.h"
#include "bbs/runnable.h"
#include "core/file.h"
#include "sdk/config.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/names.h"
#include "sdk/net.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/vardec.h"

///////////////////////////////////////////////////////////////////////////////
// ASV Settings (populated by INI file
//
struct asv_rec {
  uint8_t sl, dsl, exempt;

  uint16_t ar, dar, restrict;
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

namespace wwiv {
namespace core {
class IniFile;
}
} // namespace wwiv

/**
 * Application - Holds information and status data about the current user
 * session on the BBS. (i.e. user record, and all data/variables
 * associated with the current logon)
 *
 * This is different from the BbsApp which holds global BBS information
 * associated with this instance of WWIV globally (not tied to a user)
 */
class Application : public Runnable {
  friend class BbsHelper;

public:
  // Constants
  static constexpr int exitLevelOK = 0;
  static constexpr int exitLevelNotOK = 1;
  static constexpr int exitLevelQuit = 2;

  Application(LocalIO* localIO);
  virtual ~Application();

  wwiv::sdk::User* user() { return &thisuser_; }
  wwiv::bbs::SessionContext& context();

  void handle_sysop_key(uint8_t key);
  void tleft(bool check_for_timeout);
  void DisplaySysopWorkingIndicator(bool displayWait);
  RemoteIO* remoteIO() const { return comm_.get(); }
  LocalIO* localIO() const { return local_io_.get(); }
  bool reset_local_io(LocalIO* wlocal_io);
  const std::string& GetAttachmentDirectory() const { return attach_dir_; }
  int instance_number() const { return instance_number_; }
  const std::string& network_extension() const { return network_extension_; }

  void UpdateTopScreen();
  void ClearTopScreenProtection();
  /** Calls LocalIO::Cls() and bout.clear_lines_listed */
  void Cls();

  /*! @function CreateComm Creates up the communications subsystem */
  void CreateComm(unsigned int nHandle, CommunicationType type);
  /**
   * Sets the RemoteIO handle for testing.
   * NOTE: This should only be used in unit tests.
   */
  void SetCommForTest(RemoteIO* remote_io);

  bool ReadCurrentUser() { return ReadCurrentUser(usernum); }
  bool ReadCurrentUser(int user_number);
  bool WriteCurrentUser() { return WriteCurrentUser(usernum); }
  bool WriteCurrentUser(int user_number);

  void reset_effective_sl() { effective_sl_ = user()->GetSl(); }
  void effective_sl(int nSl) { effective_sl_ = nSl; }
  int effective_sl() const { return effective_sl_; }
  const slrec& effective_slrec() const { return config()->sl(effective_sl_); }

  int GetChatNameSelectionColor() const { return chatname_color_; }

  int GetMessageColor() const { return message_color_; }

  uint16_t GetForcedReadSubNumber() const { return forced_read_subnum_; }
  void SetForcedReadSubNumber(uint16_t n) { forced_read_subnum_ = n; }

  std::string GetCurrentSpeed() const { return current_speed_; }
  void SetCurrentSpeed(const std::string& s) { current_speed_ = s; }

  // This is used in sprintf in many places, so we return a char*
  // instead of a string.
  const char* network_name() const;
  const std::string network_directory() const;

  bool IsCarbonCopyEnabled() const { return allow_cc_; }
  void SetCarbonCopyEnabled(bool b) { allow_cc_ = b; }

  bool IsUserOnline() const { return user_online_; }
  void SetUserOnline(bool b) { user_online_ = b; }

  int language_number() const { return m_nCurrentLanguageNumber; }
  void set_language_number(int n) {
    m_nCurrentLanguageNumber = n;
    if (n >= 0 && n <= static_cast<int>(languages.size())) {
      cur_lang_name = languages[n].name;
      language_dir = languages[n].dir;
    }
  }

  bool IsInternetUseRealNames() const { return m_bInternetUseRealNames; }
  void SetInternetUseRealNames(bool b) { m_bInternetUseRealNames = b; }

  int GetNumMessagesReadThisLogon() const { return m_nNumMessagesReadThisLogon; }
  void SetNumMessagesReadThisLogon(int n) { m_nNumMessagesReadThisLogon = n; }

  bool IsNewScanAtLogin() const { return newscan_at_login_; }
  void SetNewScanAtLogin(bool b) { newscan_at_login_ = b; }

  // This is the current user's dir number they are sitting on.
  // This is a user dir number (a()->udir[b], not directories[b]).
  uint16_t current_user_dir_num() const { return user_dir_num_; }
  void set_current_user_dir_num(uint16_t n) { user_dir_num_ = n; }

  // This is the current user's sub number they are sitting on.
  // This is a user sub number (usub[b], not subboards[b]).
  uint16_t current_user_sub_num() const { return user_sub_num_; }
  void set_current_user_sub_num(uint16_t n) { user_sub_num_ = n; }

  const usersubrec& current_user_sub() const { return usub[current_user_sub_num()]; }
  const usersubrec& current_user_dir() const { return udir[current_user_dir_num()]; }

  // This is set by iscan1 (for the most part) and is the sub number the user is
  // currently scanning/reading.  Note. this is the subnumber from subboards
  // not usub.
  // The most common usage pattern is:
  // iscan(a()->current_user_sub_num());
  // if (a()->GetCurrentReadMessageArea() < 0) { ... }

  // Note: This may be set to -1 to mean no area.
  int GetCurrentReadMessageArea() const { return current_read_message_area; }
  void SetCurrentReadMessageArea(int n) { current_read_message_area = n; }

  const wwiv::sdk::subboard_t& current_sub() const {
    return subs().sub(GetCurrentReadMessageArea());
  }
  const directoryrec& current_dir() const { return directories[current_user_dir().subnum]; }

  const net_networks_rec& current_net() const {
    const static net_networks_rec empty_rec{};
    if (net_networks.empty()) {
      return empty_rec;
    }
    return net_networks[net_num()];
  }

  uint16_t GetCurrentConferenceMessageArea() const { return current_conf_msgarea_; }
  void SetCurrentConferenceMessageArea(uint16_t n) { current_conf_msgarea_ = n; }

  uint16_t GetCurrentConferenceFileArea() const { return current_conf_filearea_; }
  void SetCurrentConferenceFileArea(uint16_t n) { current_conf_filearea_ = n; }

  bool IsUseInternalZmodem() const { return internal_zmodem_; }

  int GetNumMessagesInCurrentMessageArea() const { return m_nNumMsgsInCurrentSub; }
  void SetNumMessagesInCurrentMessageArea(int n) { m_nNumMsgsInCurrentSub = n; }

  int GetBeginDayNodeNumber() const { return beginday_node_number_; }
  void SetBeginDayNodeNumber(int n) { beginday_node_number_ = n; }

  int GetExecChildProcessWaitTime() const { return exec_child_process_wait_time_; }
  void SetExecChildProcessWaitTime(int n) { exec_child_process_wait_time_ = n; }

  bool IsExecLogSyncFoss() const { return exec_log_syncfoss_; }

  bool IsTimeOnlineLimited() const { return m_bTimeOnlineLimited; }
  void SetTimeOnlineLimited(bool b) { m_bTimeOnlineLimited = b; }

  int net_num() const { return network_num_; }
  void set_net_num(int n) { network_num_ = n; }

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

  bool HasConfigFlag(int nFlag) const { return (flags_ & nFlag) != 0; }

  uint16_t spawn_option(const std::string& c) const { return spawn_opts_.at(c); }

  bool IsCleanNetNeeded() const { return need_to_clean_net_; }
  void SetCleanNetNeeded(bool b) { need_to_clean_net_ = b; }

  void set_at_wfc(bool b) { at_wfc_ = b; }
  bool at_wfc() const { return at_wfc_; }

  bool fullscreen_read_prompt() const { return full_screen_read_prompt_; }

  void SetChatReason(const std::string& chat_reason) { chat_reason_ = chat_reason; }

  /** Returns the WWIV SDK Config Object. */
  wwiv::sdk::Config* config() const { return config_.get(); }
  void set_config_for_test(std::unique_ptr<wwiv::sdk::Config> config) {
    config_ = std::move(config);
  }
  /** Returns the WWIV Names.LST Config Object. */
  wwiv::sdk::Names* names() const { return names_.get(); }

  wwiv::sdk::msgapi::MessageApi* msgapi(int type) const { return msgapis_.at(type).get(); }
  wwiv::sdk::msgapi::MessageApi* msgapi() const {
    return msgapis_.at(current_sub().storage_type).get();
  }
  wwiv::sdk::msgapi::WWIVMessageApi* msgapi_email() const {
    return static_cast<wwiv::sdk::msgapi::WWIVMessageApi*>(msgapi(2));
  }

  // Public subsystems
  Batch& batch() { return batch_; }
  wwiv::sdk::Subs& subs() { return *subs_.get(); }
  const wwiv::sdk::Subs& subs() const { return *subs_.get(); }

  bool read_subs();
  bool create_message_api();
  void SetLogonTime();
  std::chrono::system_clock::time_point system_logon_time() const { return system_logon_time_; }
  std::chrono::seconds duration_used_this_session() const;

  std::chrono::seconds extratimecall() const;
  std::chrono::seconds set_extratimecall(std::chrono::duration<double> et);
  std::chrono::seconds add_extratimecall(std::chrono::duration<double> et);
  std::chrono::seconds subtract_extratimecall(std::chrono::duration<double> et);

  /*!
   * @function Run main bbs loop - Invoked from the application
   *           main method.
   * @param argc The number of arguments
   * @param argv arguments
   */
  int Run(int argc, char* argv[]);

  void ExitBBSImpl(int exit_level, bool perform_shutdown);

  void InitializeBBS();                       // old init() method
  void ReadINIFile(wwiv::core::IniFile& ini); // from xinit.cpp
  bool ReadInstanceSettings(int instance_number, wwiv::core::IniFile& ini);
  bool ReadConfig();

public:
  // Data from system_operation_rec, make it public for now, and add
  // accessors later on.
  int chatname_color_ = 0;
  int message_color_ = 0;

  uint16_t forced_read_subnum_ = 0;
  bool allow_cc_ = false;
  bool user_online_{false};
  bool quoting_ = false;
  bool m_bTimeOnlineLimited = false;

  bool newscan_at_login_ = false, internal_zmodem_ = true, exec_log_syncfoss_ = true;
  int m_nNumMessagesReadThisLogon = 0, m_nCurrentLanguageNumber = 0;
  uint16_t user_dir_num_ = 0;
  uint16_t user_sub_num_ = 0;
  // This one should stay in int since -1 is an allowed value.
  int current_read_message_area = 0;
  uint16_t current_conf_msgarea_ = 0;
  uint16_t current_conf_filearea_ = 0;
  int m_nNumMsgsInCurrentSub = 0, beginday_node_number_ = 1, exec_child_process_wait_time_ = 500,
      m_nMaxNumberMessageAreas = 0, m_nMaxNumberFileAreas = 0, network_num_ = 0,
      m_nMaxNetworkNumber = 0, numf = 0, subchg = 0, topdata = 0, using_modem = 0;
  int screenlinest = 25;
  int defscreenbottom = 24;

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

  uint16_t usernum;

  asv_rec asv;

  uint16_t mail_who_field_len = 0, max_batch = 0, max_extend_lines = 0, max_chains = 0,
           max_gfilesec = 0, screen_saver_time = 0;

  std::vector<uint8_t> newuser_colors;
  std::vector<uint8_t> newuser_bwcolors;

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
  std::vector<tagrec_t> filelist;
  std::vector<confrec> subconfs;
  std::vector<confrec> dirconfs;

  std::vector<userconfrec> uconfsub;
  std::vector<userconfrec> uconfdir;

  std::string beginday_cmd;     // beginday event
  std::string logon_cmd;        // logon event
  std::string newuser_cmd;      // newuser event
  std::string upload_cmd;       // upload event
  std::string terminal_command; // Terminal command

  // TODO(rushfan): Make this private. This is needed by WFC
  uint16_t unx_;

  // TODO(rushfan): All of these are moved from vars.h.
  // Figure out a better way
  bool chat_file_{false};
  bool chatcall_{false};
  bool received_short_message_{false};
  bool emchg_{false};
  bool hangup_{false};
  int chatting_ = false;
  int do_event_ = false;
  bool no_hangup_ = false;
  bool in_chatroom_ = false;
  bool chatline_ = false;
  int modem_speed_ = 0;
  bool mci_enabled_{true};

protected:
  /*!
   * @function GetCaller WFC Screen loop
   */
  void GetCaller();

  /*!
   * @function GotCaller login routines
   * @param ms Modem Speed (may be a locked speed)
   */
  void GotCaller(unsigned int ms);

private:
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
  void check_phonenum();
  void create_phone_file();

  // Private fields.
private:
  /*! The current working directory.*/
  std::string current_dir_;
  int oklevel_;
  int errorlevel_;
  int instance_number_{-1};
  std::string network_extension_;
  bool user_already_on_{false};
  bool need_to_clean_net_{false};
  bool at_wfc_{false};

  std::unique_ptr<wwiv::sdk::StatusMgr> statusMgr;
  std::unique_ptr<wwiv::sdk::UserManager> user_manager_;
  std::string attach_dir_;
  wwiv::sdk::User thisuser_;
  int effective_sl_{0};
  std::unique_ptr<RemoteIO> comm_;
  std::unique_ptr<LocalIO> local_io_;
  std::string current_speed_;
  std::unique_ptr<wwiv::sdk::Config> config_;
  std::unique_ptr<wwiv::sdk::Names> names_;
  std::map<int, std::unique_ptr<wwiv::sdk::msgapi::MessageApi>> msgapis_;

  Batch batch_;
  std::unique_ptr<wwiv::sdk::Subs> subs_;
  wwiv::bbs::SessionContext session_context_;

  // Former global variables and system_operation_rec members to be moved
  uint32_t flags_{0};
  std::map<const std::string, uint16_t> spawn_opts_;
  bool full_screen_read_prompt_{true};
  int last_read_user_number_{0};
  std::chrono::system_clock::time_point system_logon_time_;
  std::chrono::duration<double> extratimecall_;
};

#endif // #if !defined (__INCLUDED_BBS_APPLICATION_H__)
