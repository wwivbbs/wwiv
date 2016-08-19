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
#include <memory>
#include <string>

#include "sdk/status.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"

statusrec_t statusrec;

using std::string;
using std::unique_ptr;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {


static string GetSysopLogFileName(const string& d) {
  return StringPrintf("%c%c%c%c%c%c.log", d[6], d[7], d[0], d[1], d[3], d[4]);
}

WStatus::WStatus(const std::string& datadir, statusrec_t* pStatusRecord) : datadir_(datadir) {
  m_pStatusRecord = pStatusRecord;
}

WStatus::WStatus(const std::string& datadir) {
  m_pStatusRecord = &statusrec;
}

WStatus::~WStatus() {};

const char* WStatus::GetLastDate(int nDaysAgo) const {
  DCHECK_GE(nDaysAgo, 0);
  DCHECK_GE(nDaysAgo, 2);
  switch (nDaysAgo) {
  case 0:
    return m_pStatusRecord->date1;
  case 1:
    return m_pStatusRecord->date2;
  case 2:
    return m_pStatusRecord->date3;
  default:
    return m_pStatusRecord->date1;
  }
}

const char* WStatus::GetLogFileName(int nDaysAgo) const {
  DCHECK_GE(nDaysAgo, 0);
  switch (nDaysAgo) {
  case 0:
  {
    static char s[81]; // logname
    std::string todays_log = GetSysopLogFileName(daten_to_date(time(nullptr)));
    strcpy(s, todays_log.c_str());
    return s;
  }
  case 1:
    return m_pStatusRecord->log1;
  case 2:
    return m_pStatusRecord->log2;
  default:
    return m_pStatusRecord->log1;
  }
}

void WStatus::EnsureCallerNumberIsValid() {
  if (m_pStatusRecord->callernum != 65535) {
    this->SetCallerNumber(m_pStatusRecord->callernum);
    m_pStatusRecord->callernum = 65535;
  }
}

void WStatus::ValidateAndFixDates() {
  if (m_pStatusRecord->date1[8] != '\0') {
    m_pStatusRecord->date1[6] = '0';
    m_pStatusRecord->date1[7] = '0';
    m_pStatusRecord->date1[8] = '\0'; // forgot to add null termination
  }

  string currentDate = daten_to_date(time(nullptr));
  if (m_pStatusRecord->date3[8] != '\0') {
    m_pStatusRecord->date3[6] = currentDate[6];
    m_pStatusRecord->date3[7] = currentDate[7];
    m_pStatusRecord->date3[8] = '\0';
  }
  if (m_pStatusRecord->date2[8] != '\0') {
    m_pStatusRecord->date2[6] = currentDate[6];
    m_pStatusRecord->date2[7] = currentDate[7];
    m_pStatusRecord->date2[8] = '\0';
  }
  if (m_pStatusRecord->date1[8] != '\0') {
    m_pStatusRecord->date1[6] = currentDate[6];
    m_pStatusRecord->date1[7] = currentDate[7];
    m_pStatusRecord->date1[8] = '\0';
  }
  if (m_pStatusRecord->gfiledate[8] != '\0') {
    m_pStatusRecord->gfiledate[6] = currentDate[6];
    m_pStatusRecord->gfiledate[7] = currentDate[7];
    m_pStatusRecord->gfiledate[8] = '\0';
  }
}

bool WStatus::NewDay() {
  m_pStatusRecord->callstoday = 0;
  m_pStatusRecord->msgposttoday = 0;
  m_pStatusRecord->localposts = 0;
  m_pStatusRecord->emailtoday = 0;
  m_pStatusRecord->fbacktoday = 0;
  m_pStatusRecord->uptoday = 0;
  m_pStatusRecord->activetoday = 0;
  m_pStatusRecord->days++;

  // Need to verify the dates aren't trashed otherwise we can crash here.
  ValidateAndFixDates();

  strcpy(m_pStatusRecord->date3, m_pStatusRecord->date2);
  strcpy(m_pStatusRecord->date2, m_pStatusRecord->date1);
  const string d = daten_to_date(time(nullptr));
  strcpy(m_pStatusRecord->date1, d.c_str());
  strcpy(m_pStatusRecord->log2, m_pStatusRecord->log1);

  const string log = GetSysopLogFileName(GetLastDate(1));
  strcpy(m_pStatusRecord->log1, log.c_str());
  return true;
}

// StatusMgr
bool StatusMgr::Get(bool bLockFile) {
  if (!m_statusFile.IsOpen()) {
    m_statusFile.SetName(datadir_, STATUS_DAT);
    int nLockMode = (bLockFile) ? (File::modeReadWrite | File::modeBinary) : (File::modeReadOnly | File::modeBinary);
    m_statusFile.Open(nLockMode);
  } else {
    m_statusFile.Seek(0L, File::seekBegin);
  }
  if (!m_statusFile.IsOpen()) {
    return false;
  } else {
    char oldFileChangeFlags[7];
    for (int nFcIndex = 0; nFcIndex < 7; nFcIndex++) {
      oldFileChangeFlags[nFcIndex] = statusrec.filechange[nFcIndex];
    }
    m_statusFile.Read(&statusrec, sizeof(statusrec_t));

    if (!bLockFile) {
      m_statusFile.Close();
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
  if (m_statusFile.IsOpen()) {
    m_statusFile.Close();
  }
}

WStatus* StatusMgr::BeginTransaction() {
  this->Get(true);
  return new WStatus(datadir_, &statusrec);
}

bool StatusMgr::CommitTransaction(WStatus* pStatus) {
  unique_ptr<WStatus> deleter(pStatus);
  return this->Write(pStatus->m_pStatusRecord);
}

bool StatusMgr::Write(statusrec_t *pStatus) {
  if (!m_statusFile.IsOpen()) {
    m_statusFile.SetName(datadir_, STATUS_DAT);
    m_statusFile.Open(File::modeReadWrite | File::modeBinary);
  } else {
    m_statusFile.Seek(0L, File::seekBegin);
  }

  if (!m_statusFile.IsOpen()) {
    return false;
  }
  m_statusFile.Write(pStatus, sizeof(statusrec_t));
  m_statusFile.Close();
  return true;
}

const int StatusMgr::GetUserCount() {
  unique_ptr<WStatus>pStatus(GetStatus());
  return pStatus->GetNumUsers();
}

}
}
