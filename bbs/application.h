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
#ifndef INCLUDED_BBS_APPLICATION_H
#define INCLUDED_BBS_APPLICATION_H

#include "bbs/batch.h"
#include "bbs/interpret.h"
#include "bbs/runnable.h"
#include "common/context.h"
#include "common/output.h"
#include "sdk/vardec.h"
#include "sdk/net/networks.h"
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

class LocalIO;

struct net_networks_rec;

namespace wwiv {
  namespace common {
enum class CommunicationType;
class RemoteIO;

}
namespace core {
class IniFile;
}
namespace sdk {
struct conference_t;
class Config;
class Conferences;
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

enum class get_caller_t { exit, fast_login, normal_login };
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
  ~Application() override;

  [[nodiscard]] wwiv::sdk::User* user() const { return thisuser_.get(); }
  [[nodiscard]] wwiv::common::Context& context();
  [[nodiscard]] const wwiv::common::Context& context() const;
  [[nodiscard]] wwiv::common::SessionContext& sess();
  [[nodiscard]] const wwiv::common::SessionContext& sess() const;

  void handle_sysop_key(uint8_t key);
  void tleft(bool check_for_timeout);
  void DisplaySysopWorkingIndicator(bool displayWait);
  [[nodiscard]] wwiv::common::RemoteIO* remoteIO() const { return comm_.get(); }
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
  void CreateComm(unsigned int nHandle, wwiv::common::CommunicationType type);
  /**
   * Sets the RemoteIO handle for testing.
   * NOTE: This should only be used in unit tests.
   */
  void SetCommForTest(wwiv::common::RemoteIO* remote_io);

  bool ReadCurrentUser();
  bool ReadCurrentUser(int user_number);
  bool WriteCurrentUser();
  bool WriteCurrentUser(int user_number);

  void reset_effective_sl();
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

  [[nodiscard]] int language_number() const;

  void set_language_number(int n);
  
  [[nodiscard]] int GetNumMessagesReadThisLogon() const { return m_nNumMessagesReadThisLogon; }
  void SetNumMessagesReadThisLogon(int n) { m_nNumMessagesReadThisLogon = n; }

  [[nodiscard]] bool IsNewScanAtLogin() const { return newscan_at_login_; }
  void SetNewScanAtLogin(bool b) { newscan_at_login_ = b; }

  // This is the current user's dir number they are sitting on.
  // This is a user dir number (a()->udir[b], not directories[b]).
  [[nodiscard]] uint16_t current_user_dir_num() const { return user_dir_num_; }
  void set_current_user_dir_num(int n);

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
  // if (a()->sess().GetCurrentReadMessageArea() < 0) { ... }

  [[nodiscard]] const wwiv::sdk::subboard_t& current_sub() const;
  [[nodiscard]] const wwiv::sdk::files::directory_t& current_dir() const;

  [[nodiscard]] const net_networks_rec& current_net() const;

  [[nodiscard]] bool IsUseInternalZmodem() const { return internal_zmodem_; }
  [[nodiscard]] bool IsUseInternalFsed() const; 

  [[nodiscard]] int GetNumMessagesInCurrentMessageArea() const { return m_nNumMsgsInCurrentSub; }
  void SetNumMessagesInCurrentMessageArea(int n) { m_nNumMsgsInCurrentSub = n; }

  [[nodiscard]] int GetBeginDayNodeNumber() const { return beginday_node_number_; }
  void SetBeginDayNodeNumber(int n) { beginday_node_number_ = n; }

  [[nodiscard]] int GetExecChildProcessWaitTime() const { return exec_child_process_wait_time_; }
  void SetExecChildProcessWaitTime(int n) { exec_child_process_wait_time_ = n; }

  [[nodiscard]] bool IsExecLogSyncFoss() const { return exec_log_syncfoss_; }

  [[nodiscard]] int net_num() const { return network_num_; }
  void set_net_num(int n) { network_num_ = n; }

  [[nodiscard]] wwiv::sdk::StatusMgr* status_manager() const { return statusMgr.get(); }
  [[nodiscard]] wwiv::sdk::UserManager* users() const { return user_manager_.get(); }

  [[nodiscard]] const std::filesystem::path& temp_directory() const { return temp_directory_; }
  [[nodiscard]] const std::filesystem::path& batch_directory() const { return batch_directory_; }
  /**
   * Used instead of QWK_DIRECTORY.  Today it is the same as batch but wanted to
   * leave it open for changing in the future.
   */
  [[nodiscard]] const std::filesystem::path& qwk_directory() const { return batch_directory_; }
  [[nodiscard]] uint8_t primary_port() const { return primary_port_; }

  [[nodiscard]] std::filesystem::path bbspath() const noexcept;
  [[nodiscard]] std::string bindir() const noexcept;
  [[nodiscard]] std::string configdir() const noexcept;
  [[nodiscard]] std::string logdir() const noexcept;
  [[nodiscard]] int verbose() const noexcept;

  [[nodiscard]] bool HasConfigFlag(int nFlag) const { return (flags_ & nFlag) != 0; }

  [[nodiscard]] uint16_t spawn_option(const std::string& c) const { return spawn_opts_.at(c); }

  void set_at_wfc(bool b) { at_wfc_ = b; }
  [[nodiscard]] bool at_wfc() const { return at_wfc_; }

  [[nodiscard]] bool fullscreen_read_prompt() const { return full_screen_read_prompt_; }


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

  int usernum() const noexcept { return sess().user_num(); }
  void usernum(int u) { sess().user_num(u); }

  // Public subsystems
  [[nodiscard]] Batch& batch();
  [[nodiscard]] wwiv::sdk::Subs& subs();
  [[nodiscard]] const wwiv::sdk::Subs& subs() const;
  [[nodiscard]] wwiv::sdk::Conferences& all_confs();
  [[nodiscard]] const wwiv::sdk::Conferences& all_confs() const;


  [[nodiscard]] wwiv::sdk::files::Dirs& dirs();
  [[nodiscard]] const wwiv::sdk::files::Dirs& dirs() const;

  [[nodiscard]] wwiv::sdk::Networks& nets();
  [[nodiscard]] const wwiv::sdk::Networks& nets() const;

  bool read_subs();
  bool create_message_api();

  [[nodiscard]] std::chrono::seconds extratimecall() const;
  std::chrono::seconds set_extratimecall(std::chrono::duration<double> et);
  std::chrono::seconds add_extratimecall(std::chrono::duration<double> et);
  std::chrono::seconds subtract_extratimecall(std::chrono::duration<double> et);

  void frequent_init();
  bool CheckForHangup();
  void Hangup();

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
  bool quoting_ = false;

  bool newscan_at_login_{false};
  bool internal_zmodem_{true};
  bool internal_fsed_;
  bool exec_log_syncfoss_{true};
  int m_nNumMessagesReadThisLogon{0};
  int m_nCurrentLanguageNumber{0};
  uint16_t user_dir_num_{0};
  uint16_t user_sub_num_{0};
  // This one should stay in int since -1 is an allowed value.
  int m_nNumMsgsInCurrentSub{0};

  int beginday_node_number_{1};
  int exec_child_process_wait_time_{500};
  int m_nMaxNumberMessageAreas{0};
  int m_nMaxNumberFileAreas{0};
  int network_num_{0};
  int m_nMaxNetworkNumber{0};
  int subchg{0};

  std::string net_email_name;
  std::filesystem::path temp_directory_;
  std::filesystem::path batch_directory_;
  uint8_t primary_port_{1};
  std::string dsz_logfile_name_;

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
  // The current set of subs visible to the user in the current conference.
  // TODO(rusfan): Move into conf
  std::vector<usersubrec> usub;
  // The current set of dirs visible to the user in the current conference.
  std::vector<usersubrec> udir;
  std::vector<tagrec_t> filelist;
  std::unique_ptr<wwiv::sdk::Conferences> all_confs_;

  std::vector<wwiv::sdk::conference_t> uconfsub;
  std::vector<wwiv::sdk::conference_t> uconfdir;

  std::string beginday_cmd;     // beginday event
  std::string logon_cmd;        // logon event
  std::string logoff_cmd;       // logoff event
  std::string cleanup_cmd;       // logoff event
  std::string newuser_cmd;      // newuser event
  std::string upload_cmd;       // upload event
  std::string terminal_command; // Terminal command

  // TODO(rushfan): All of these are moved from vars.h.
  // Figure out a better way
  bool emchg_{false};
  bool no_hangup_{false};
  int modem_speed_{38400};

protected:
  /*!
   * @function GetCaller WFC Screen loop
   */
  get_caller_t GetCaller();

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
  wwiv::common::SessionContext session_context_;

  /*!
   * The current working directory.
   */
  std::filesystem::path bbs_dir_;
  std::string bindir_;
  std::string configdir_;
  std::string logdir_;
  int verbose_{0};

  int instance_number_{-1};
  std::string network_extension_;
  bool user_already_on_{false};
  bool at_wfc_{false};

  std::unique_ptr<wwiv::sdk::StatusMgr> statusMgr;
  std::unique_ptr<wwiv::sdk::UserManager> user_manager_;
  std::string attach_dir_;
  std::unique_ptr<wwiv::sdk::User> thisuser_;
  std::unique_ptr<wwiv::common::RemoteIO> comm_;
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
  std::chrono::duration<double> extratimecall_{};
  std::unique_ptr<wwiv::common::Context> context_;
  BbsMacroContext bbs_macro_context_;
};

#endif