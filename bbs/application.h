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
#if !defined(__INCLUDED_BBS_APPLICATION_H__)
#define __INCLUDED_BBS_APPLICATION_H__

#include "bbs/batch.h"
#include "bbs/conf.h"
#include "bbs/context.h"
#include "bbs/output.h"
#include "bbs/runnable.h"
#include "sdk/net/networks.h"
#include "sdk/vardec.h"

#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
enum class CommunicationType;
class LocalIO;
class RemoteIO;
struct net_networks_rec;

namespace wwiv {
namespace core {
class IniFile;
}
namespace sdk {
class Config;
class Chains;
class Names;
class StatusMgr;
class Subs;
struct subboard_t;
class User;
class UserManager;

namespace files {
class Dirs;
class FileApi;
class FileArea;
struct directory_t;
} // namespace wwiv::sdk::files

namespace msgapi {
class MessageApi;
class WWIVMessageApi;
} // namespace msgapi
} // namespace sdk
} // namespace wwiv

/**
 * Application - Holds information and status data about the current user
 * session on the BBS. (i.e. user record, and all data/variables
 * associated with the current logon)
 *
 * This is different from the BbsApp which holds global BBS information
 * associated with this instance of WWIV globally (not tied to a user)
 */
class Application final : public Runnable {
  friend class BbsHelper;

public:
  // Constants
  static constexpr int exitLevelOK{0};
  static constexpr int exitLevelNotOK = 1;
  static constexpr int exitLevelQuit = 2;

  explicit Application(LocalIO* localIO);
  virtual ~Application();

  [[nodiscard]] wwiv::sdk::User* user() const { return thisuser_.get(); }
  [[nodiscard]] wwiv::bbs::SessionContext& context();

  void handle_sysop_key(uint8_t key);
  void tleft(bool check_for_timeout);
  void DisplaySysopWorkingIndicator(bool displayWait);
  [[nodiscard]] RemoteIO* remoteIO() const { return comm_.get(); }
  [[nodiscard]] LocalIO* localIO() const;
  bool reset_local_io(LocalIO* wlocal_io);
  [[nodiscard]] const std::string& GetAttachmentDirectory() const { return attach_dir_; }
  [[nodiscard]] int instance_number() const { return instance_number_; }
  [[nodiscard]] const std::string& network_extension() const { return network_extension_; }

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

  void reset_effective_sl();
  void effective_sl(int nSl);
  [[nodiscard]] int effective_sl() const;
  [[nodiscard]] const slrec& effective_slrec() const;

  [[nodiscard]] int GetChatNameSelectionColor() const { return chatname_color_; }

  [[nodiscard]] int GetMessageColor() const { return message_color_; }

  [[nodiscard]] uint16_t GetForcedReadSubNumber() const { return forced_read_subnum_; }
  void SetForcedReadSubNumber(int n) { forced_read_subnum_ = static_cast<uint16_t>(n); }

  [[nodiscard]] std::string GetCurrentSpeed() const { return current_speed_; }
  void SetCurrentSpeed(const std::string& s) { current_speed_ = s; }

  [[nodiscard]] std::string network_name() const;
  [[nodiscard]] std::string network_directory() const;

  [[nodiscard]] bool IsCarbonCopyEnabled() const { return allow_cc_; }
  void SetCarbonCopyEnabled(bool b) { allow_cc_ = b; }

  [[nodiscard]] bool IsUserOnline() const { return user_online_; }
  void SetUserOnline(bool b) { user_online_ = b; }

  [[nodiscard]] int language_number() const;

  void set_language_number(int n);

  [[nodiscard]] bool IsInternetUseRealNames() const { return m_bInternetUseRealNames; }
  void SetInternetUseRealNames(bool b) { m_bInternetUseRealNames = b; }

  [[nodiscard]] int GetNumMessagesReadThisLogon() const { return m_nNumMessagesReadThisLogon; }
  void SetNumMessagesReadThisLogon(int n) { m_nNumMessagesReadThisLogon = n; }

  [[nodiscard]] bool IsNewScanAtLogin() const { return newscan_at_login_; }
  void SetNewScanAtLogin(bool b) { newscan_at_login_ = b; }

  // This is the current user's dir number they are sitting on.
  // This is a user dir number (a()->udir[b], not directories[b]).
  [[nodiscard]] uint16_t current_user_dir_num() const { return user_dir_num_; }
  void set_current_user_dir_num(int n) { user_dir_num_ = static_cast<uint16_t>(n); }

  // This is the current user's sub number they are sitting on.
  // This is a user sub number (usub[b], not subboards[b]).
  [[nodiscard]] uint16_t current_user_sub_num() const { return user_sub_num_; }
  void set_current_user_sub_num(int n) { user_sub_num_ = static_cast<uint16_t>(n); }

  [[nodiscard]] const usersubrec& current_user_sub() const { return usub[current_user_sub_num()]; }
  [[nodiscard]] const usersubrec& current_user_dir() const { return udir[current_user_dir_num()]; }

  // This is set by iscan1 (for the most part) and is the sub number the user is
  // currently scanning/reading.  Note. this is the subnumber from subboards
  // not usub.
  // The most common usage pattern is:
  // iscan(a()->current_user_sub_num());
  // if (a()->GetCurrentReadMessageArea() < 0) { ... }

  // Note: This may be set to -1 to mean no area.
  [[nodiscard]] int GetCurrentReadMessageArea() const { return current_read_message_area; }
  void SetCurrentReadMessageArea(int n) { current_read_message_area = n; }

  [[nodiscard]] const wwiv::sdk::subboard_t& current_sub() const;
  [[nodiscard]] const wwiv::sdk::files::directory_t& current_dir() const;

  [[nodiscard]] const net_networks_rec& current_net() const;

  [[nodiscard]] uint16_t current_user_sub_conf_num() const { return current_conf_msgarea_; }
  void set_current_user_sub_conf_num(int n) { current_conf_msgarea_ = static_cast<uint16_t>(n); }

  [[nodiscard]] uint16_t current_user_dir_conf_num() const { return current_conf_filearea_; }
  void set_current_user_dir_conf_num(int n) { current_conf_filearea_ = static_cast<uint16_t>(n); }

  [[nodiscard]] bool IsUseInternalZmodem() const { return internal_zmodem_; }
  [[nodiscard]] bool IsUseInternalFsed() const; 

  [[nodiscard]] int GetNumMessagesInCurrentMessageArea() const { return m_nNumMsgsInCurrentSub; }
  void SetNumMessagesInCurrentMessageArea(int n) { m_nNumMsgsInCurrentSub = n; }

  [[nodiscard]] int GetBeginDayNodeNumber() const { return beginday_node_number_; }
  void SetBeginDayNodeNumber(int n) { beginday_node_number_ = n; }

  [[nodiscard]] int GetExecChildProcessWaitTime() const { return exec_child_process_wait_time_; }
  void SetExecChildProcessWaitTime(int n) { exec_child_process_wait_time_ = n; }

  [[nodiscard]] bool IsExecLogSyncFoss() const { return exec_log_syncfoss_; }

  [[nodiscard]] bool IsTimeOnlineLimited() const { return m_bTimeOnlineLimited; }
  void SetTimeOnlineLimited(bool b) { m_bTimeOnlineLimited = b; }

  [[nodiscard]] int net_num() const { return network_num_; }
  void set_net_num(int n) { network_num_ = n; }

  [[nodiscard]] wwiv::sdk::StatusMgr* status_manager() const { return statusMgr.get(); }
  [[nodiscard]] wwiv::sdk::UserManager* users() const { return user_manager_.get(); }

  [[nodiscard]] const std::string& temp_directory() const { return temp_directory_; }
  [[nodiscard]] const std::string& batch_directory() const { return batch_directory_; }
  /**
   * Used instead of QWK_DIRECTORY.  Today it is the same as batch but wanted to
   * leave it open for changing in the future.
   */
  [[nodiscard]] const std::string& qwk_directory() const { return batch_directory_; }
  [[nodiscard]] uint8_t primary_port() const { return primary_port_; }

  [[nodiscard]] std::filesystem::path bbspath() const noexcept;
  [[nodiscard]] std::string bbsdir() const noexcept;
  [[nodiscard]] std::string bindir() const noexcept;
  [[nodiscard]] std::string configdir() const noexcept;
  [[nodiscard]] std::string logdir() const noexcept;
  [[nodiscard]] int verbose() const noexcept;

  [[nodiscard]] bool HasConfigFlag(int nFlag) const { return (flags_ & nFlag) != 0; }

  [[nodiscard]] uint16_t spawn_option(const std::string& c) const { return spawn_opts_.at(c); }

  void set_at_wfc(bool b) { at_wfc_ = b; }
  [[nodiscard]] bool at_wfc() const { return at_wfc_; }

  [[nodiscard]] bool fullscreen_read_prompt() const { return full_screen_read_prompt_; }

  void SetChatReason(const std::string& chat_reason) {
    chat_reason_ = chat_reason;
    chatcall_ = !chat_reason.empty();
  }

  /** Is the chat call alert (user wanted to chat with the sysop. enabled? */
  [[nodiscard]] bool chatcall() const { return chatcall_; }
  /** Clears the chat call alert (user wanted to chat with the sysop. enabled? */
  void clear_chatcall() { chatcall_ = false; }

  /** Returns the WWIV SDK Config Object. */
  [[nodiscard]] wwiv::sdk::Config* config() const;
  void set_config_for_test(std::unique_ptr<wwiv::sdk::Config> config);

  /** Returns the WWIV Names.LST Config Object. */
  [[nodiscard]] wwiv::sdk::Names* names() const;

  [[nodiscard]] wwiv::sdk::msgapi::MessageApi* msgapi(int type) const;
  [[nodiscard]] wwiv::sdk::msgapi::MessageApi* msgapi() const;
  [[nodiscard]] wwiv::sdk::msgapi::WWIVMessageApi* msgapi_email() const;

  [[nodiscard]] wwiv::sdk::files::FileApi* fileapi() const;
  [[nodiscard]] wwiv::sdk::files::FileArea* current_file_area() const;
  void set_current_file_area(std::unique_ptr<wwiv::sdk::files::FileArea> a);

  // Public subsystems
  [[nodiscard]] Batch& batch();
  [[nodiscard]] wwiv::sdk::Subs& subs();
  [[nodiscard]] const wwiv::sdk::Subs& subs() const;

  [[nodiscard]] wwiv::sdk::files::Dirs& dirs();
  [[nodiscard]] const wwiv::sdk::files::Dirs& dirs() const;

  [[nodiscard]] wwiv::sdk::Networks& nets();
  [[nodiscard]] const wwiv::sdk::Networks& nets() const;

  bool read_subs();
  bool create_message_api();
  void SetLogonTime();
  [[nodiscard]] std::chrono::system_clock::time_point system_logon_time() const { return system_logon_time_; }
  [[nodiscard]] std::chrono::seconds duration_used_this_session() const;

  [[nodiscard]] std::chrono::seconds extratimecall() const;
  std::chrono::seconds set_extratimecall(std::chrono::duration<double> et);
  std::chrono::seconds add_extratimecall(std::chrono::duration<double> et);
  std::chrono::seconds subtract_extratimecall(std::chrono::duration<double> et);

  /*!
   * @function Run main bbs loop - Invoked from the application
   *           main method.
   * @param argc The number of arguments
   * @param argv arguments
   */
  int Run(int argc, char* argv[]) override;

  int ExitBBSImpl(int exit_level, bool perform_shutdown);

  [[nodiscard]] bool InitializeBBS(bool cleanup_network); // old init() method
  void ReadINIFile(wwiv::core::IniFile& ini); // from xinit.cpp
  bool ReadInstanceSettings(int instance_number, wwiv::core::IniFile& ini);
  bool ReadConfig();

public:
  // Data from system_operation_rec, make it public for now, and add
  // accessors later on.
  int chatname_color_{0};
  int message_color_{0};

  uint16_t forced_read_subnum_{0};
  bool allow_cc_ = false;
  bool user_online_{false};
  bool quoting_ = false;
  bool m_bTimeOnlineLimited{false};

  bool newscan_at_login_{false};
  bool internal_zmodem_{true};
  bool internal_fsed_;
  bool exec_log_syncfoss_{true};
  int m_nNumMessagesReadThisLogon{0};
  int m_nCurrentLanguageNumber{0};
  uint16_t user_dir_num_{0};
  uint16_t user_sub_num_{0};
  // This one should stay in int since -1 is an allowed value.
  int current_read_message_area{0};
  uint16_t current_conf_msgarea_{0};
  uint16_t current_conf_filearea_{0};
  int m_nNumMsgsInCurrentSub{0};

  int beginday_node_number_{1};
  int exec_child_process_wait_time_{500};
  int m_nMaxNumberMessageAreas{0};
  int m_nMaxNumberFileAreas{0};
  int network_num_{0};
  int m_nMaxNetworkNumber{0};
  int subchg{0};
  int topdata{0};
  int using_modem{0};
  int screenlinest{25};
  int defscreenbottom{24};

  std::string internetPopDomain;
  std::string internetEmailDomain;
  std::string internetEmailName;
  std::string internetFullEmailAddress;
  std::string usenetReferencesLine;
  bool m_bInternetUseRealNames{false};
  std::string language_dir;
  std::string cur_lang_name;
  std::string chat_reason_;
  std::string net_email_name;
  std::string temp_directory_;
  std::string batch_directory_;
  uint8_t primary_port_{1};
  std::string dsz_logfile_name_;

  uint16_t usernum{};

  asv_rec asv{};

  uint16_t mail_who_field_len{0};

  uint16_t max_batch{0};
  uint16_t max_extend_lines{0};
  uint16_t max_chains{0};
  uint16_t max_gfilesec{0};
  uint16_t screen_saver_time{0};

  std::vector<uint8_t> newuser_colors;
  std::vector<uint8_t> newuser_bwcolors;

  // public data structures
  std::vector<editorrec> editors;
  std::unique_ptr<wwiv::sdk::Chains> chains;

  std::vector<newexternalrec> externs;
  std::vector<newexternalrec> over_intern;
  std::vector<languagerec> languages;
  std::vector<gfiledirrec> gfilesec;
  std::vector<arcrec> arcs;
  std::vector<usersubrec> usub;
  std::vector<usersubrec> udir;
  std::vector<tagrec_t> filelist;
  std::vector<confrec> subconfs;
  std::vector<confrec> dirconfs;

  std::vector<userconfrec> uconfsub;
  std::vector<userconfrec> uconfdir;

  std::string beginday_cmd;     // beginday event
  std::string logon_cmd;        // logon event
  std::string logoff_cmd;       // logoff event
  std::string cleanup_cmd;       // logoff event
  std::string newuser_cmd;      // newuser event
  std::string upload_cmd;       // upload event
  std::string terminal_command; // Terminal command

  // TODO(rushfan): Make this private. This is needed by WFC
  uint16_t unx_{};

  // TODO(rushfan): All of these are moved from vars.h.
  // Figure out a better way
  bool received_short_message_{false};
  bool emchg_{false};
  int chatting_{0};
  bool no_hangup_{false};
  bool in_chatroom_{false};
  bool chatline_{false};
  int modem_speed_{0};

protected:
  /*!
   * @function GetCaller WFC Screen loop
   */
  bool GetCaller();

  /*!
   * @function GotCaller login routines
   * @param ms Modem Speed (may be a locked speed)
   */
  void GotCaller(int ms);

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
  // Constructor parameters

  std::unique_ptr<LocalIO> local_io_;
  int oklevel_;
  int errorlevel_;
  wwiv::bbs::SessionContext session_context_;

  /*!
   * The current working directory.
   * Please note that bbs_dir_string_ must be updated along with bbs_dir_. This is so we can
   * return a string version of it without risk of exception.
   */
  std::filesystem::path bbs_dir_;
  std::string bbs_dir_string_;
  std::string bindir_;
  std::string configdir_;
  std::string logdir_;
  int verbose_{0};

  int instance_number_{-1};
  std::string network_extension_;
  bool user_already_on_{false};
  bool at_wfc_{false};
  bool chatcall_{false};

  std::unique_ptr<wwiv::sdk::StatusMgr> statusMgr;
  std::unique_ptr<wwiv::sdk::UserManager> user_manager_;
  std::string attach_dir_;
  std::unique_ptr<wwiv::sdk::User> thisuser_;
  int effective_sl_{0};
  std::unique_ptr<RemoteIO> comm_;
  std::string current_speed_;
  std::unique_ptr<wwiv::sdk::Config> config_;
  std::unique_ptr<wwiv::sdk::Names> names_;
  std::map<int, std::unique_ptr<wwiv::sdk::msgapi::MessageApi>> msgapis_;
  std::unique_ptr<wwiv::sdk::files::FileApi> fileapi_;
  std::unique_ptr<wwiv::sdk::files::FileArea> file_area_;

  Batch batch_;
  std::unique_ptr<wwiv::sdk::Subs> subs_;
  std::unique_ptr<wwiv::sdk::files::Dirs> dirs_;
  std::unique_ptr<wwiv::sdk::Networks> nets_;

  // Former global variables and system_operation_rec members to be moved
  uint32_t flags_{0};
  std::map<const std::string, uint16_t> spawn_opts_;
  bool full_screen_read_prompt_{true};
  int last_read_user_number_{0};
  std::chrono::system_clock::time_point system_logon_time_;
  std::chrono::duration<double> extratimecall_{};
};

#endif // #if !defined (__INCLUDED_BBS_APPLICATION_H__)
