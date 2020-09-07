/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2018-2020, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_CONTEXT_H__
#define __INCLUDED_BBS_CONTEXT_H__

#include "core/file.h"
#include "core/wwivport.h"
#include "sdk/config.h"
#include "sdk/user.h"
#include "sdk/files/dirs.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

class LocalIO;

namespace wwiv::bbs {

class Dirs {
public:
  explicit Dirs(const std::filesystem::path& bbsdir);
  Dirs(const std::string& temp, const std::string& batch, const std::string& qwk)
      : temp_directory_(temp), batch_directory_(batch), qwk_directory_(qwk) {}

  [[nodiscard]] const std::string& temp_directory() const noexcept { return temp_directory_; }
  [[nodiscard]] const std::string& batch_directory() const noexcept { return batch_directory_; }

  /**
   * Used instead of QWK_DIRECTORY.  Today it is the same as batch but wanted to
   * leave it open for changing in the future.
   */
  [[nodiscard]] const std::string& qwk_directory() const noexcept { return qwk_directory_; }

  [[nodiscard]] const std::string& language_directory() const noexcept {
    return language_directory_;
  }
  void language_directory(const std::string& l) {
    language_directory_ = l;
  }

private:
  std::string temp_directory_;
  std::string batch_directory_;
  std::string qwk_directory_;
  std::string language_directory_;
};

enum class chatting_t { none, one_way, two_way };

class SessionContext {
public:
  explicit SessionContext(LocalIO* io);
  virtual ~SessionContext() = default;

  /**
   * Initializes an empty context, called after Config.dat
   * has been read and processed.
   */
  void InitalizeContext(const wwiv::sdk::Config& config);
  /**
   * Clears the qscan pointers.
   */
  void ResetQScanPointers(const wwiv::sdk::Config& config);

  /**
   * Resets the application level context.
   */
  void reset();

  /** Resets the local IO pointer */
  void reset_local_io(LocalIO* io);

  bool ok_modem_stuff() const noexcept { return ok_modem_stuff_; }
  void ok_modem_stuff(bool o) { ok_modem_stuff_ = o; }

  bool incom() const noexcept { return incom_; }
  void incom(bool i) { incom_ = i; }
  bool outcom() const noexcept { return outcom_; }
  void outcom(bool o) { outcom_ = o; }

  bool okmacro() const noexcept { return okmacro_; }
  void okmacro(bool o) { okmacro_ = o; }

  bool forcescansub() const noexcept { return forcescansub_; }
  void forcescansub(bool g) { forcescansub_ = g; }
  bool guest_user() const noexcept { return guest_user_; }
  void guest_user(bool g) { guest_user_ = g; }

  daten_t nscandate() const noexcept { return nscandate_; }
  void nscandate(daten_t d) { nscandate_ = d; }

  bool disable_conf() const noexcept { return disable_conf_; }
  void disable_conf(bool b) { disable_conf_ = b; }
  bool disable_pause() const noexcept { return disable_pause_; }
  void disable_pause(bool b) { disable_pause_ = b; }
  bool scanned_files() const noexcept { return scanned_files_; }
  void scanned_files(bool b) { scanned_files_ = b; }
  bool made_find_str() const noexcept { return made_find_str_; }
  void made_find_str(bool b) { made_find_str_ = b; }
  
  // qsc is the qscan pointer. The 1st 4 bytes are the sysop sub number.
  uint32_t* qsc{nullptr};
  // A bitfield controlling if the directory should be included in the new scan.
  uint32_t* qsc_n{nullptr};
  // A bitfield contorlling if the sub should be included in the new scan.
  uint32_t* qsc_q{nullptr};
  // Array of 32-bit unsigned integers for the qscan pointer value
  // aka high message read pointer) for each sub.
  uint32_t* qsc_p{nullptr};

  std::string irt() const { return std::string(irt_); }
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
  void dirs(Dirs d) { dirs_ = d; }

  [[nodiscard]] const wwiv::sdk::files::directory_t& current_dir() const { return current_dir_; }
  void current_dir(const wwiv::sdk::files::directory_t& dir) { current_dir_ = dir; }

  // TODO(rushfan): Move this to private later
  char irt_[81];

private:
  LocalIO* io_;
  std::unique_ptr<uint32_t[]> qscan_;

  bool ok_modem_stuff_{false};
  bool incom_{false};
  bool outcom_{false};
  bool okmacro_{true};
  bool forcescansub_{false};
  bool guest_user_{false};
  daten_t nscandate_{0};
  bool disable_conf_{false};
  bool disable_pause_{false};
  bool scanned_files_{false};
  bool made_find_str_{false};
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
  wwiv::sdk::files::directory_t current_dir_;
};

class Context {
public:
  virtual ~Context() = default;
  virtual wwiv::sdk::User& u() = 0;
  virtual wwiv::bbs::SessionContext& session_context() = 0;
  virtual bool mci_enabled() const = 0;
  virtual const wwiv::sdk::Config& config() const = 0;
};

} // namespace wwiv::bbs

#endif // __INCLUDED_BBS_CONTEXT_H__