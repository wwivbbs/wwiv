/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
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
#include "wwivinit.h"


// Stipped down version of what the BBS uses, this one will not create
// the status.dat since we know INIT already did it for us.


void InitStatusMgr::Get(bool bFailOnFailure, bool bLockFile)
{
    WFile file(syscfg.datadir, "status.dat");
    int mode = WFile::modeBinary;
    int shareMode = WFile::shareDenyNone;
    int perm = WFile::permRead;
    if (bLockFile) {
        mode |= WFile::modeReadWrite;
        shareMode = WFile::shareDenyReadWrite;
        perm = WFile::permReadWrite;
    } else {
        mode |= WFile::modeReadOnly;
    }
    if (!file.Open(mode, shareMode, perm)) {
        if (bFailOnFailure) {
		    Printf("%s NOT FOUND\r\n", file.GetFullPathName());
		    exit( 1 );
        }
	} else {
        file.Seek(0L, WFile::seekBegin);
    }
}

void InitStatusMgr::Lock()
{
	this->Get(true, true);
}

void InitStatusMgr::Read()
{
	this->Get(true, false);
}

void InitStatusMgr::Write()
{
    WFile file(syscfg.datadir, "status.dat");
    if (file.Open(WFile::modeBinary|WFile::modeReadWrite, WFile::shareDenyWrite, WFile::permReadWrite)) {
        file.Write(&status, sizeof(statusrec));
        file.Close();
	}
}


