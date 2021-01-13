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
#include "sdk/status.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/filenames.h"
#include <memory>
#include <string>

using std::string;
using std::unique_ptr;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::sdk {

static string GetSysopLogFileName(const string& d) {
  return fmt::sprintf("%c%c%c%c%c%c.log", d[6], d[7], d[0], d[1], d[3], d[4]);
}

Status::Status(const std::string& datadir, const statusrec_t& s)
    : status_(s), datadir_(datadir) {}

Status::~Status() = default;

std::string Status::GetLastDate(int days_ago) const {
  DCHECK_GE(days_ago, 0);
  DCHECK_LE(days_ago, 2);
  switch (days_ago) {
  case 0:
    return status_.date1;
  case 1:
    return status_.date2;
  case 2:
    return status_.date3;
  default:
    return status_.date1;
  }
}

std::string Status::GetLogFileName(int nDaysAgo) const {
  DCHECK_GE(nDaysAgo, 0);
  switch (nDaysAgo) {
  case 0:
  {
    return GetSysopLogFileName(DateTime::now().to_string("%m/%d/%y"));
  }
  case 1:
    return status_.log1;
  case 2:
    return status_.log2;
  default:
    return status_.log1;
  }
}

void Status::EnsureCallerNumberIsValid() {
  if (status_.callernum != 65535) {
    this->SetCallerNumber(status_.callernum);
    status_.callernum = 65535;
  }
}

void Status::ValidateAndFixDates() {
  if (status_.date1[8] != '\0') {
    status_.date1[6] = '0';
    status_.date1[7] = '0';
    status_.date1[8] = '\0'; // forgot to add null termination
  }

  auto currentDate = DateTime::now().to_string("%m/%d/%y");
  if (status_.date3[8] != '\0') {
    status_.date3[6] = currentDate[6];
    status_.date3[7] = currentDate[7];
    status_.date3[8] = '\0';
  }
  if (status_.date2[8] != '\0') {
    status_.date2[6] = currentDate[6];
    status_.date2[7] = currentDate[7];
    status_.date2[8] = '\0';
  }
  if (status_.date1[8] != '\0') {
    status_.date1[6] = currentDate[6];
    status_.date1[7] = currentDate[7];
    status_.date1[8] = '\0';
  }
  if (status_.gfiledate[8] != '\0') {
    status_.gfiledate[6] = currentDate[6];
    status_.gfiledate[7] = currentDate[7];
    status_.gfiledate[8] = '\0';
  }
}

bool Status::NewDay() {
  status_.callstoday = 0;
  status_.msgposttoday = 0;
  status_.localposts = 0;
  status_.emailtoday = 0;
  status_.fbacktoday = 0;
  status_.uptoday = 0;
  status_.activetoday = 0;
  status_.days++;

  // Need to verify the dates aren't trashed otherwise we can crash here.
  ValidateAndFixDates();

  strcpy(status_.date3, status_.date2);
  strcpy(status_.date2, status_.date1);
  const auto d = DateTime::now().to_string("%m/%d/%y");
  to_char_array(status_.date1, d);
  strcpy(status_.log2, status_.log1);

  const auto log = GetSysopLogFileName(GetLastDate(1));
  to_char_array(status_.log1, log);
  return true;
}

// StatusMgr
bool StatusMgr::Get(bool bLockFile) {
  if (!status_file_) {
    status_file_.reset(new File(FilePath(datadir_, STATUS_DAT)));
    const auto lock_mode = (bLockFile) ? (File::modeReadWrite | File::modeBinary) : (File::modeReadOnly | File::modeBinary);
    status_file_->Open(lock_mode);
  } else {
    status_file_->Seek(0L, File::Whence::begin);
  }
  if (!status_file_->IsOpen()) {
    return false;
  }
  char oldFileChangeFlags[7];
  for (auto nFcIndex = 0; nFcIndex < 7; nFcIndex++) {
    oldFileChangeFlags[nFcIndex] = statusrec_.filechange[nFcIndex];
  }
  status_file_->Read(&statusrec_, sizeof(statusrec_t));

  if (!bLockFile) {
    status_file_.reset();
  }

  for (int i = 0; i < 7; i++) {
    if (oldFileChangeFlags[i] != statusrec_.filechange[i]) {
      // Invoke callback on changes.
      if (callback_) {
        callback_(i);
      }
    }
  }
  return true;
}

bool StatusMgr::RefreshStatusCache() {
  return this->Get(false);
}

std::unique_ptr<Status> StatusMgr::GetStatus() {
  this->Get(false);
  return std::make_unique<Status>(datadir_, statusrec_);
}

void StatusMgr::AbortTransaction(std::unique_ptr<Status> pStatus) {
  status_file_.reset();
}

std::unique_ptr<Status> StatusMgr::BeginTransaction() {
  this->Get(true);
  return std::make_unique<Status>(datadir_, statusrec_);
}

bool StatusMgr::CommitTransaction(std::unique_ptr<Status> pStatus) {
  return this->Write(&pStatus->status_);
}

bool StatusMgr::Write(statusrec_t *pStatus) {
  if (!status_file_) {
    status_file_.reset(new File(FilePath(datadir_, STATUS_DAT)));
    status_file_->Open(File::modeReadWrite | File::modeBinary);
  } else {
    status_file_->Seek(0L, File::Whence::begin);
  }

  if (!status_file_->IsOpen()) {
    return false;
  }
  status_file_->Write(pStatus, sizeof(statusrec_t));
  status_file_.reset();
  return true;
}

int StatusMgr::GetUserCount() {
  return GetStatus()->GetNumUsers();
}

bool StatusMgr::Run(status_txn_fn fn) {
  auto status = BeginTransaction();
  CHECK_NOTNULL(status);
  fn(*status);
  return CommitTransaction(std::move(status));
}


}
