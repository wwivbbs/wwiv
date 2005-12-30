/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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

#include "wwiv.h"
#include "fix.h"
#include "log.h"
#include "users.h"


void checkUserList()
{
    WFile userFile(syscfg.datadir, USER_LST);
    if(!userFile.Exists())
    {
        Print(NOK, true, "%s does not exist.", userFile.GetFullPathName());
        giveUp();
    }

    WUserManager userMgr;
    Print(OK, true, "Checking USER.LST... found %d user records.", userMgr.GetNumberOfUserRecords());

    Print(OK, true, "TBD: Check for trashed user recs.");
    if(userMgr.GetNumberOfUserRecords() > syscfg.maxusers)
    {
        Print(OK, true, "Might be too many.");
	maybeGiveUp();
    }
    else
    {
        Print(OK, true, "Reasonable number.");
    }

    for(int i = 0; i < userMgr.GetNumberOfUserRecords(); i++)
    {
        WUser user;
	userMgr.ReadUser(&user, i);
	printf("User: '%s' '%s' %s\n", user.GetName(), user.GetPassword(), user.IsUserDeleted() ? "true" : "false");
	user.FixUp();
	userMgr.WriteUser(&user, i);
    }

    Print(OK, true, "Checking NAMES.LST");
    WFile nameFile(syscfg.datadir, NAMES_LST);
    if(!nameFile.Exists())
    {
        Print(NOK, true, "%s does not exist, regenerating", nameFile.GetFullPathName());
    }
    else
    {
        if(nameFile.Open(WFile::modeReadOnly | WFile::modeBinary))
	{
	    unsigned long size = nameFile.GetLength();
	    unsigned int recs = size / sizeof(smalrec);
	    if(recs != status.users)
	    {
	        status.users = recs;
		Print(NOK, true, "STATUS.DAT contained an incorrect user count.");
	    }
	    else
	    {
		Print(OK, true, "status.users = %d", status.users);
	    }
	}
	nameFile.Close();
    }
    
}
