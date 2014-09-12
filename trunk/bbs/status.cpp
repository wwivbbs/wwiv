/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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

#include "wwiv.h"

statusrec status;

using std::unique_ptr;

// WStatus
const int WStatus::fileChangeNames = 0;
const int WStatus::fileChangeUpload = 1;
const int WStatus::fileChangePosts = 2;
const int WStatus::fileChangeEmail = 3;
const int WStatus::fileChangeNet = 4;

const char* WStatus::GetLastDate(int nDaysAgo) const {
  WWIV_ASSERT(nDaysAgo >= 0);
  WWIV_ASSERT(nDaysAgo <= 2);
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
  WWIV_ASSERT(nDaysAgo >= 0);
  WWIV_ASSERT(nDaysAgo <= 1);
  switch (nDaysAgo) {
  case 0:
    return m_pStatusRecord->log1;
  case 1:
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

  std::string currentDate = date();
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
  m_pStatusRecord->callstoday   = 0;
  m_pStatusRecord->msgposttoday = 0;
  m_pStatusRecord->localposts   = 0;
  m_pStatusRecord->emailtoday   = 0;
  m_pStatusRecord->fbacktoday   = 0;
  m_pStatusRecord->uptoday      = 0;
  m_pStatusRecord->activetoday  = 0;
  m_pStatusRecord->days++;

  // Need to verify the dates aren't trashed otherwise we can crash here.
  ValidateAndFixDates();

  strcpy(m_pStatusRecord->date3, m_pStatusRecord->date2);
  strcpy(m_pStatusRecord->date2, m_pStatusRecord->date1);
  strcpy(m_pStatusRecord->date1, date());
  strcpy(m_pStatusRecord->log2, m_pStatusRecord->log1);

  GetSysopLogFileName(GetLastDate(1), m_pStatusRecord->log1);
  return true;
}

// StatusMgr
bool StatusMgr::Get(bool bLockFile) {
  if (!m_statusFile.IsOpen()) {
    m_statusFile.SetName(syscfg.datadir, STATUS_DAT);
    int nLockMode = (bLockFile) ? (WFile::modeReadWrite | WFile::modeBinary) : (WFile::modeReadOnly | WFile::modeBinary);
    m_statusFile.Open(nLockMode);
  } else {
    m_statusFile.Seek(0L, WFile::seekBegin);
  }
  if (!m_statusFile.IsOpen()) {
    sysoplog("CANNOT READ STATUS");
    return false;
  } else {
    char oldFileChangeFlags[7];
    uint32_t lQScanPtr = status.qscanptr;
    for (int nFcIndex = 0; nFcIndex < 7; nFcIndex++) {
      oldFileChangeFlags[nFcIndex] = status.filechange[nFcIndex];
    }
    m_statusFile.Read(&status, sizeof(statusrec));

    if (!bLockFile) {
      m_statusFile.Close();
    }

    if (lQScanPtr != status.qscanptr) {
      if (GetSession()->m_SubDateCache) {
        // kill subs cache
        for (int i1 = 0; i1 < GetSession()->num_subs; i1++) {
          GetSession()->m_SubDateCache[i1] = 0L;
        }
      }
      GetSession()->SetMessageAreaCacheNumber(0);
      GetSession()->subchg = 1;
      //g_szMessageGatFileName[0] = 0;
    }
    for (int i = 0; i < 7; i++) {
      if (oldFileChangeFlags[i] != status.filechange[i]) {
        switch (i) {
        case WStatus::fileChangeNames:            // re-read names.lst
          if (smallist) {
            WFile namesFile(syscfg.datadir, NAMES_LST);
            if (namesFile.Open(WFile::modeBinary | WFile::modeReadOnly)) {
              namesFile.Read(smallist, (sizeof(smalrec) * status.users));
              namesFile.Close();
            }
          }
          break;
        case WStatus::fileChangeUpload: {         // kill dirs cache
          if (GetSession()->m_DirectoryDateCache) {
            for (int i1 = 0; i1 < GetSession()->num_dirs; i1++) {
              GetSession()->m_DirectoryDateCache[i1] = 0L;
            }
          }
          GetSession()->SetFileAreaCacheNumber(0);
        }
        break;
        case WStatus::fileChangePosts:
          GetSession()->subchg = 1;
          //g_szMessageGatFileName[0] = 0;
          break;
        case WStatus::fileChangeEmail:
          emchg = true;
          mailcheck = false;
          break;
        case WStatus::fileChangeNet: {
          int nOldNetNum = GetSession()->GetNetworkNumber();
          zap_bbs_list();
          for (int i1 = 0; i1 < GetSession()->GetMaxNetworkNumber(); i1++) {
            set_net_num(i1);
            zap_call_out_list();
            zap_contacts();
          }
          set_net_num(nOldNetNum);
        }
        break;
        }
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
  return new WStatus(&status);
}

void StatusMgr::AbortTransaction(WStatus* pStatus) {
  if (m_statusFile.IsOpen()) {
    m_statusFile.Close();
  }
  delete pStatus;
}

WStatus* StatusMgr::BeginTransaction() {
  this->Get(true);
  return new WStatus(&status);
}

bool StatusMgr::CommitTransaction(WStatus* pStatus) {
  unique_ptr<WStatus>(pStatus);
  return this->Write(pStatus->m_pStatusRecord);
}

bool StatusMgr::Write(statusrec *pStatus) {
  if (!m_statusFile.IsOpen()) {
    m_statusFile.SetName(syscfg.datadir, STATUS_DAT);
    m_statusFile.Open(WFile::modeReadWrite | WFile::modeBinary);
  } else {
    m_statusFile.Seek(0L, WFile::seekBegin);
  }

  if (!m_statusFile.IsOpen()) {
    sysoplog("CANNOT SAVE STATUS");
    return false;
  }
  m_statusFile.Write(pStatus, sizeof(statusrec));
  m_statusFile.Close();
  return true;
}

const int StatusMgr::GetUserCount() {
  unique_ptr<WStatus>pStatus(GetStatus());
  return pStatus->GetNumUsers();
}
