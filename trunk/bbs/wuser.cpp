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
#include "core/wfile.h"
#include "wuser.h"
#include "core/wstringutils.h"
#include "filenames.h"
#include <iostream>
#include <memory>

#if defined( _WIN32 )
#define snprintf _snprintf
#endif // _WIN32

#ifndef NOT_BBS
#include "wsession.h"
#include "bbs.h"
#include "wconstants.h"
#include "vars.h"
#include "wstatus.h"
#endif // NOT_BBS



const int WUser::userDeleted                = 0x01;
const int WUser::userInactive               = 0x02;

// USERREC.exempt
const int WUser::exemptRatio                = 0x01;
const int WUser::exemptTime                 = 0x02;
const int WUser::exemptPost                 = 0x04;
const int WUser::exemptAll                  = 0x08;
const int WUser::exemptAutoDelete           = 0x10;

// USERREC.restrict
const int WUser::restrictLogon              = 0x0001;
const int WUser::restrictChat               = 0x0002;
const int WUser::restrictValidate           = 0x0004;
const int WUser::restrictAutomessage        = 0x0008;
const int WUser::restrictAnony              = 0x0010;
const int WUser::restrictPost               = 0x0020;
const int WUser::restrictEmail              = 0x0040;
const int WUser::restrictVote               = 0x0080;
const int WUser::restrictMultiNodeChat      = 0x0100;
const int WUser::restrictNet                = 0x0200;
const int WUser::restrictUpload             = 0x0400;

// USERREC.sysstatus
const int WUser::ansi                       = 0x00000001;
const int WUser::color                      = 0x00000002;
const int WUser::music                      = 0x00000004;
const int WUser::pauseOnPage                = 0x00000008;
const int WUser::expert                     = 0x00000010;
const int WUser::SMW                        = 0x00000020;
const int WUser::fullScreen                 = 0x00000040;
const int WUser::nscanFileSystem            = 0x00000080;
const int WUser::extraColor                 = 0x00000100;
const int WUser::clearScreen                = 0x00000200;
const int WUser::upperASCII                 = 0x00000400;
const int WUser::noTag                      = 0x00000800;
const int WUser::conference                 = 0x00001000;
const int WUser::noChat                     = 0x00002000;
const int WUser::noMsgs                     = 0x00004000;
const int WUser::menuSys                    = 0x00008000;
const int WUser::listPlus                   = 0x00010000;
const int WUser::autoQuote                  = 0x00020000;
const int WUser::twentyFourHourClock        = 0x00040000;
const int WUser::msgPriority                = 0x00080000;

extern unsigned char *translate_letters[];


WUser::WUser() {
  ZeroUserData();
}


WUser::~WUser() {
}


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
//
// class WUserManager
//
//
WUserManager::WUserManager(std::string dataDirectory, int nUserRecordLength, int nMaxNumberOfUsers) :
  m_dataDirectory(dataDirectory), m_nUserRecordLength(nUserRecordLength), m_nMaxNumberOfUsers(nMaxNumberOfUsers),
  m_bUserWritesAllowed(true), m_bInitalized(true) {
}

WUserManager::WUserManager() : m_bUserWritesAllowed(true), m_bInitalized(false) {
}


WUserManager::~WUserManager() {
}


void WUserManager::InitializeUserManager(std::string dataDirectory, int nUserRecordLength, int nMaxNumberOfUsers) {
  m_bInitalized = true;
  m_dataDirectory = dataDirectory;
  m_nUserRecordLength = nUserRecordLength;
  m_nMaxNumberOfUsers = nMaxNumberOfUsers;
}

int  WUserManager::GetNumberOfUserRecords() const {
  WFile userList(m_dataDirectory, USER_LST);
  if (userList.Open(WFile::modeReadOnly | WFile::modeBinary)) {
    long nSize = userList.GetLength();
    int nNumRecords = (static_cast<int>(nSize / m_nUserRecordLength) - 1);
    return nNumRecords;
  }
  return 0;
}


bool WUserManager::ReadUserNoCache(WUser *pUser, int nUserNumber) {
  WFile userList(m_dataDirectory, USER_LST);
  if (!userList.Open(WFile::modeReadOnly | WFile::modeBinary)) {
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
  userList.Seek(pos, WFile::seekBegin);
  userList.Read(&pUser->data,  m_nUserRecordLength);
  pUser->FixUp();
  return true;
}


bool WUserManager::ReadUser(WUser *pUser, int nUserNumber, bool bForceRead) {
#ifndef NOT_BBS
  if (!bForceRead) {
    bool userOnAndCurrentUser = (GetSession()->IsUserOnline() && (nUserNumber == GetSession()->usernum));
    int nWfcStatus = GetApplication()->GetWfcStatus();
    bool wfcStatusAndUserOne = (nWfcStatus && nUserNumber == 1);
    if (userOnAndCurrentUser || wfcStatusAndUserOne) {
      pUser->data = GetSession()->GetCurrentUser()->data;
      pUser->FixUp();
      return true;
    }
  }
#endif // NOT_BBS
  return this->ReadUserNoCache(pUser, nUserNumber);
}


bool WUserManager::WriteUserNoCache(WUser *pUser, int nUserNumber) {
  WFile userList(m_dataDirectory, USER_LST);
  if (userList.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile,
                    WFile::shareUnknown, WFile::permReadWrite)) {
    long pos = static_cast<long>(m_nUserRecordLength) * static_cast<long>(nUserNumber);
    userList.Seek(pos, WFile::seekBegin);
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
  if ((GetSession()->IsUserOnline() && nUserNumber == static_cast<int>(GetSession()->usernum)) ||
      (GetApplication()->GetWfcStatus() && nUserNumber == 1)) {
    if (&pUser->data != &GetSession()->GetCurrentUser()->data) {
      GetSession()->GetCurrentUser()->data = pUser->data;
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
  if (sr != NULL) {
    return sr->number;
  }
#else
  std::unique_ptr<WFile> usersFile(new WFile(m_dataDirectory, USER_LST));
  usersFile->Open(WFile::modeBinary | WFile::modeReadOnly);
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
  static char s_szNamBuffer[ 255 ];
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
    char szBuffer[ 10 ];
    snprintf(szBuffer, sizeof(szBuffer), " @%u", nSystemNumber);
    strcat(s_szNamBuffer, szBuffer);
  }
  return s_szNamBuffer;
}
