/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_STATUS_H
#define INCLUDED_SDK_STATUS_H

#include "core/strings.h"
#include "sdk/vardec.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace wwiv::sdk {

class Status {
  friend class StatusMgr;

public:
  Status(std::string datadir, const statusrec_t& s);
  ~Status();

  /** If the caller number variable is using the old (pre 4.2) location, move it to the new location
   */
  void ensure_callernum_valid();
  /** Checks for corruption in the date strings, and try to fix if the date strings are corrupt */
  void ensure_dates_valid();

  /** Updates the status object for a new day.  This zeros out daily stats and updates the log file
   * names */
  bool NewDay();

  [[nodiscard]] std::string last_date(int days_ago = 0) const;
  [[nodiscard]] std::string log_filename(int nDaysAgo = 0) const;
  void gfile_date(const std::string& s) { strings::to_char_array(status_.gfiledate, s); }
  [[nodiscard]] uint8_t filechanged(int nFlag) const { return status_.filechange[nFlag]; }
  void increment_filechanged(int nFlag) { status_.filechange[nFlag]++; }

  [[nodiscard]] uint16_t localposts() const { return status_.localposts; }
  void IncrementNumLocalPosts() { status_.localposts++; }
  void localposts(int n) { status_.localposts = static_cast<uint16_t>(n); }

  [[nodiscard]] int num_users() const { return status_.users; }
  void increment_num_users() { status_.users++; }
  void decrement_num_users() { status_.users--; }
  void num_users(int n) { status_.users = static_cast<uint16_t>(n); }

  [[nodiscard]] int caller_num() const { return status_.callernum1; }
  void increment_caller_num() { status_.callernum1++; }
  void caller_num(unsigned long l) { status_.callernum1 = l; }

  [[nodiscard]] uint16_t calls_today() const { return status_.callstoday; }
  void increment_calls_today() { status_.callstoday++; }
  void calls_today(int n) { status_.callstoday = static_cast<uint16_t>(n); }

  [[nodiscard]] uint16_t msgs_today() const { return status_.msgposttoday; }
  void increment_msgs_today() { status_.msgposttoday++; }
  void msgs_today(int n) { status_.msgposttoday = static_cast<uint16_t>(n); }

  [[nodiscard]] uint16_t email_today() const { return status_.emailtoday; }
  void increment_email_today() { status_.emailtoday++; }
  void email_today(int n) { status_.emailtoday = static_cast<uint16_t>(n); }

  [[nodiscard]] uint16_t feedback_today() const { return status_.fbacktoday; }
  void increment_feedback_today() { status_.fbacktoday++; }
  void feedback_today(int n) { status_.fbacktoday = static_cast<uint16_t>(n); }

  [[nodiscard]] uint16_t uploads_today() const { return status_.uptoday; }
  void increment_uploads_today() { status_.uptoday++; }
  void uploads_today(int n) { status_.uptoday = static_cast<uint16_t>(n); }

  [[nodiscard]] uint16_t active_today_minutes() const { return status_.activetoday; }
  void active_today_minutes(int n) { status_.activetoday = static_cast<uint16_t>(n); }

  [[nodiscard]] uint32_t qscanptr() const { return status_.qscanptr; }
  uint32_t next_qscanptr() { return status_.qscanptr++; }
  void qscanptr(uint32_t l) { status_.qscanptr = l; }

  [[nodiscard]] bool automessage_anon() const { return status_.amsganon ? true : false; }
  void automessage_anon(bool b) { status_.amsganon = (b) ? 1 : 0; }

  [[nodiscard]] uint16_t automessage_usernum() const { return status_.amsguser; }
  void automessage_usernum(int n) { status_.amsguser = static_cast<uint16_t>(n); }

  [[nodiscard]] uint16_t status_wwiv_version() const { return status_.wwiv_version; }
  void status_wwiv_version(int n) { status_.wwiv_version = static_cast<uint16_t>(n); }

  [[nodiscard]] uint16_t status_net_version() const { return status_.net_version; }
  [[nodiscard]] uint16_t  days_active() const { return status_.days; }
  void days_active(int n) { status_.days = static_cast<uint16_t>(n); }

  static constexpr int file_change_names = 0;
  static constexpr int file_change_upload = 1;
  static constexpr int file_change_posts = 2;
  static constexpr int file_change_email = 3;
  static constexpr int file_change_net = 4;

private:
  statusrec_t status_;
  const std::string datadir_;
};

/*!
 * @class StatusMgr
 * manages STATUS.DAT
 */
class StatusMgr {
public:
  typedef std::function<void(int)> status_callabck_fn;
  typedef std::function<void(Status& s)> status_txn_fn;

  /*!
   * @function StatusMgr Constructor
   */
  StatusMgr(std::string datadir, status_callabck_fn callback)
      : datadir_(std::move(datadir)), callback_(std::move(callback)) {}
  explicit StatusMgr(std::string datadir) : datadir_(std::move(datadir)) {}
  virtual ~StatusMgr() = default;
  /*!
   * @function Loads the contents of STATUS.DAT
   * @return true on success
   */
  bool reload_status();

  /**
   * Gets the status object with no locks.  Just delete it when finished
   */
  std::unique_ptr<Status> get_status();

  [[nodiscard]] int user_count();

  bool Run(status_txn_fn fn);

private:
  const std::string datadir_;
  status_callabck_fn callback_;
  statusrec_t statusrec_{};
};

} // namespace

#endif
