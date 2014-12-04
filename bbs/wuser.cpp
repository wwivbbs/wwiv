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
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <memory>
#include "sdk/filenames.h"
#include "bbs/wuser.h"
#include "core/strings.h"
#include "core/file.h"

#if defined( _WIN32 )
#define snprintf _snprintf
#endif // _WIN32

#ifndef NOT_BBS
#include "bbs.h"
#include "vars.h"
#include "wstatus.h"
#endif // NOT_BBS
extern unsigned char *translate_letters[];

WUser::WUser() {
  ZeroUserData();
}

WUser::~WUser() { }

WUser::WUser(const WUser& w) {
  memcpy(&data, &w.data, sizeof(userrec));
}

WUser& WUser::operator=(const WUser& rhs) {
  if (this == &rhs) {
    return *this;
  }
  memcpy(&data, &rhs.data, sizeof(userrec));
  return *this;
}

void WUser::FixUp() {
  data.name[sizeof(data.name) - 1]      = '\0';
  data.realname[sizeof(data.realname) - 1]  = '\0';
  data.callsign[sizeof(data.callsign) - 1]  = '\0';
  data.phone[sizeof(data.phone) - 1]      = '\0';
  data.dataphone[sizeof(data.dataphone) - 1]  = '\0';
  data.street[sizeof(data.street) - 1]    = '\0';
  data.city[sizeof(data.city) - 1]      = '\0';
  data.state[sizeof(data.state) - 1]      = '\0';
  data.country[sizeof(data.country) - 1]    = '\0';
  data.zipcode[sizeof(data.zipcode) - 1]    = '\0';
  data.pw[sizeof(data.pw) - 1]        = '\0';
  data.laston[sizeof(data.laston) - 1]    = '\0';
  data.firston[sizeof(data.firston) - 1]    = '\0';
  data.firston[2]                 = '/';
  data.firston[5]                 = '/';
  data.note[sizeof(data.note) - 1]      = '\0';
  data.macros[0][sizeof(data.macros[0]) - 1]  = '\0';
  data.macros[1][sizeof(data.macros[1]) - 1]  = '\0';
  data.macros[2][sizeof(data.macros[2]) - 1]  = '\0';
}

void WUser::ZeroUserData() {
  memset(&data, 0, sizeof(userrec));
}

const char *WUser::GetUserNameAndNumber(int nUserNumber) const {
  return nam(nUserNumber);
}

const char *WUser::GetUserNameNumberAndSystem(int nUserNumber, int nSystemNumber) const {
  return nam1(nUserNumber, nSystemNumber);
}

/////////////////////////////////////////////////////////////////////////////
// class WUserManager

WUserManager::WUserManager(std::string dataDirectory, int nUserRecordLength, int nMaxNumberOfUsers) :
  m_dataDirectory(dataDirectory), m_nUserRecordLength(nUserRecordLength), m_nMaxNumberOfUsers(nMaxNumberOfUsers),
  m_bUserWritesAllowed(true), m_bInitalized(true) {
}

WUserManager::WUserManager() : m_bUserWritesAllowed(true), m_bInitalized(false) { }


WUserManager::~WUserManager() { }

void WUserManager::InitializeUserManager(std::string dataDirectory, int nUserRecordLength, int nMaxNumberOfUsers) {
  m_bInitalized = true;
  m_dataDirectory = dataDirectory;
  m_nUserRecordLength = nUserRecordLength;
  m_nMaxNumberOfUsers = nMaxNumberOfUsers;
}

int  WUserManager::GetNumberOfUserRecords() const {
  File userList(m_dataDirectory, USER_LST);
  if (userList.Open(File::modeReadOnly | File::modeBinary)) {
    long nSize = userList.GetLength();
    int nNumRecords = (static_cast<int>(nSize / m_nUserRecordLength) - 1);
    return nNumRecords;
  }
  return 0;
}

bool WUserManager::ReadUserNoCache(WUser *pUser, int nUserNumber) {
  File userList(m_dataDirectory, USER_LST);
  if (!userList.Open(File::modeReadOnly | File::modeBinary)) {
    pUser->data.inact = inact_deleted;
    pUser->FixUp();
    return false;
  }
  long nSize = userList.GetLength();
  int nNumUserRecords = (static_cast<int>(nSize / m_nUserRecordLength) - 1);

  if (nUserNumber > nNumUserRecords) {
    pUser->data.inact = inact_deleted;
    pUser->FixUp();
    return false;
  }
  long pos = static_cast<long>(m_nUserRecordLength) * static_cast<long>(nUserNumber);
  userList.Seek(pos, File::seekBegin);
  userList.Read(&pUser->data,  m_nUserRecordLength);
  pUser->FixUp();
  return true;
}

bool WUserManager::ReadUser(WUser *pUser, int nUserNumber, bool bForceRead) {
#ifndef NOT_BBS
  if (!bForceRead) {
    bool userOnAndCurrentUser = (session()->IsUserOnline() && (nUserNumber == session()->usernum));
    int nWfcStatus = GetApplication()->GetWfcStatus();
    bool wfcStatusAndUserOne = (nWfcStatus && nUserNumber == 1);
    if (userOnAndCurrentUser || wfcStatusAndUserOne) {
      pUser->data = session()->user()->data;
      pUser->FixUp();
      return true;
    }
  }
#endif // NOT_BBS
  return this->ReadUserNoCache(pUser, nUserNumber);
}

bool WUserManager::WriteUserNoCache(WUser *pUser, int nUserNumber) {
  File userList(m_dataDirectory, USER_LST);
  if (userList.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    long pos = static_cast<long>(m_nUserRecordLength) * static_cast<long>(nUserNumber);
    userList.Seek(pos, File::seekBegin);
    userList.Write(&pUser->data,  m_nUserRecordLength);
    return true;
  }
  return false;
}

bool WUserManager::WriteUser(WUser *pUser, int nUserNumber) {
  if (nUserNumber < 1 || nUserNumber > m_nMaxNumberOfUsers || !IsUserWritesAllowed()) {
    return true;
  }

#ifndef NOT_BBS
  if ((session()->IsUserOnline() && nUserNumber == static_cast<int>(session()->usernum)) ||
      (GetApplication()->GetWfcStatus() && nUserNumber == 1)) {
    if (&pUser->data != &session()->user()->data) {
      session()->user()->data = pUser->data;
    }
  }
#endif // NOT_BBS
  return this->WriteUserNoCache(pUser, nUserNumber);
}

int WUserManager::FindUser(std::string searchString) {
#ifndef NOT_BBS
  smalrec *sr = (smalrec *)bsearch((const void *)searchString.c_str(),
                                   static_cast<const void *>(smallist),
                                   static_cast<size_t>(GetApplication()->GetStatusManager()->GetUserCount()),
                                   sizeof(smalrec),
                                   (int(*)(const void *, const void *)) wwiv::strings::StringCompareIgnoreCase);
  if (sr != nullptr) {
    return sr->number;
  }
#else
  std::unique_ptr<File> usersFile(new File(m_dataDirectory, USER_LST));
  usersFile->Open(File::modeBinary | File::modeReadOnly);
  smalrec userRec;
  if (usersFile->IsOpen()) {
    bool done = false;
    while (!done) {
      int bytesRead = usersFile->Read(&userRec, sizeof(smalrec));
      if (bytesRead != sizeof(smalrec)) {
        done = true;
      } else if (!wwiv::strings::StringCompareIgnoreCase(searchString.c_str(), (const char *)userRec.name)) {
        return (userRec.number);
      }
    }
  }
#endif // NOT_BBS
  return 0;
}

char *WUser::nam(int nUserNumber) const {
  static char s_szNamBuffer[255];
  bool f = true;
  unsigned int p = 0;
  for (p = 0; p < strlen(this->GetName()); p++) {
    if (f) {
      unsigned char* ss = reinterpret_cast<unsigned char*>(strchr(reinterpret_cast<char*>(translate_letters[ 1 ]),
                          data.name[ p ]));
      if (ss) {
        f = false;
      }
      s_szNamBuffer[ p ] = data.name[ p ];
    } else {
      char* ss = strchr(reinterpret_cast<char*>(translate_letters[ 1 ]), data.name[ p ]);
      if (ss) {
        s_szNamBuffer[ p ] = locase(data.name[ p ]);
      } else {
        if ((data.name[ p ] >= ' ' && data.name[ p ] <= '/') && data.name[ p ] != 39) {
          f = true;
        }
        s_szNamBuffer[ p ] = data.name[ p ];
      }
    }
  }
  s_szNamBuffer[ p++ ] = ' ';
  s_szNamBuffer[ p++ ] = '#';
  snprintf(&s_szNamBuffer[p], sizeof(s_szNamBuffer) - p, "%d", nUserNumber);
  return s_szNamBuffer;
}

char *WUser::nam1(int nUserNumber, int nSystemNumber) const {
  static char s_szNamBuffer[ 255 ];

  strcpy(s_szNamBuffer, nam(nUserNumber));
  if (nSystemNumber) {
    char szBuffer[10];
    snprintf(szBuffer, sizeof(szBuffer), " @%u", nSystemNumber);
    strcat(s_szNamBuffer, szBuffer);
  }
  return s_szNamBuffer;
}
