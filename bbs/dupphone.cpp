/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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

#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "sdk/filenames.h"

void add_phone_number(int usernum, const char *phone) {
  if (strstr(phone, "000-")) {
    return;
  }

  File phoneFile(syscfg.datadir, PHONENUM_DAT);
  if (!phoneFile.Open(File::modeReadWrite | File::modeAppend | File::modeBinary | File::modeCreateFile)) {
    return;
  }

  phonerec p;
  p.usernum = static_cast<short>(usernum);
  strcpy(reinterpret_cast<char*>(p.phone), phone);
  phoneFile.Write(&p, sizeof(phonerec));
  phoneFile.Close();
}


void delete_phone_number(int usernum, const char *phone) {
  File phoneFile(syscfg.datadir, PHONENUM_DAT);
  if (!phoneFile.Open(File::modeReadWrite | File::modeBinary)) {
    return;
  }
  long lFileSize = phoneFile.GetLength();
  int nNumRecords = static_cast<int>(lFileSize / sizeof(phonerec));
  phonerec *p = static_cast<phonerec *>(BbsAllocA(lFileSize));
  WWIV_ASSERT(p);
  if (p == nullptr) {
    return;
  }
  phoneFile.Read(p, lFileSize);
  phoneFile.Close();
  int i;
  for (i = 0; i < nNumRecords; i++) {
    if (p[i].usernum == usernum &&
        wwiv::strings::IsEquals(reinterpret_cast<char*>(p[i].phone), phone)) {
      break;
    }
  }
  if (i < nNumRecords) {
    for (int i1 = i; i1 < nNumRecords; i1++) {
      p[i1] = p[i1 + 1];
    }
    --nNumRecords;
    phoneFile.Delete();
    phoneFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
    phoneFile.Write(p, static_cast<long>(nNumRecords * sizeof(phonerec)));
    phoneFile.Close();
  }
  free(p);
}


int find_phone_number(const char *phone) {
  File phoneFile(syscfg.datadir, PHONENUM_DAT);
  if (!phoneFile.Open(File::modeReadWrite | File::modeBinary)) {
    return 0;
  }
  long lFileSize = phoneFile.GetLength();
  int nNumRecords = static_cast<int>(lFileSize / sizeof(phonerec));
  phonerec *p = static_cast<phonerec *>(BbsAllocA(lFileSize));
  WWIV_ASSERT(p);
  if (p == nullptr) {
    return 0;
  }
  phoneFile.Read(p, lFileSize);
  phoneFile.Close();
  int i = 0;
  for (i = 0; i < nNumRecords; i++) {
    if (wwiv::strings::IsEquals(reinterpret_cast<char*>(p[i].phone), phone)) {
      WUser user;
      session()->users()->ReadUser(&user, p[i].usernum);
      if (!user.IsUserDeleted()) {
        break;
      }
    }
  }
  free(p);
  if (i < nNumRecords) {
    return p[i].usernum;
  }
  return 0;
}

