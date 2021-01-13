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
#ifndef INCLUDED_SDK_STATUS_H
#define INCLUDED_SDK_STATUS_H

#include "core/file.h"
#include "core/strings.h"
#include "sdk/vardec.h"

#include <functional>
#include <memory>
#include <string>

namespace wwiv::sdk {

class Status {
  friend class StatusMgr;

public:
  Status(const std::string& datadir, const statusrec_t& s);
  ~Status();

  /** If the caller number variable is using the old (pre 4.2) location, move it to the new location
   */
  void EnsureCallerNumberIsValid();
  /** Checks for corruption in the date strings, and try to fix if the date strings are corrupt */
  void ValidateAndFixDates();

  /** Updates the status object for a new day.  This zeros out daily stats and updates the log file
   * names */
  bool NewDay();

  [[nodiscard]] std::string GetLastDate(int days_ago = 0) const;
  [[nodiscard]] std::string GetLogFileName(int nDaysAgo = 0) const;
  [[nodiscard]] std::string GetGFileDate() const { return status_.gfiledate; }
  void SetGFileDate(const std::string& s) { strings::to_char_array(status_.gfiledate, s); }
  [[nodiscard]] char GetFileChangedFlag(int nFlag) const { return status_.filechange[nFlag]; }
  void IncrementFileChangedFlag(int nFlag) { status_.filechange[nFlag]++; }

  [[nodiscard]] unsigned short GetNumLocalPosts() const { return status_.localposts; }
  [[nodiscard]] int IncrementNumLocalPosts() { return status_.localposts++; }
  void SetNumLocalPosts(int n) { status_.localposts = static_cast<uint16_t>(n); }

  [[nodiscard]] int GetNumUsers() const { return status_.users; }
  int IncrementNumUsers() { return status_.users++; }
  int DecrementNumUsers() { return status_.users--; }
  void SetNumUsers(int n) { status_.users = static_cast<uint16_t>(n); }

  [[nodiscard]] unsigned long GetCallerNumber() const { return status_.callernum1; }
  unsigned long IncrementCallerNumber() { return status_.callernum1++; }
  void SetCallerNumber(unsigned long l) { status_.callernum1 = l; }

  [[nodiscard]] unsigned short GetNumCallsToday() const { return status_.callstoday; }
  int IncrementNumCallsToday() { return status_.callstoday++; }
  void SetNumCallsToday(int n) { status_.callstoday = static_cast<uint16_t>(n); }

  [[nodiscard]] int GetNumMessagesPostedToday() const { return status_.msgposttoday; }
  int IncrementNumMessagesPostedToday() { return status_.msgposttoday++; }
  void SetNumMessagesPostedToday(int n) { status_.msgposttoday = static_cast<uint16_t>(n); }

  [[nodiscard]] unsigned short GetNumEmailSentToday() const { return status_.emailtoday; }
  int IncrementNumEmailSentToday() { return status_.emailtoday++; }
  void SetNumEmailSentToday(int n) { status_.emailtoday = static_cast<uint16_t>(n); }

  [[nodiscard]] unsigned short GetNumFeedbackSentToday() const { return status_.fbacktoday; }
  int IncrementNumFeedbackSentToday() { return status_.fbacktoday++; }
  void SetNumFeedbackSentToday(int n) { status_.fbacktoday = static_cast<uint16_t>(n); }

  [[nodiscard]] unsigned short GetNumUploadsToday() const { return status_.uptoday; }
  int IncrementNumUploadsToday() { return status_.uptoday++; }
  void SetNumUploadsToday(int n) { status_.uptoday = static_cast<uint16_t>(n); }

  [[nodiscard]] unsigned short GetMinutesActiveToday() const { return status_.activetoday; }
  void SetMinutesActiveToday(int n) { status_.activetoday = static_cast<uint16_t>(n); }

 [[nodiscard]]  uint32_t GetQScanPointer() const { return status_.qscanptr; }
  uint32_t IncrementQScanPointer() { return status_.qscanptr++; }
  void SetQScanPointer(uint32_t l) { status_.qscanptr = l; }

  [[nodiscard]] bool IsAutoMessageAnonymous() const { return status_.amsganon ? true : false; }
  void SetAutoMessageAnonymous(bool b) { status_.amsganon = (b) ? 1 : 0; }

  [[nodiscard]] int GetAutoMessageAuthorUserNumber() const { return status_.amsguser; }
  void SetAutoMessageAuthorUserNumber(int n) { status_.amsguser = static_cast<uint16_t>(n); }

  [[nodiscard]] int GetWWIVVersion() const { return status_.wwiv_version; }
  void SetWWIVVersion(int n) { status_.wwiv_version = static_cast<uint16_t>(n); }

  [[nodiscard]] int GetNetworkVersion() const { return status_.net_version; }
  [[nodiscard]] int GetDays() const { return status_.days; }
  void SetDays(int n) { status_.days = static_cast<uint16_t>(n); }

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
  StatusMgr(const std::string& datadir, status_callabck_fn callback)
      : datadir_(datadir), callback_(std::move(callback)) {}
  StatusMgr(const std::string& datadir)
      : datadir_(datadir) {}
  virtual ~StatusMgr() = default;
  /*!
   * @function Read Loads the contents of STATUS.DAT
   */
  bool RefreshStatusCache();

  /**
   * Gets the status object with no locks.  Just delete it when finished
   */
  std::unique_ptr<Status> GetStatus();

  void AbortTransaction(std::unique_ptr<Status> pStatus);

  /**
   * Replacement for Lock
   */
  std::unique_ptr<Status> BeginTransaction();

  /**
   * Replacement for Write
   */
  bool CommitTransaction(std::unique_ptr<Status> pStatus);

  [[nodiscard]] int GetUserCount();

  bool Run(status_txn_fn fn);

private:
  std::unique_ptr<wwiv::core::File> status_file_;
  const std::string datadir_;
  status_callabck_fn callback_;
  statusrec_t statusrec_{};
  bool Write(statusrec_t* pStatus);
  /*!
   * @function Get Loads the contents of STATUS.DAT with
   *           control on failure and lock mode
   * @param bLockFile Acquires write lock
   * @return true on success
   */
  bool Get(bool bLockFile);
};

} // namespace

#endif
