/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2018-2021, WWIV Software Services             */
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
/**************************************************************************/
#ifndef INCLUDED_COMMON_CONTEXT_H
#define INCLUDED_COMMON_CONTEXT_H

#include "core/wwivport.h"
#include "sdk/config.h"
#include "sdk/user.h"
#include "sdk/value/valueprovider.h"
#include "sdk/files/dirs.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace wwiv {
namespace sdk {
class Chains;
}
}

namespace wwiv::local::io {
class LocalIO;
}

namespace wwiv::common {

class Dirs {
public:
  explicit Dirs(const std::filesystem::path& bbsdir);
  Dirs(const std::string& temp, const std::string& batch, const std::string& qwk,
       std::string gfiles, const std::string& scratch)
      : temp_directory_(temp), batch_directory_(batch), qwk_directory_(qwk),
        gfiles_directory_(std::move(gfiles)), scratch_directory_(scratch) {}

  [[nodiscard]] const std::filesystem::path& temp_directory() const noexcept { return temp_directory_; }
  void temp_directory(const std::string& d) { temp_directory_ = d; }

  [[nodiscard]] const std::filesystem::path& batch_directory() const noexcept { return batch_directory_; }
  void batch_directory(const std::string& d) { batch_directory_ = d; }

  [[nodiscard]] const std::filesystem::path& scratch_directory() const noexcept { return scratch_directory_; }

  /**
   * Used instead of QWK_DIRECTORY.  Today it is the same as batch but wanted to
   * leave it open for changing in the future.
   */
  [[nodiscard]] const std::filesystem::path& qwk_directory() const noexcept { return qwk_directory_; }
  void qwk_directory(const std::string& d) { qwk_directory_ = d; }

  [[nodiscard]] const std::string& gfiles_directory() const noexcept { return gfiles_directory_; }
  void gfiles_directory(const std::string& d) { gfiles_directory_ = d; }

  [[nodiscard]] std::filesystem::path current_menu_directory() const noexcept {
    return current_menu_directory_;
  }
  void current_menu_directory(const std::filesystem::path& l) {
    current_menu_directory_ = l;
  }

  /**
   * Current directory for this menus set's gfiles dir. i.e. where menus specific
   * display files reside.
   */
  [[nodiscard]] std::filesystem::path current_menu_gfiles_directory() const noexcept;

  /**
   * Current directory for this menus set's script dir. i.e. where menus specific
   * script files reside.
   */
  [[nodiscard]] std::filesystem::path current_menu_script_directory() const noexcept;

private:
  std::filesystem::path temp_directory_;
  std::filesystem::path batch_directory_;
  std::filesystem::path qwk_directory_;
  std::filesystem::path current_menu_directory_;
  std::string gfiles_directory_;
  std::filesystem::path scratch_directory_;
};

enum class chatting_t { none, one_way, two_way };

class SessionContext final {
public:
  explicit SessionContext(wwiv::local::io::LocalIO* io);
  SessionContext(wwiv::local::io::LocalIO* io, const std::filesystem::path& root_directory);
  ~SessionContext() = default;

  /**
   * Initializes an empty context, called after Config.dat
   * has been read and processed.
   */
  void InitalizeContext(const sdk::Config& config);

  /**
   * Clears the qscan pointers.
   */
  void ResetQScanPointers(const wwiv::sdk::Config& config);

  /**
   * Resets the application level context.
   */
  void reset();

  /** Resets the local IO pointer */
  void reset_local_io(wwiv::local::io::LocalIO* io);

  [[nodiscard]] bool ok_modem_stuff() const noexcept { return ok_modem_stuff_; }
  void ok_modem_stuff(bool o) { ok_modem_stuff_ = o; }

  [[nodiscard]] int instance_number() const { return instance_number_; }
  void instance_number(int i) { instance_number_ = i; }

  [[nodiscard]] bool incom() const noexcept { return incom_; }
  void incom(bool i) { incom_ = i; }
  [[nodiscard]] bool outcom() const noexcept { return outcom_; }
  void outcom(bool o) { outcom_ = o; }

  [[nodiscard]] bool okmacro() const noexcept { return okmacro_; }
  void okmacro(bool o) { okmacro_ = o; }

  [[nodiscard]] bool forcescansub() const noexcept { return forcescansub_; }
  void forcescansub(bool g) { forcescansub_ = g; }

  [[nodiscard]] daten_t nscandate() const noexcept { return nscandate_; }
  void nscandate(daten_t d) { nscandate_ = d; }

  [[nodiscard]] bool disable_conf() const noexcept { return disable_conf_; }
  void disable_conf(bool b) { disable_conf_ = b; }
  [[nodiscard]] bool disable_pause() const noexcept { return disable_pause_; }
  void disable_pause(bool b) { disable_pause_ = b; }
  [[nodiscard]] bool scanned_files() const noexcept { return scanned_files_; }
  void scanned_files(bool b) { scanned_files_ = b; }
  
  // qsc is the qscan pointer. The 1st 4 bytes are the sysop sub number.
  uint32_t* qsc{nullptr};
  // A bitfield controlling if the directory should be included in the new scan.
  uint32_t* qsc_n{nullptr};
  // A bitfield controlling if the sub should be included in the new scan.
  uint32_t* qsc_q{nullptr};
  // Array of 32-bit unsigned integers for the qscan pointer value
  // aka high message read pointer) for each sub.
  uint32_t* qsc_p{nullptr};

  [[nodiscard]] std::string irt() const { return std::string(irt_); }
  void irt(const std::string& irt);
  void clear_irt() { irt_[0] = '\0'; }

  [[nodiscard]] bool hangup() const noexcept { return hangup_; }
  void hangup(bool h) { hangup_ = h; }

  [[nodiscard]] int num_screen_lines() const noexcept { return num_screen_lines_; }
  void num_screen_lines(int n) { num_screen_lines_ = n; }

  [[nodiscard]] chatting_t chatting() const noexcept { return chatting_; }
  void chatting(chatting_t n) { chatting_ = n; }

  [[nodiscard]] bool using_modem() const noexcept { return using_modem_; }
  void using_modem(bool n) { using_modem_ = n; }

  void chat_reason(const std::string& chat_reason) {
    chat_reason_ = chat_reason;
    chatcall_ = !chat_reason.empty();
  }

  [[nodiscard]] std::string chat_reason() const noexcept { return chat_reason_; }

  /** Is the chat call alert (user wanted to chat with the sysop. enabled? */
  [[nodiscard]] bool chatcall() const { return chatcall_; }
  /** Clears the chat call alert (user wanted to chat with the sysop. enabled? */
  void clear_chatcall() { chatcall_ = false; }

  [[nodiscard]] bool chatline() const { return chatline_; }
  void chatline(bool n) { chatline_ = n; }

  [[nodiscard]] bool in_chatroom() const { return in_chatroom_; }
  void in_chatroom(bool n) { in_chatroom_ = n; }

  // Note: This may be set to -1 to mean no area.
  [[nodiscard]] int GetCurrentReadMessageArea() const { return current_read_message_area; }
  void SetCurrentReadMessageArea(int n) { current_read_message_area = n; }

  [[nodiscard]] uint16_t current_user_sub_conf_num() const { return current_conf_msgarea_; }
  void set_current_user_sub_conf_num(int n) { current_conf_msgarea_ = static_cast<uint16_t>(n); }

  [[nodiscard]] uint16_t current_user_dir_conf_num() const { return current_conf_filearea_; }
  void set_current_user_dir_conf_num(int n) { current_conf_filearea_ = static_cast<uint16_t>(n); }

  [[nodiscard]] bool IsUserOnline() const { return user_online_; }
  void SetUserOnline(bool b) { user_online_ = b; }

  [[nodiscard]] bool IsTimeOnlineLimited() const { return time_limited_; }
  void SetTimeOnlineLimited(bool b) { time_limited_ = b; }

  // Sets the time that the user has logged in.
  void SetLogonTime();

  // Returns the time that the user has logged in.  If the user has not yet
  // logged in, return the time the bbs started.
  [[nodiscard]] std::chrono::system_clock::time_point system_logon_time() const {
    return system_logon_time_;
  }

  // Returns the duration used this bbs session.
  [[nodiscard]] std::chrono::seconds duration_used_this_session() const;

  [[nodiscard]] const Dirs& dirs() const noexcept { return dirs_; }
  [[nodiscard]] Dirs& dirs() { return dirs_; }
  void dirs(Dirs d) { dirs_ = std::move(d); }

  [[nodiscard]] const wwiv::sdk::files::directory_t& current_dir() const { return current_dir_; }
  void current_dir(const wwiv::sdk::files::directory_t& dir) { current_dir_ = dir; }

  void effective_sl(int n) { effective_sl_ = n; }
  [[nodiscard]] int effective_sl() const { return effective_sl_; }

  void current_language(const std::string& l) { current_lang_name_ = l; }
  [[nodiscard]] std::string current_language() const noexcept { return current_lang_name_; }

  void user_num(int usernum) { user_num_ = usernum; }
  [[nodiscard]] int user_num() const noexcept { return user_num_; };

  /**
   * Sets the effective BPS to use for the system.  This ay be overridden by
   * on a per file basis.
   */
  void set_system_bps(int bps) { system_bps_ = bps; }

  /**
   * The effective BPS to use for the system.  This ay be overridden by on a
   * per file basis.
   */
  [[nodiscard]] int system_bps() const noexcept { return system_bps_; }

  /**
   * Sets the effective BPS to use for the current file.  This will be reset
   * after the file is complete.
   */
  void set_file_bps(int bps) { file_bps_ = bps; }

  /**
   * The effective BPS to use for the current file.  This will be reset
   * after the file is complete.
   */
  [[nodiscard]] int file_bps() const noexcept { return file_bps_; }

  /**
   * The effective BPS to use to display files.
   *
   * If the current file sets an explicit BPS then we'll use that, otherwise
   * we'll use the system BPS.
   */
  [[nodiscard]] int bps() const noexcept;

  /**
   * The current user's menus set
   */
  [[nodiscard]] std::string current_menu_set() const noexcept;

  void current_menu_set(const std::string& menuset);


  // TODO(rushfan): Move this to private later
  char irt_[81];

private:
  wwiv::local::io::LocalIO* io_;
  std::unique_ptr<uint32_t[]> qscan_;

  bool ok_modem_stuff_{false};
  int instance_number_{-1};
  bool incom_{false};
  bool outcom_{false};
  bool okmacro_{true};
  bool forcescansub_{false};
  daten_t nscandate_{0};
  bool disable_conf_{false};
  bool disable_pause_{false};
  bool scanned_files_{false};
  bool hangup_{false};
  int num_screen_lines_{25};
  chatting_t chatting_{chatting_t::none};
  bool using_modem_{false};
  bool chatcall_{false};
  std::string chat_reason_;
  bool chatline_{false};
  bool in_chatroom_{false};

  int current_read_message_area{0};
  uint16_t current_conf_msgarea_{0};
  uint16_t current_conf_filearea_{0};
  bool user_online_{false};
  bool time_limited_{false};
  std::chrono::system_clock::time_point system_logon_time_;
  Dirs dirs_;
  sdk::files::directory_t current_dir_{};
  int effective_sl_{0};
  std::string current_lang_name_{"English"};
  int user_num_{-1};

  int file_bps_{0};
  int system_bps_{0};
  std::string current_menu_set_;
};

class MapValueProvider : public sdk::value::ValueProvider {
public:
  explicit MapValueProvider(std::string prefix, std::map<std::string, std::string> map)
      : ValueProvider(std::move(prefix)), map_(std::move(map)) {}

  ~MapValueProvider() override = default;

  /** 
   * Optionally gets the attribute for this object.  name should just be
   * the 'attribute' and not the full object.attribute name. *
   */
  [[nodiscard]] std::optional<sdk::value::Value> value(const std::string& name) const override;

  [[nodiscard]] const std::map<std::string, std::string>& map() const;

private:
  const std::map<std::string, std::string> map_;
};


/**
 * WWIV Full Context.
 *
 * This is a smaller context class than what the Application class contains,
 * and should be enough to run displays and most activities outside of the BBS.
 *
 * This holds other sub-contexts (like SessionContext), as well as the current
 * User and Config used by the BBS.
 */
class Context {
public:
  virtual ~Context() = default;
  [[nodiscard]] virtual sdk::Config& config() = 0;
  [[nodiscard]] virtual sdk::User& u() = 0;
  [[nodiscard]] virtual SessionContext& session_context() = 0;
  [[nodiscard]] virtual bool mci_enabled() const = 0;
  [[nodiscard]] virtual const std::vector<editorrec>& editors() const = 0;
  [[nodiscard]] virtual const sdk::Chains& chains() const = 0;


  /**
   * Adds a set of context variables for use in pipe expressions.  An example for the current
   * message being read may be:
   * add_context_variable("msg", { { "num", "100" }, { "title", "A message on nothing" } });
   */
  bool add_context_variable(const std::string& prefix, std::map<std::string, std::string> map);
  bool remove_context_variable(const std::string& prefix);
  bool clear_context_variables();
  [[nodiscard]] const std::vector<std::unique_ptr<MapValueProvider>>& value_providers() const { return value_providers_; }

  /**
   * Return values to be returned from scripts to the BBS.
   * example: num_header_lines is returned from the full screen reader and editor.
   */
  [[nodiscard]] std::map<std::string, std::string>& return_values() { return return_values_; }

private:
  std::vector<std::unique_ptr<MapValueProvider>> value_providers_;
  std::map<std::string, std::string> return_values_;
};

} // namespace wwiv::common

#endif // INCLUDED_COMMON_CONTEXT_H