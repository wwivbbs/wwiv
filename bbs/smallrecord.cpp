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
#include <iostream>

#include "bbs/bbs.h"
#include "bbs/sysoplog.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "sdk/user.h"
#include "sdk/status.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "sdk/filenames.h"

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

// Inserts a record into NAMES.LST
void InsertSmallRecord(int user_number, const char *name) {
  WStatus *pStatus = session()->status_manager()->BeginTransaction();
  session()->names()->Add(name, user_number);
  session()->names()->Save();

  pStatus->IncrementNumUsers();
  pStatus->IncrementFileChangedFlag(WStatus::fileChangeNames);
  session()->status_manager()->CommitTransaction(pStatus);
}

// Deletes a record from NAMES.LST (DeleteSmallRec)
void DeleteSmallRecord(const char *name) {
  WStatus *pStatus = session()->status_manager()->BeginTransaction();
  int found_user = session()->names()->FindUser(name);
  if (found_user < 1) {
    session()->status_manager()->AbortTransaction(pStatus);
    sysoplogfi(false, "%s NOT ABLE TO BE DELETED#*#*#*#*#*#*#*#", name);
    sysoplog("#*#*#*# Run //resetf to fix it", false);
    return;
  }
  session()->names()->Remove(found_user);
  pStatus->DecrementNumUsers();
  pStatus->IncrementFileChangedFlag(WStatus::fileChangeNames);
  session()->names()->Save();
  session()->status_manager()->CommitTransaction(pStatus);
}
