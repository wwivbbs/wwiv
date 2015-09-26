/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
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
#include <iostream>

#include "bbs/bbs.h"
#include "bbs/sysoplog.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "bbs/wuser.h"
#include "bbs/wstatus.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "sdk/filenames.h"

using namespace wwiv::strings;

// Inserts a record into NAMES.LST
void InsertSmallRecord(int nUserNumber, const char *pszName) {
  smalrec sr;
  int cp = 0;
  WStatus *pStatus = application()->GetStatusManager()->BeginTransaction();
  while (cp < pStatus->GetNumUsers() &&
         StringCompare(pszName, reinterpret_cast<char*>(smallist[cp].name)) > 0) {
    ++cp;
  }
  for (int i = pStatus->GetNumUsers(); i > cp; i--) {
    smallist[i] = smallist[i - 1];
  }
  strcpy(reinterpret_cast<char*>(sr.name), pszName);
  sr.number = static_cast<unsigned short>(nUserNumber);
  smallist[cp] = sr;
  File namesList(syscfg.datadir, NAMES_LST);
  if (!namesList.Open(File::modeReadWrite | File::modeBinary | File::modeTruncate)) {
    std::cerr << namesList.full_pathname() << " NOT FOUND" << std::endl;
    application()->AbortBBS();
  }
  pStatus->IncrementNumUsers();
  pStatus->IncrementFileChangedFlag(WStatus::fileChangeNames);
  namesList.Write(smallist, (sizeof(smalrec) * status.users));
  namesList.Close();
  application()->GetStatusManager()->CommitTransaction(pStatus);
}


//
// Deletes a record from NAMES.LST (DeleteSmallRec)
//

void DeleteSmallRecord(const char *pszName) {
  int cp = 0;
  WStatus *pStatus = application()->GetStatusManager()->BeginTransaction();
  while (cp < pStatus->GetNumUsers() && !wwiv::strings::IsEquals(pszName, reinterpret_cast<char*>(smallist[cp].name))) {
    ++cp;
  }
  if (!wwiv::strings::IsEquals(pszName, reinterpret_cast<char*>(smallist[cp].name))) {
    application()->GetStatusManager()->AbortTransaction(pStatus);
    sysoplogfi(false, "%s NOT ABLE TO BE DELETED#*#*#*#*#*#*#*#", pszName);
    sysoplog("#*#*#*# Run //resetf to fix it", false);
    return;
  }
  for (int i = cp; i < pStatus->GetNumUsers() - 1; i++) {
    smallist[i] = smallist[i + 1];
  }
  File namesList(syscfg.datadir, NAMES_LST);
  if (!namesList.Open(File::modeReadWrite | File::modeBinary | File::modeTruncate)) {
    std::cerr << namesList.full_pathname() << " COULD NOT BE CREATED" << std::endl;
    application()->AbortBBS();
  }
  --status.users;
  pStatus->IncrementFileChangedFlag(WStatus::fileChangeNames);
  namesList.Write(smallist, (sizeof(smalrec) * status.users));
  namesList.Close();
  application()->GetStatusManager()->CommitTransaction(pStatus);
}
