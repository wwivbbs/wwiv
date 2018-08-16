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
#include <memory>
#include <string>

#include "sdk/status.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "core/datetime.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

namespace {
static statusrec_t statusrec;
}


static string GetSysopLogFileName(const string& d) {
  return StringPrintf("%c%c%c%c%c%c.log", d[6], d[7], d[0], d[1], d[3], d[4]);
}

WStatus::WStatus(const std::string& datadir, statusrec_t* pStatusRecord) : datadir_(datadir) {
  status_ = pStatusRecord;
}

WStatus::~WStatus() {};

std::string WStatus::GetLastDate(int days_ago) const {
  DCHECK_GE(days_ago, 0);
  DCHECK_LE(days_ago, 2);
  switch (days_ago) {
  case 0:
    return status_->date1;
  case 1:
    return status_->date2;
  case 2:
    return status_->date3;
  default:
    return status_->date1;
  }
}

std::string WStatus::GetLogFileName(int nDaysAgo) const {
  DCHECK_GE(nDaysAgo, 0);
  switch (nDaysAgo) {
  case 0:
  {
    return GetSysopLogFileName(daten_to_mmddyy(daten_t_now()));
  }
  case 1:
    return status_->log1;
  case 2:
    return status_->log2;
  default:
    return status_->log1;
  }
}

void WStatus::EnsureCallerNumberIsValid() {
  if (status_->callernum != 65535) {
    this->SetCallerNumber(status_->callernum);
    status_->callernum = 65535;
  }
}

void WStatus::ValidateAndFixDates() {
  if (status_->date1[8] != '\0') {
    status_->date1[6] = '0';
    status_->date1[7] = '0';
    status_->date1[8] = '\0'; // forgot to add null termination
  }

  string currentDate = daten_to_mmddyy(daten_t_now());
  if (status_->date3[8] != '\0') {
    status_->date3[6] = currentDate[6];
    status_->date3[7] = currentDate[7];
    status_->date3[8] = '\0';
  }
  if (status_->date2[8] != '\0') {
    status_->date2[6] = currentDate[6];
    status_->date2[7] = currentDate[7];
    status_->date2[8] = '\0';
  }
  if (status_->date1[8] != '\0') {
    status_->date1[6] = currentDate[6];
    status_->date1[7] = currentDate[7];
    status_->date1[8] = '\0';
  }
  if (status_->gfiledate[8] != '\0') {
    status_->gfiledate[6] = currentDate[6];
    status_->gfiledate[7] = currentDate[7];
    status_->gfiledate[8] = '\0';
  }
}

bool WStatus::NewDay() {
  status_->callstoday = 0;
  status_->msgposttoday = 0;
  status_->localposts = 0;
  status_->emailtoday = 0;
  status_->fbacktoday = 0;
  status_->uptoday = 0;
  status_->activetoday = 0;
  status_->days++;

  // Need to verify the dates aren't trashed otherwise we can crash here.
  ValidateAndFixDates();

  strcpy(status_->date3, status_->date2);
  strcpy(status_->date2, status_->date1);
  const string d = daten_to_mmddyy(daten_t_now());
  strcpy(status_->date1, d.c_str());
  strcpy(status_->log2, status_->log1);

  const string log = GetSysopLogFileName(GetLastDate(1));
  strcpy(status_->log1, log.c_str());
  return true;
}

// StatusMgr
bool StatusMgr::Get(bool bLockFile) {
  if (!status_file_) {
    status_file_.reset(new File(FilePath(datadir_, STATUS_DAT)));
    int nLockMode = (bLockFile) ? (File::modeReadWrite | File::modeBinary) : (File::modeReadOnly | File::modeBinary);
    status_file_->Open(nLockMode);
  } else {
    status_file_->Seek(0L, File::Whence::begin);
  }
  if (!status_file_->IsOpen()) {
    return false;
  } else {
    char oldFileChangeFlags[7];
    for (int nFcIndex = 0; nFcIndex < 7; nFcIndex++) {
      oldFileChangeFlags[nFcIndex] = statusrec.filechange[nFcIndex];
    }
    status_file_->Read(&statusrec, sizeof(statusrec_t));

    if (!bLockFile) {
      status_file_.reset();
    }

    for (int i = 0; i < 7; i++) {
      if (oldFileChangeFlags[i] != statusrec.filechange[i]) {
        // Invoke callback on changes.
        callback_(i);
      }
    }
  }
  return true;
}

bool StatusMgr::RefreshStatusCache() {
  return this->Get(false);
}

WStatus* StatusMgr::GetStatus() {
  this->Get(false);
  return new WStatus(datadir_, &statusrec);
}

void StatusMgr::AbortTransaction(WStatus* pStatus) {
  unique_ptr<WStatus> deleter(pStatus);
  status_file_.reset();
}

WStatus* StatusMgr::BeginTransaction() {
  this->Get(true);
  return new WStatus(datadir_, &statusrec);
}

bool StatusMgr::CommitTransaction(WStatus* pStatus) {
  unique_ptr<WStatus> deleter(pStatus);
  return this->Write(pStatus->status_);
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

const int StatusMgr::GetUserCount() {
  unique_ptr<WStatus>pStatus(GetStatus());
  return pStatus->GetNumUsers();
}

bool StatusMgr::Run(status_txn_fn fn) {
  WStatus* status = BeginTransaction();
  CHECK_NOTNULL(status);
  fn(*status);
  return CommitTransaction(status);
}


}
}
