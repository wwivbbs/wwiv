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
#ifndef __INCLUDED_SDK_STATUS_H__
#define __INCLUDED_SDK_STATUS_H__

#include <functional>
#include <string>

#include "sdk/vardec.h"
#include "core/file.h"

namespace wwiv {
namespace sdk {

class WStatus {
  friend class StatusMgr;

public:
  static constexpr int fileChangeNames = 0;
  static constexpr int fileChangeUpload = 1;
  static constexpr int fileChangePosts = 2;
  static constexpr int fileChangeEmail = 3;
  static constexpr int fileChangeNet = 4;

private:
  statusrec_t* status_;

public:
  WStatus(const std::string& datadir, statusrec_t* pStatusRecord);
  WStatus(const std::string& datadir);
  ~WStatus();

  /** If the caller number variable is using the old (pre 4.2) location, move it to the new location */
  void EnsureCallerNumberIsValid();
  /** Checks for corruption in the date strings, and try to fix if the date strings are corrupt */
  void ValidateAndFixDates();

  /** Updates the status object for a new day.  This zeros out daily stats and updates the log file names */
  bool NewDay();

  const char* GetLastDate(int nDaysAgo = 0) const;
  const char* GetLogFileName(int nDaysAgo = 0) const;
  const char* GetGFileDate() const {
    return status_->gfiledate;
  }
  void SetGFileDate(const char *s) {
    strcpy(status_->gfiledate, s);
  }
  const char  GetFileChangedFlag(int nFlag) const {
    return status_->filechange[nFlag];
  }
  void IncrementFileChangedFlag(int nFlag) {
    status_->filechange[nFlag]++;
  }

  const unsigned short GetNumLocalPosts() const {
    return status_->localposts;
  }
  const int IncrementNumLocalPosts() {
    return status_->localposts++;
  }
  void SetNumLocalPosts(int n) {
    status_->localposts = static_cast<uint16_t>(n);
  }

  const int GetNumUsers() const {
    return status_->users;
  }
  const int IncrementNumUsers() {
    return status_->users++;
  }
  const int DecrementNumUsers() {
    return status_->users--;
  }
  void SetNumUsers(int n) {
    status_->users = static_cast<uint16_t>(n);
  }

  const unsigned long GetCallerNumber() const {
    return status_->callernum1;
  }
  const unsigned long  IncrementCallerNumber() {
    return status_->callernum1++;
  }
  void SetCallerNumber(unsigned long l) {
    status_->callernum1 = l;
  }

  const unsigned short GetNumCallsToday() const {
    return status_->callstoday;
  }
  const int IncrementNumCallsToday() {
    return status_->callstoday++;
  }
  void SetNumCallsToday(int n) {
    status_->callstoday = static_cast<uint16_t>(n);
  }

  const int GetNumMessagesPostedToday() const {
    return status_->msgposttoday;
  }
  const int IncrementNumMessagesPostedToday() {
    return status_->msgposttoday++;
  }
  void SetNumMessagesPostedToday(int n) {
    status_->msgposttoday = static_cast<uint16_t>(n);
  }

  const unsigned short GetNumEmailSentToday() const {
    return status_->emailtoday;
  }
  const int IncrementNumEmailSentToday() {
    return status_->emailtoday++;
  }
  void SetNumEmailSentToday(int n) {
    status_->emailtoday = static_cast<uint16_t>(n);
  }

  const unsigned short GetNumFeedbackSentToday() const {
    return status_->fbacktoday;
  }
  const int IncrementNumFeedbackSentToday() {
    return status_->fbacktoday++;
  }
  void SetNumFeedbackSentToday(int n) {
    status_->fbacktoday = static_cast<uint16_t>(n);
  }

  const unsigned short GetNumUploadsToday() const {
    return status_->uptoday;
  }
  const int IncrementNumUploadsToday() {
    return status_->uptoday++;
  }
  void SetNumUploadsToday(int n) {
    status_->uptoday = static_cast<uint16_t>(n);
  }

  const unsigned short GetMinutesActiveToday() const {
    return status_->activetoday;
  }
  const int IncrementMinutesActiveToday(int nMinutes) {
    return status_->uptoday += static_cast<uint16_t>(nMinutes);
  }
  void SetMinutesActiveToday(int n) {
    status_->activetoday = static_cast<uint16_t>(n);
  }

  const uint32_t GetQScanPointer() const {
    return status_->qscanptr;
  }
  const uint32_t IncrementQScanPointer() {
    return status_->qscanptr++;
  }
  void SetQScanPointer(uint32_t l) {
    status_->qscanptr = l;
  }

  const bool IsAutoMessageAnonymous() const {
    return status_->amsganon ? true : false;
  }
  void SetAutoMessageAnonymous(bool b) {
    status_->amsganon = (b) ? 1 : 0;
  }

  const int GetAutoMessageAuthorUserNumber() const {
    return status_->amsguser;
  }
  void SetAutoMessageAuthorUserNumber(int n) {
    status_->amsguser = static_cast<uint16_t>(n);
  }

  const int GetWWIVVersion() const {
    return status_->wwiv_version;
  }
  void SetWWIVVersion(int n) {
    status_->wwiv_version = static_cast<uint16_t>(n);
  }

  const int GetNetworkVersion() const {
    return status_->net_version;
  }
  void SetNetworkVersion(int n) {
    status_->net_version = static_cast<uint16_t>(n);
  }

  const int GetDays() const {
    return status_->days;
  }
  void SetDays(int n) {
    status_->days = static_cast<uint16_t>(n);
  }
private:
  const std::string datadir_;
};



/*!
 * @class StatusMgr Manages STATUS.DAT
 */
class StatusMgr {
public:

  typedef std::function<void(int)> status_callabck_fn;
  typedef std::function<void(WStatus& s)> status_txn_fn;

  /*!
   * @function StatusMgr Constructor
   */
  StatusMgr(const std::string& datadir, status_callabck_fn callback)
    : datadir_(datadir), callback_(callback) {}
  virtual ~StatusMgr() {}
  /*!
   * @function Read Loads the contents of STATUS.DAT
   */
  bool RefreshStatusCache();

  /**
   * Gets the status object with no locks.  Just delete it when finished
   */
  WStatus* GetStatus();

  void AbortTransaction(WStatus* pStatus);

  /**
   * Replacement for Lock
   */
  WStatus* BeginTransaction();

  /**
   * Replacement for Write
   */
  bool CommitTransaction(WStatus* pStatus);

  const int GetUserCount();

  bool Run(status_txn_fn fn);

private:
  File status_file_;
  const std::string datadir_;
  status_callabck_fn callback_;
  bool Write(statusrec_t *pStatus);
  /*!
  * @function Get Loads the contents of STATUS.DAT with
  *           control on failure and lock mode
  * @param bLockFile Aquires write lock
  * @return true on success
  */
  bool Get(bool bLockFile);

};

}
}

#endif // __INCLUDED_SDK_STATUS_H__

